/**
 * @file pipe_fd.c
 * @brief Implementazione delle funzioni dichiarate in pipe_fd.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <stdlib.h>
#include <unistd.h>

#include <pipe_fd.h>
#include <connections.h>
#include <error_handler.h>


/* ------------------- funzioni di utilita' -------------------- */

/**
 * @function lock_pipe 
 * @brief prende la lock della pipe
 * 
 * @param pipe puntatore al gestore della pipe
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
static int lock_pipe(pipe_fd_t *pipe_fd){ 
    return pthread_mutex_lock(&(pipe_fd->mtx));
}


/**
 * @function unlock_pipe
 * @brief rilascia la lock della pipe
 * 
 * @param pipe puntatore al gestore della pipe
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
static int unlock_pipe(pipe_fd_t *pipe_fd){ 
    return pthread_mutex_unlock(&(pipe_fd->mtx));
}



/* --------------- interfaccia per la gestione della pipe ---------------- */

/**
 * @function init_pipe_fd
 * @brief Inizializza il gestore della pipe
 * 
 * @return p puntatore al gestore della pipe, NULL in caso di fallimento
 */
pipe_fd_t *init_pipe_fd(){
    //alloco la memoria per pipe
    pipe_fd_t *pipe_fd = malloc(sizeof(pipe_fd_t));
    err_return_msg(pipe_fd,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri della lista
    int fd[2];
    if (pipe(fd) == -1) {
        fprintf(stderr,"Errore: creazione pipe\n");
        free(pipe_fd);
        return NULL;
    }
    pipe_fd->pipe_read  = fd[0];
    pipe_fd->pipe_write = fd[1];
    pthread_mutex_init(&(pipe_fd->mtx),NULL);

    return pipe_fd;
}


/**
 * @function clean_pipe_fd
 * @brief Cancella la memoria allocata per il gestore della pipe e chiude i descrittori della pipe
 * 
 * @param pipe puntatore al gestore della pipe
 */
void clean_pipe_fd(pipe_fd_t *pipe_fd){
    if(pipe_fd == NULL) return;

    //chiudo i descrittori della pipe
    close(pipe_fd->pipe_read);
    close(pipe_fd->pipe_write);

    //libero la memoria del gestore della pipe
    free(pipe_fd);
}


/**
 * @function write_pipe
 * @brief Scrive sulla pipe passata un fd e l'operazione relativa ad esso
 * 
 * @param pipe puntatore al gestore della pipe
 * @param fd   fd da comunicare 
 * @param op   operazione relativa all'fd 
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int write_pipe(pipe_fd_t *pipe_fd, int fd, op_pipe_t op){
    //controllo gli argomenti 
    err_check_return(pipe_fd == NULL, EINVAL, "write_pipe", -1);

    //variabile di ritorno della funzione
    int ret = 0;

    errno = 0;
    int checklock = lock_pipe(pipe_fd);
    err_check_return(checklock != 0, checklock, "lock_pipe", -1);
    //scrivo l'fd
    if (writen(pipe_fd->pipe_write, &fd , sizeof(int)) < 0) {
        fprintf(stderr,"Errore scrittura fd nella pipe");
        ret = -1;
    }
    else {
        //scrivo l'operazione
        if (writen(pipe_fd->pipe_write, &op , sizeof(op_pipe_t)) < 0) {
            fprintf(stderr,"Errore scrittura op nella pipe");
            ret = -1;
        }
    }
    checklock = unlock_pipe(pipe_fd);
    err_check_return(checklock != 0, checklock, "unlock_pipe", -1);

    return ret;
}


/**
 * @function read_pipe
 * @brief Legge dalla pipe l'fd e l'operazione relativa ad esso
 * 
 * @param pipe puntatore al gestore della pipe
 * @param fd   puntatore su cui scrivere l'fd letto nella pipe
 * @param op   puntatore su cui scrivere l'operazione letta nella pipe
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int read_pipe(pipe_fd_t *pipe_fd, int *fd, op_pipe_t *op){
    //controllo gli argomenti 
    err_check_return(pipe_fd == NULL, EINVAL, "read_pipe", -1);
    err_check_return(fd == NULL, EINVAL, "read_pipe", -1);
    err_check_return(op == NULL, EINVAL, "read_pipe", -1);

    //variabile di ritorno della funzione
    int ret = 0;

    errno = 0;
    int checklock = lock_pipe(pipe_fd);
    err_check_return(checklock != 0, checklock, "lock_pipe", -1);

    //leggo l'fd
    if (readn(pipe_fd->pipe_read, fd, sizeof(int)) <= 0) {
        fprintf(stderr,"Errore lettura op nella pipe");
        ret = -1;
    }
    else {
        //leggo l'operazione
        if (readn(pipe_fd->pipe_read, op, sizeof(op_pipe_t)) <= 0) {
            fprintf(stderr,"Errore lettura op nella pipe");
            ret = -1;
        }
    }
    checklock = unlock_pipe(pipe_fd);
    err_check_return(checklock != 0, checklock, "unlock_pipe", -1);

    return ret;
}