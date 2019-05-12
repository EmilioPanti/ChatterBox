/**
 * @file listener.c
 * @brief Implementazione delle funzioni dichiarate in listener.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>      
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <signal.h>

#include <error_handler.h>
#include <connections.h>
#include <listener.h>



//configurazioni del server (definita in chatty.c)
extern configs_t conf_server;


/* --------------------------- funzioni di utilita' ------------------------------- */

/**
 * @function updatemax
 * @brief aggiorna il massimo id dei descrittori in set
 * 
 * @param set    dove sono registrati i fd da analizzare
 * @param fdmax  valore da aggiornare
 * 
 * @return fdmax aggiornato, -1 se set vuoto
 */
static int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	    if (FD_ISSET(i, &set)) return i;

    return -1;
}


/**
 * @function close_all_fd
 * @brief chiude tutti i fd presenti in set ad eccezione di quello destinato alla pipe
 *        (il descrittore della pipe è chiuso dalla funzione clean_all in chatty.c)
 * 
 * @param set      dove sono registrati i fd da chiudere
 * @param fdmax    fd di valore più alto presente in set
 * @param pipe_fd  descrittore della pipe (da non chiudere)
 */
static void close_all_fd(fd_set set, int fdmax, int pipe_fd) {
    for(int i=0; i <= fdmax; i++) {
	    if (FD_ISSET(i, &set) && (i != pipe_fd)) close(i);
    }
}


/**
 * @function quit_fun
 * @brief Invia un segnale al thread signal_handler (per comunicare che si deve
 *        terminare il processo) e chiama la pthread_exit()
 * 
 * @param sh_tid tid del thread segnal_handler
 * 
 * @note si presuppone che venga chiamata a seguito di un errore e che 
 *       quindi errno sia settato
 */
static void quit_fun(pthread_t sh_tid) {
    //se non riesco ad inviare il segnale al segnal_handler termino in maniera piu brutale
    if (pthread_kill(sh_tid, SIGUSR2) != 0) exit(EXIT_FAILURE);
    long ret = errno;
    pthread_exit((void *) ret);
}


/* ------------------------- interfaccia listener ---------------------------- */

/**
 * @function listener
 * @brief Funzione eseguita dal thread listener 
 * 
 * @param arg argomenti necessari al thread listener
 * 
 * @return 0 in caso di successo, altrimenti err
 */
