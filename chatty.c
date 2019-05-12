/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

/* inserire gli altri include che servono */
#include <stats.h>
#include <config.h>
#include <error_handler.h>
#include <fd_queue.h>
#include <pipe_fd.h>
#include <listener.h>
#include <signal_handler.h>
#include <thread_pool.h>
#include <files_handler.h>

/* -------------------------- strutture dati globali --------------------------- */

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 */
struct statistics chattyStats = { 0,0,0,0,0,0,0 };
pthread_mutex_t mtx_stats = PTHREAD_MUTEX_INITIALIZER;

//configurazioni del server
configs_t conf_server = { NULL,0,0,0,0,0,NULL,NULL };


/* ---------------------- altre strutture dati utilizzate ----------------------- */

//coda degli fd printi ad inviare una richiesta al server (utilizzata sia dal listener che dai thread worker)
static fd_queue_t *fd_queue = NULL;   

// pipe utilizzata dai worker thread per comunicare con il listener thread.
// E' utilizzata anche per far terminare l'esecuzione del thread listener
static pipe_fd_t *pipe_fd = NULL;

//gestore del listener thread
static handler_listener_t *hl_listener = NULL;

//gestore thread pool
static hl_thread_pool_t   *thread_pool = NULL;


/* ---------------------------- funzioni di utilità ---------------------------- */

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}


/**
 * @function clean_all
 * @brief Libera la memoria occupata
 */
static void clean_all() {
    //termino i threads
    if(hl_listener != NULL) ends_listener(hl_listener);
    if(thread_pool != NULL) ends_thread_pool(thread_pool);

    //cancello tutti i file inviati dagli utenti
    clean_dirfile(conf_server.dir_name);

    //cancello le strutture dati
    clean_configs(&conf_server);
    if(fd_queue != NULL) clean_fd_queue(fd_queue);
    if(pipe_fd  != NULL) clean_pipe_fd(pipe_fd);

    #if defined(PRINT_STATUS)
        fprintf(stdout,"CLEAN ALL: chiamata e terminata\n");
    #endif
}



int main(int argc, char *argv[]) {
    if (argc!=3 || strncmp(argv[1],"-f",2)!=0) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    #if defined(PRINT_STATUS)
        fprintf(stdout,"  PRINT_STATUS definita\n");
    #endif

    //variabile di controllo
    int check;

   /* ------------------------- creazione strutture dati -------------------------- */

    //prendo le variabili di configurazione server dal file 
    check = configura_server(argv[2],&conf_server);
    err_exit(check,-1,clean_all());

    //creo la coda per gli fd 
    fd_queue = init_fd_queue();
    err_exit(fd_queue,NULL,clean_all());

    //creo la pipe per le comunicazioni extra tra workers e listener
    pipe_fd = init_pipe_fd();
    err_exit(pipe_fd,NULL,clean_all());


   /* ----------------------------- creazione threads ------------------------------- */

    //setto la maschera per i segnali da passare al signal_handler
    //e che viene ereditata dagli altri threads
    sigset_t set;
    sigemptyset(&set);        // resetto tutti i bits
    sigaddset(&set, SIGUSR1); // aggiunto SIGUSR1 --> per stampare le statistiche
    sigaddset(&set, SIGINT);  // aggiunto SIGINT  --> per terminare chatterbox
    sigaddset(&set, SIGTERM); // aggiunto SIGTERM --> per terminare chatterbox
    sigaddset(&set, SIGQUIT); // aggiunto SIGQUIT --> per terminare chatterbox
    sigaddset(&set, SIGPIPE); // aggiunto SIGPIPE --> da ignorare 
    sigaddset(&set, SIGUSR2); // aggiunto SIGUSR2 --> per terminare chatterbox 
    //NOTA su SIGUSR2: inviato dagli altri threads al signal_handler in caso di errore

    // blocco i segnali 
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
	    fprintf(stderr, "ERROR: pthread_sigmask\n");
        clean_all();
	    return (EXIT_FAILURE);
    }
    
    //avvio il signal_handler
    pthread_t tid_sh;
    args_sig_handler_t sh_args = { &set, clean_all };
    if (pthread_create(&tid_sh, NULL, signal_handler, &sh_args) != 0) {
	    fprintf(stderr, "pthread_create failed (signal_handler)\n");
        clean_all();
	    exit(EXIT_FAILURE);
    }

    //avvio il thradpool dei worker
    thread_pool = starts_thread_pool(fd_queue, pipe_fd, tid_sh);
    err_exit(thread_pool,NULL,clean_all());

    //avvio il listener
    hl_listener = starts_listener(fd_queue, pipe_fd, tid_sh);
    err_exit(hl_listener,NULL,clean_all());


   /* ------------------------------ terminazione chatterbox ------------------------------ */

    //aspetto la terminazione del signal handler
    long status = 0;
    check = pthread_join(tid_sh, (void*) &status);
    //errore nella join
    if(check != 0) {
        errno = check;
        perror("Errore thread signal_handler"); 
        clean_all(); 
        exit(EXIT_FAILURE);
    }
    //se terminato per errore (ha già ripulito tutto il signal_handler anche in caso di errore)
    else if(status != 0) exit(EXIT_FAILURE);
    
    #if defined(PRINT_STATUS)
        fprintf(stdout, "status SH = %ld\n", status);
    #endif

    return 0;
}
