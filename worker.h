/**
 * @file worker.h
 * @brief File contenente le funzioni e le struct usate dal thread worker, destinato ad eseguire
 *        le richieste dei client
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef WORKER_H_
#define WORKER_H_

#include <pthread.h>
#include <fd_queue.h>
#include <pipe_fd.h>
#include <config.h>
#include <user.h>
#include <abs_hashtable.h>


/**
 * @struct request_t
 * @brief Struttura di una richiesta fatta da un client
 * 
 * @var fd          descrittore aperto verso il client 
 * @var msg         puntatore al messaggio della richiesta fatta dal client
 * @var data_file   puntatore alla pare data del file (solo quando l'op di msg è POSTFILE_OP)
 */
typedef struct request {
    long            fd;
    message_t       *msg;
    message_data_t  *data_file;
} request_t;


/**
 * @struct args_worker_t
 * @brief Argomenti necessari alla funzione worker
 * 
 * @var tid       identificatore del worker
 * @var fd_queue  coda per gli fd pronti a fare richieste al server
 * @var us_on     lista utenti online (condivisa solo tra i workers)
 * @var hash_us   tabella hash degli utenti registrati
 * @var hash_gr   tabella hash dei gruppi creati
 * @var pipe_fd   gestore della pipe per comunicare con il listener
 * @var tid_sh    tid del signal handler (per comunicargli eventualii errori)
 */
typedef struct args_worker {
    pthread_t     tid;
    fd_queue_t    *fd_queue;
    list_t        *us_on;
    hashtable_t   *hash_us;
    hashtable_t   *hash_gr;
    pipe_fd_t     *pipe_fd;
    pthread_t     tid_sh;
} args_worker_t;


/**
 * @function worker
 * @brief Funzione eseguita dal thread worker 
 * 
 * @param arg argomenti necessari al thread worker
 * 
 * @return 0 in caso di successo, altrimenti error
 */
void* worker(void* arg);


#endif /* WORKER_H_ */