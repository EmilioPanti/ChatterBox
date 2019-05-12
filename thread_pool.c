/**
 * @file thread_pool.c
 * @brief Implementazione delle funzioni dichiarate in thread_pool.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <thread_pool.h>
#include <error_handler.h>
#include <ops.h>
#include <group.h>


//configurazioni del server (definita in chatty.c)
extern configs_t conf_server;

//funzioni necessarie per la creazione della tabella hash degli utenti
//registrati e della lista degli utenti online (dichiarate in user.h)
extern void clean_user(void *us);
extern void setmutex_user(void *us, pthread_mutex_t *mutex);
extern int hashfun_user(int dim, void *name);
extern int cmp_user_by_name(void *us, void *name);
extern int cmp_user_by_fd(void *us, void *fd);

//funzioni necessarie per la creazione della tabella hash dei gruppi
//utenti (dichiarate in group.h)
extern void clean_group(void *gr);
extern void setmutex_group(void *gr, pthread_mutex_t *mutex);
extern int hashfun_group(int dim, void *name);
extern int cmp_group(void *gr, void *name);


/* ------------------------- interfaccia thread pool ---------------------------- */

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
hl_thread_pool_t* starts_thread_pool(fd_queue_t *fd_queue, pipe_fd_t *pipe_fd, pthread_t tid_sh){
    //controllo gli argomenti
    err_check_return(fd_queue == NULL, EINVAL, "starts_thread_pool", NULL);
    err_check_return(pipe_fd == NULL, EINVAL, "starts_thread_pool", NULL);

    hl_thread_pool_t *htp = malloc(sizeof(hl_thread_pool_t));
    err_return_msg(htp,NULL,NULL,"Errore: malloc\n");

    htp->n_threads    = 0;
    htp->th           = NULL;
    htp->thARGS       = NULL;
    htp->users_on     = NULL;
    htp->hash_users   = NULL;
    htp->hash_groups  = NULL;
    int check = pthread_mutex_init(&(htp->mtx_users_on),NULL);
    if (check != 0){
        errno = check;
        free(htp);
        perror("pthread_mutex_init");
        return NULL;
    }
    htp->fd_queue      = fd_queue;    //passata da parametro, da NON creare


    //prendo il numero di threads da creare nel pool
    int nth;
    if (conf_server.threads < MAXTHREADS) nth = conf_server.threads;
    else nth = MAXTHREADS;

    //creo il vettore dei thread ids
    htp->th = malloc(nth*sizeof(pthread_t));
    err_return_msg_clean(htp->th,NULL,NULL,"Errore: malloc\n",ends_thread_pool(htp));

    //creo il vettore degli argomenti per i threads
    htp->thARGS = malloc(nth*sizeof(args_worker_t));
    err_return_msg_clean(htp->thARGS,NULL,NULL,"Errore: malloc\n",ends_thread_pool(htp));

    //creo la lista degli utenti online
    htp->users_on = init_list(DEFAULT_LEN, &htp->mtx_users_on, NULL, cmp_user_by_fd);
    err_return_msg_clean(htp->users_on,NULL,NULL,"Errore: init_list\n",ends_thread_pool(htp));

    //creo la tabella hash degli utenti registrati
    //nota: passo come parametro il numero di max connessioni in modo che la tabella hash
    //      cresca/diminuisca di dimensione in base alle configurazioni date
    htp->hash_users = init_hashtable(conf_server.max_conn, 10, clean_user, cmp_user_by_name, hashfun_user, setmutex_user);
    err_return_msg_clean(htp->hash_users,NULL,NULL,"Errore: init_hashtable\n",ends_thread_pool(htp));

    //creo la tabella hash dei gruppi utente
    //nota: passo come parametro la metà di max connessioni in modo che la tabella hash
    //      cresca/diminuisca di dimensione in base alle configurazioni date
    int n_mtx_groups = 1;
    if (conf_server.max_conn > 1) n_mtx_groups = (int)(conf_server.max_conn / 2);
    htp->hash_groups = init_hashtable(n_mtx_groups, 10, clean_group, cmp_group, hashfun_group, setmutex_group);
    err_return_msg_clean(htp->hash_groups,NULL,NULL,"Errore: init_hashtable\n",ends_thread_pool(htp));

    //preparo i parametri per i threads
    for(int i=0; i<nth; i++) {
        (htp->thARGS)[i].tid       = i;
        (htp->thARGS)[i].fd_queue  = htp->fd_queue;
        (htp->thARGS)[i].us_on     = htp->users_on;
        (htp->thARGS)[i].hash_us   = htp->hash_users;
        (htp->thARGS)[i].hash_gr   = htp->hash_groups;
        (htp->thARGS)[i].pipe_fd   = pipe_fd;
        (htp->thARGS)[i].tid_sh    = tid_sh;
    }

    //creazione dei threads worker
    for(int i=0; i<nth; i++) {
        if (pthread_create(&(htp->th)[i], NULL, worker, &(htp->thARGS)[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            htp->n_threads = i;    //nel pool ci sono 'i' threads da terminare
            ends_thread_pool(htp);
            return NULL;
        }
    }

    htp->n_threads = conf_server.threads;

    return htp;
}


/**
 * @function ends_thread_pool
 * @brief Termina il thread pool e libera le risorse allocate per esso
 * 
 * @param htp gestore del thread pool
 * 
 * @note la terminazione dei threads avviene inserendo n volte '-1' nella coda degli fd, 
 *       i workers quando leggono tale valore terminano la loro esecuzione
 */
void ends_thread_pool(hl_thread_pool_t *htp){
    //controllo gli argomenti
    if (htp == NULL)  {
        errno = EINVAL;
        perror("ends_listener");
        return;
    }
    
    //prendo il numero di threads attivati
    int nth = htp->n_threads;

    if (nth != 0) {
        int i = 0, check = 0;
        long status = 0;

        //invio il messaggio di terminazione a tutti i threads
        for (i=0; i<nth; i++) {
            //se non riesco a inviare le richieste termino brutalmente
            if (push_fd(htp->fd_queue, -1) != 0) pthread_cancel((htp->th)[i]);
        }

        //aspetto la terminazione di tutti i threads
        for(i=0; i<nth; i++) {
            check = pthread_join((htp->th)[i],(void*) &status);

            //errore nel worker thread (chiama pthread_exit(errno) se ha avuto qualche errore)
            if(status != 0) {
                errno = status;
                perror("Errore worker thread");
            }
            //errore nella join
            else if(check != 0) fprintf(stderr,"Errore nella join del worker n° %d",i);
            else {
                #if defined(PRINT_STATUS)
                    fprintf(stdout,"Worker %d terminato con successo\n", i);
                #endif
            }
        }
    }

    if (htp->th != NULL) free(htp->th);
    if (htp->thARGS != NULL) free(htp->thARGS);
    if (htp->users_on != NULL) clean_list(htp->users_on);
    if (htp->hash_users != NULL) clean_hashtable(htp->hash_users);
    if (htp->hash_groups != NULL) clean_hashtable(htp->hash_groups);
    //la coda delle richieste è liberata (ed anche creata) in chatty.c

    free(htp);
}