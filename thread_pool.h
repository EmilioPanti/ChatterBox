/**
 * @file thread_pool.h
 * @brief File contenente l'interfaccia del thread pool, ha anche la funzione di gestire
 *        le strutture dati condivise tra i threads worker
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef THREAD_POOL_
#define THREAD_POOL_

#include <worker.h>
#include <config.h>


/**
 * @struct hl_thread_pool_t
 * @brief Gestore thread pool
 * 
 * @var n_threads      numero di thread all'interno del pool
 * @var th             vettore dei thread ids
 * @var thARGS         vettore degli argomenti per i threads
 * @var fd_queue       coda degli fd pronti a fare una richiesta al server (condivisa tra workers e listener)
 * @var users_on       lista utenti online (utilizzata solo dai workers)
 * @var mtx_users_on   mutex per la lista utenti online
 * @var hash_users     tabella hash degli utenti registrati
 * @var hash_groups    tabella hash dei gruppi utenti 
 * 
 * @note: la coda degli fd (fd_queue) è passata come parametro al costrutture         
 *        del thread pool, pertanto viene creata e distrutta altrove (chatty.c).
 *        La lista degli utenti online e le tabelle hash degli utenti e dei gruppi
 *        vengono invece completamente gestite dal thread pool, dato che sono strutture
 *        utilizzate esclusivamente dai threads worker
 */
typedef struct hl_thread_pool {
    int             n_threads;
    pthread_t       *th;
    args_worker_t   *thARGS;
    fd_queue_t      *fd_queue; 
    list_t          *users_on;
    pthread_mutex_t mtx_users_on;
    hashtable_t     *hash_users;
    hashtable_t     *hash_groups;
} hl_thread_pool_t;



/**
 * @function starts_thread_pool
 * @brief Fa partire la pool di threads e crea le strutture dati condivise tra i workers
 * 
 * @param fd_queue  coda degli fd pronti a fare una richiesta al server (condivisa con il listener)
 * @param pipe_fd   gestore della pipe per comunicare con il listener (da passare ai worker)
 * @param tid_sh    tid del signal handler (da passare ai worker)
 * 
 * @return gestore thread pool se successo, NULL in caso di errore (errno settato)
 */
hl_thread_pool_t* starts_thread_pool(fd_queue_t *fd_queue, pipe_fd_t *pipe_fd, pthread_t tid_sh);


/**
 * @function ends_thread_pool
 * @brief Termina il thread pool e libera le risorse allocate per esso
 * 
 * @param htp gestore del thread pool
 * 
 * @note la terminazione dei threads avviene inserendo n volte '-1' nella coda degli fd, 
 *       i workers quando leggono tale valore terminano la loro esecuzione
 */
void ends_thread_pool(hl_thread_pool_t *htp);


#endif /* THREAD_POOL_ */