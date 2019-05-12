/**
 * @file fd_queue.c
 * @brief File contenente l'implementazioni delle funzioni dichiarate in fd_queue.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <string.h>
#include <stdlib.h>
#include <error_handler.h>
#include <fd_queue.h>


/* ------------------- funzioni di utilita' -------------------- */

/**
 * @function lock_queue
 * @brief prende la lock della coda degli fd
 * 
 * @param q puntatore alla coda degli fd 
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore 
 */
static int lock_queue(fd_queue_t *q){ 
    return pthread_mutex_lock(&(q->mtx));
}


/**
 * @function unlock_queue
 * @brief rilascia la lock della coda degli fd
 * 
 * @param q puntatore alla coda degli fd
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore 
 */
static int unlock_queue(fd_queue_t *q){ 
    return pthread_mutex_unlock(&(q->mtx));
}


/**
 * @function unlock_queue
 * @brief rilascia la lock e si mette in attesa di una signal
 * 
 * @param q puntatore alla coda degli fd
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore 
 */
static int unlock_queue_and_wait(fd_queue_t *q){
    return pthread_cond_wait(&(q->cond), &(q->mtx));
}


/**
 * @function unlock_queue_and_signal
 * @brief manda una signal e rilascia la lock della coda degli fd
 * 
 * @param q puntatore alla coda degli fd
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore 
 */
static int unlock_queue_and_signal(fd_queue_t *q) {
    int check = pthread_cond_signal(&(q->cond));
    if (check != 0) return check;
    return pthread_mutex_unlock(&(q->mtx));
}


/* ----------------- interfaccia della coda richieste ------------------ */

/**
 * @function init_fd_queue
 * @brief Inizializza la coda degli fd pronti
 * 
 * @return q puntatore alla nuova coda, NULL in caso di fallimento
 */
fd_queue_t *init_fd_queue(){
    //alloco la memoria per la coda richieste
    fd_queue_t *q = malloc(sizeof(fd_queue_t));
    err_return_msg(q,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri della coda
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&(q->mtx),NULL);
    int check = pthread_cond_init(&(q->cond),NULL); //se 1 errore
    if(check == 1) {
        free(q);
        fprintf(stderr,"Errore: inizializzazione cond\n");
        return NULL;
    }

    return q;
}


/**
 * @function clean_fd_queue
 * @brief Cancella la coda degli fd pronti passata da parametro
 * 
 * @param q puntatore alla coda da cancellare
 */
void clean_fd_queue(fd_queue_t *q){
    if(q == NULL) return;

    //variabile di appoggio
    fd_ready_t *curr = NULL;

    while(q->head != q->tail) {
        curr = q->head;
    	q->head = q->head->next;
	    free(curr);
    }
    if (q->head) free(q->head);

    free(q);
}


/**
 * @function push_fd
 * @brief Inserisce un nuovo fd pronto nella coda
 * 
 * @param q    puntatore alla coda degli fd
 * @param fd   descrittore aperto verso il client
 * 
 * @return 0 se successo, -1 se errore
 */
int push_fd(fd_queue_t *q, long fd){
    //controllo gli argomenti
    err_check_return(q == NULL, EINVAL, "push_fd", -1);

    //creo un nuovo nodo per la coda
    fd_ready_t *new = malloc(sizeof(fd_ready_t));
    err_return_msg(new,NULL,-1,"Errore: malloc\n");

    new->fd = fd;
    new->next = NULL;

    //lock sulla coda richieste
    int check = lock_queue(q);
    err_check_return(check != 0, check, "lock_queue", -1);

    //se la coda è vuota
    if(q->head == NULL){
        q->head = new;
        q->tail = q->head;
    }
    //se la coda ha almeno un elemento
    else {
        q->tail->next = new;
        q->tail = new;
    }

    //unclock e signal 
    check = unlock_queue_and_signal(q);
    err_check_return(check != 0, check, "unlock_queue_and_signal", -1);

    return 0;
}


/**
 * @function pop_fd
 * @brief Preleva un fd dalla coda
 * 
 * @param q puntatore alla coda degli fd
 * 
 * @return fd pronto a fare una richiesta al server, 
 *         -1 ed errno settato in caso di errore
 */
long pop_fd(fd_queue_t *q){
    //controllo gli argomenti
    err_check_return(q == NULL, EINVAL, "pop_fd", -1);

    int check = lock_queue(q);
    err_check_return(check != 0, check, "lock_queue", -1);
    while(q->head == NULL) {
	    check = unlock_queue_and_wait(q);
        err_check_return(check != 0, check, "unlock_queue_and_wait", -1);
    }
    // locked
    fd_ready_t *node  = q->head;

    //se c'è solo un elemento
    if (q->head == q->tail) {
        q->head = NULL;
        q->tail = NULL;
    }
    //se ci sono più elementi
    else q->head = q->head->next;

    check = unlock_queue(q);
    err_check_return(check != 0, check, "unlock_queue", -1);

    long ret = node->fd;
    free(node);

    return ret;
}