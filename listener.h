/**
 * @file listener.h
 * @brief File contenente le funzioni e le struct usate dal thread listener, destinato all'ascolto
 *        di nuove connessioni da parte dei client e delle loro richieste
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef LISTENER_H_
#define LISTENER_H_

#include <pthread.h>
#include <fd_queue.h>
#include <pipe_fd.h>

//massimo numero connessioni pendenti nel listener socket
#define MAXBACKLOG   64


/**
 * @struct args_listener_t
 * @brief Argomenti necessari alla funzione listener
 * 
 * @var fd_queue    coda degli fd pronti ad inviare richieste al server
 * @var pipe_fd     gestore della pipe 
 * @var tid_sh      tid del signal handler
 */
typedef struct args_listener {
    fd_queue_t      *fd_queue;
    pipe_fd_t       *pipe_fd;
    pthread_t        tid_sh;
} args_listener_t;


/**
 * @struct args_listener_t
 * @brief Gestore del thread listener 
 * 
 * @var tid   identificatore del thread listener 
 * @var arg   argomenti della funzione listener
 */
typedef struct handler_listener {
    pthread_t       tid;
    args_listener_t arg;
} handler_listener_t;


/**
 * @function listener
 * @brief Funzione eseguita dal thread listener 
 * 
 * @param arg argomenti necessari al thread listener
 * 
 * @return 0 in caso di successo, altrimenti error
 */
void* listener(void* arg);


/**
 * @function starts_listener
 * @brief Fa partire il listener thread
 * 
 * @param fd_queue   coda degli fd pronti 
 * @param pipe_fd    gestore della pipe
 * @param tid_sh     tid del signal handler
 * 
 * @return gestore listener se successo, NULL in caso di errore (errno settato)
 */
handler_listener_t* starts_listener(fd_queue_t *fd_queue, pipe_fd_t *pipe_fd, pthread_t tid_sh);


/**
 * @function ends_listener
 * @brief Termina il listener thread
 * 
 * @param hl  gestore del listener thread
 * 
 * @note il thread listener capisce di dover terminare la propria esecuzione quando
 *       legge l'operazione TERMINATE nella pipe a cui è collegato
 */
void ends_listener(handler_listener_t *hl);


#endif /* LISTENER_H_ */