void* listener(void* arg){
    //prendo gli argomenti passati alla funzione
    fd_queue_t *fd_queue  = ((args_listener_t*)arg)->fd_queue;
    pipe_fd_t  *pipe_fd   = ((args_listener_t*)arg)->pipe_fd;
    pthread_t tid_sh      = ((args_listener_t*)arg)->tid_sh;

    char *socket_name   = conf_server.socket_path;
    int max_conn        = conf_server.max_conn;
    
    //creo il socket per ascoltare nuove connessioni
    int listenfd = -1;
    listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenfd == -1) quit_fun(tid_sh);
    
    struct sockaddr_un serv_addr;
    serv_addr.sun_family = AF_UNIX;    
    strncpy(serv_addr.sun_path, socket_name, strlen(socket_name)+1);

    int check = 0;
    check = bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));
    if (check == -1) {
        close(listenfd);
        quit_fun(tid_sh);
    }
    check = listen(listenfd, MAXBACKLOG);
    if (check == -1) {
        close(listenfd);
        quit_fun(tid_sh);
    }
    
    fd_set set, tmpset;
    // azzero sia il master set che il set temporaneo usato per la select
    FD_ZERO(&set);
    FD_ZERO(&tmpset);

    // aggiungo il listener fd ed il descrittore in lettura della pipe al master set
    FD_SET(listenfd, &set);
    FD_SET(pipe_fd->pipe_read, &set);

    // tengo traccia del file descriptor con id piu' grande
    int fdmax;
    if(listenfd > pipe_fd->pipe_read) fdmax = listenfd;
    else fdmax = pipe_fd->pipe_read; 

    //numero di client connessi (non deve superare max_conn)
    int n_conn = 0;

    //ack da inviare al client se la connessione viene accettata
    int ack = 1;

    while(1) {      
	// copio il set nella variabile temporanea per la select
	tmpset = set;
	check = select(fdmax+1, &tmpset, NULL, NULL, NULL);
    if (check == -1) {
        close_all_fd(set, fdmax, pipe_fd->pipe_read);
        quit_fun(tid_sh);
    }

	// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
	for(int i=0; i <= fdmax; i++) {
	    if (FD_ISSET(i, &tmpset)) {
		    int connfd;

            //se e' una nuova richiesta di connessione
            if (i == listenfd) {
                connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL);
                if (connfd == -1) {
                    fprintf(stderr, "Errore: accept in listener\n");
                    close(connfd);
                    close_all_fd(set, fdmax, pipe_fd->pipe_read);
                    quit_fun(tid_sh);
                }
                
                //se non ho raggiunto max connessioni
                if(n_conn < max_conn) {
                    //invio l'ack per comunicare al client che ho accettato la connessione
                    errno = 0;
                    check = writen(connfd, &ack, sizeof(int));
                    //se il client si è disconnesso
                    if (check == -1 && errno == EPIPE) {
                        close(connfd);
                    }
                    //errore 
                    else if (check == -1 && errno != EPIPE) {
                        fprintf(stderr, "Errore: write ack in listener\n");
                        perror("Errno");
                        close(connfd);
                        close_all_fd(set, fdmax, pipe_fd->pipe_read);
                        quit_fun(tid_sh);
                    }
                    //se ok aggiungo l'fd al set e aggiorno le connessioni attuali
                    else {
                        FD_SET(connfd, &set); 
                        n_conn++;
                        if(connfd > fdmax) fdmax = connfd;
                    }
                }
                //se ho raggiunto max connessioni
                else {
                    close(connfd);
                }
            } 
            //se è una comunicazione sulla pipe
            else if (i == pipe_fd->pipe_read) {
                op_pipe_t op;

                //leggo la comunicazione dalla pipe
                check = read_pipe(pipe_fd, &connfd, &op);
                if (check != 0) {
                    close_all_fd(set, fdmax, pipe_fd->pipe_read);
                    quit_fun(tid_sh);
                }

                //in base all' operazione letta
                switch(op) {
                    //se è stata soddisfatta la richiesta del client
                    case UPDATE:
                        FD_SET(connfd, &set);
                        if(connfd > fdmax) fdmax = connfd;
                        break;
                    //se si è disconnesso il client durante l'esecuzione della sua richiesta
                    case CLOSE:
                        if (!FD_ISSET(connfd, &set)) {
                            n_conn--;
                            close(connfd);
                        }
                        break;
                    //se devo terminare il listener thread 
                    case TERMINATE:
                        //chiudo tutti i descrittori aperti
                        close_all_fd(set, fdmax, pipe_fd->pipe_read);
                        //termino l'esecuzione
                        pthread_exit((void *) 0);
                        break;
                    default:
                        ;
                    break;
                }
            }
            //se un client già connesso è pronto a fare una nuova richiesta
            else { 
                connfd = i;
                //rimuovo l'fd dal set in attesa che venga eseguita la sua richiesta da un worker
                FD_CLR(connfd, &set);
                //aggiorno fdmax
                if (connfd == fdmax) fdmax = updatemax(set, fdmax);
                //aggiungo l'fd del client alla coda degli fd pronti
                if (push_fd(fd_queue, connfd) == -1) {
                    close_all_fd(set, fdmax, pipe_fd->pipe_read);
                    quit_fun(tid_sh);
                }
            }
	    }
	}
    }
    return 0;
}


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
handler_listener_t* starts_listener(fd_queue_t *fd_queue, pipe_fd_t *pipe_fd, pthread_t tid_sh){
    //controllo gli argomenti
    err_check_return(fd_queue == NULL, EINVAL, "starts_listener", NULL);
    err_check_return(pipe_fd == NULL, EINVAL, "starts_listener", NULL);

    //creo il gestore listener
    handler_listener_t *hl = malloc(sizeof(handler_listener_t));
    err_return_msg(hl,NULL,NULL,"Errore: malloc\n");

    (hl->arg).fd_queue = fd_queue;
    (hl->arg).pipe_fd  = pipe_fd;
    (hl->arg).tid_sh   = tid_sh;

    //avvio il thread
    int check = pthread_create(&(hl->tid), NULL, listener, &(hl->arg));
    if (check != 0) {
        errno = check;
	    fprintf(stderr, "pthread_create listener fallita\n");
        free(hl);
        return NULL;
	}

    return hl;
}


/**
 * @function ends_listener
 * @brief Termina il listener thread
 * 
 * @param hl  gestore del listener thread
 * 
 * @note il thread listener capisce di dover terminare la propria esecuzione quando
 *       legge l'operazione TERMINATE nella pipe a cui è collegato
 */
void ends_listener(handler_listener_t *hl){
    //controllo gli argomenti
    if (hl == NULL)  {
        errno = EINVAL;
        perror("ends_listener");
        return;
    }
    
    long status = 0;
    int check = 0;

    //scrivo sulla pipe l'operazione di terminazione
    check = write_pipe(hl->arg.pipe_fd, -1, TERMINATE);
    if (check != 0) {
        //terminazione un po' più brutale se la write non ha funzionato
        fprintf(stderr,"Errore scrittura sulla pipe listener");
        pthread_cancel(hl->tid);
    }

    //aspetto la terminazione del thread listener
    check = pthread_join(hl->tid,(void*) &status);
    #if defined(PRINT_STATUS)
        fprintf(stdout, "status LISTENER = %ld\n", status);
    #endif
    //errore nel listener thread (chiama pthread_exit(errno) se ha avuto qualche errore)
    if(status != 0) {
        errno = status;
        perror("Errore thread listener");
    }
    //errore nella join
    if(check != 0) fprintf(stderr,"Errore nella join del listener");

    //libero la memoria allocata per hl
    free(hl);
}