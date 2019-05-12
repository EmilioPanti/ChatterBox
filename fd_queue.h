/**
 * @file fd_queue.h
 * @brief File per la gestione/creazione della coda degli fd (pronti a fare una richiesta al server)
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef FD_QUEUE_H_
#define FD_QUEUE_H_

#include <pthread.h>


/**
 * @struct fd_ready_t
 * @brief Nodo della coda per gli fd pronti a fare una richiesta al server
 * 
 * @var fd          descrittore aperto verso il client
 * @var next        puntatore al prossimo elemento
 */
typedef struct fd_ready {
    long            fd;
    struct fd_ready  *next;
} fd_ready_t;


/**
 * @struct fd_queue_t
 * @brief Gestore della coda degli fd dei client pronti a fare una richiesta
 * 
 * @var head  puntatore al primo fd pronto della coda
 * @var tail  puntatore all'ultimo fd pronto della coda
 * @var mtx   mutex per controllare l'accesso alla coda
 * @var cond  cv per eventi sulla coda
 */
typedef struct fd_queue {
    fd_ready_t        *head;
    fd_ready_t        *tail;
    pthread_mutex_t  mtx;
    pthread_cond_t   cond;
} fd_queue_t;


/**
 * @function init_fd_queue
 * @brief Inizializza la coda degli fd pronti
 * 
 * @return q puntatore alla nuova coda, NULL in caso di fallimento
 */
fd_queue_t *init_fd_queue();


/**
 * @function clean_fd_queue
 * @brief Cancella la coda degli fd pronti passata da parametro
 * 
 * @param q puntatore alla coda da cancellare
 */
void clean_fd_queue(fd_queue_t *q);


/**
 * @function push_fd
 * @brief Inserisce un nuovo fd pronto nella coda
 * 
 * @param q    puntatore alla coda degli fd
 * @param fd   descrittore aperto verso il client
 * 
 * @return 0 se successo, -1 se errore
 */
int push_fd(fd_queue_t *q, long fd);


/**
 * @function pop_fd
 * @brief Preleva un fd dalla coda
 * 
 * @param q puntatore alla coda degli fd
 * 
 * @return fd pronto a fare una richiesta al server, 
 *         -1 ed errno settato in caso di errore
 */
long pop_fd(fd_queue_t *q);


#endif /* FD_QUEUE_H_ */