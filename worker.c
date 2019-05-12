/**
 * @file worker.c
 * @brief Implementazione delle funzioni dichiarate in worker.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#define _POSIX_C_SOURCE 200809L
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>

#include <worker.h>
#include <files_handler.h>
#include <error_handler.h>
#include <stats.h>
#include <connections.h>
#include <group.h>


//configurazioni del server (definita in chatty.c)
extern configs_t conf_server;

//statistiche del server (definita in chatty.c)
extern struct statistics chattyStats;

//funzioni da applicare a liste o tabelle hash di
//utenti (dichiarate in user.h)
extern int get_listname(void *us, void *param);
extern int postmsg_all(void *us, void *param);


/* ---------------------- variabili globali nel file worker ------------------------- */

//numero del thread all'interno del thread pool
static pthread_t tid;

//coda richieste (condivisa con il thread listener e con gli altri workers)
static fd_queue_t *fd_queue;

//lista degli utenti online (condivisa tra i workers)
static list_t *us_on;

//tabella hash utenti registrati (condivisa tra i workers)
static hashtable_t  *hash_us;

//tabella hash per i gruppi utente (condivisa tra i workers)
static hashtable_t  *hash_gr;

//gestore della pipe per comunicare con il listener
static pipe_fd_t *pipe_fd;

//id del thread worker
static pthread_t tid_sh;


/* --------------------------- funzioni di utilita' ------------------------------- */

/**
 * @function quit_worker
 * @brief Invia un segnale al thread signal_handler (per comunicare che si deve
 *        terminare il processo) e chiama la pthread_exit()
 * 
 * @param sh_tid tid del thread segnal_handler
 * 
 * @note si presuppone che venga chiamata a seguito di un errore e che 
 *       quindi errno sia settato
 */
static void quit_worker(pthread_t sh_tid) {
    //se non riesco ad inviare il segnale al segnal_handler termino in maniera piu brutale
    if (pthread_kill(sh_tid, SIGUSR2) != 0) exit(EXIT_FAILURE);
    perror("quit_worker");
    long ret = errno;
    pthread_exit((void *) ret);
}


/**
 * @function free_request
 * @brief Libera la memoria allocata per la richiesta
 * 
 * @param r richiesta da cancellare
 */
static void free_request(request_t *r) {
    if (r->msg != NULL) free_msg(r->msg); //funzione implementata nel file message.h
    if (r->data_file != NULL) {
        if (r->data_file->buf != NULL) free(r->data_file->buf);
        free(r->data_file);
    }
    free(r);
}


/**
 * @function read_request
 * @brief legge una richiesta dal client
 * 
 * @param connfd  fd del client che manda la richiesta
 * @param req     puntatore alla richiesta da leggere
 * 
 * @return 0 se descrittore chiuso, -1 se errore (errno settato), 1 se ha successo.
 */
static int read_request(int connfd, request_t *req) {
    req->msg = NULL;
    req->data_file = NULL;

    message_t *msg = malloc(sizeof(message_t));
    if (msg == NULL) {
        free_request(req);
        fprintf(stderr, "Errore: malloc\n");
        return -1;
    }
    msg->data.buf = NULL;
    
    //leggo il messaggio del client
    int check = readMsg(connfd, msg);
    if(check <= 0) { //errore o connfd chiuso
        free_msg(msg);
        free_request(req);
        return check;
    }
    else {
        req->fd = connfd;
        req->msg = msg;
        //se devo leggere anche il file
        if (msg->hdr.op == POSTFILE_OP) {
            message_data_t *data_file = malloc(sizeof(message_data_t));
            if (data_file == NULL){
                free_request(req);
                fprintf(stderr, "Errore: malloc\n");
                return -1;
            }

            data_file->buf = NULL;
            check = readData(connfd, data_file);
            if(check<=0) { //errore o connfd chiuso
                free_request(req);
                free(data_file);
                return check;
            }

            req->data_file = data_file;
        }
    }

    return 1;
}


/**
 * @function disconnect_fun
 * @brief Si occupa di rimuovere l'utente dalla lista degli utenti online e di aggiornare
 *        le statitiche 
 * 
 * @param fd fd dell'utente disconnesso
 * 
 * @return 0 se successo, -1 in caso di errore e si deve terminare il server chatty
 */
static int disconnect_fun(long fd) {

    //rimuovo l'utente dalla lista online
    user_t *user = remove_data(us_on, (void*) &fd);

    if(user != NULL) {
        //aggiorno le statistiche
        int check = lock_stats();
        err_check_return(check != 0, check, "lock_stats", -1);
        chattyStats.nonline--;
        check = unlock_stats();
        err_check_return(check != 0, check, "unlock_stats", -1);
    }

    return 0;
}


/**
 * @function send_error
 * @brief Invia un messaggio di errore al client o all'utente (se passato da parametro)
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param user  struttura utente (se NULL vengono fatti meno controlli)
 * @param op    operazione di errore da inviare al client/utente
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int send_error(request_t *req, user_t *user, op_t op) {  
    //variabili di appoggio
    int check = 1;

    //setto il messaggio di errore
    setHeader(&req->msg->hdr, op, "");

    //se sto inviando il messaggio di errore ad un utente connesso
    if (user != NULL) check = sendHdr_toUser(user, &req->msg->hdr);
    //se l'utente non è passato da parametro invio il messaggio di
    //errore al client che aveva fatto la richiesta
    else check = sendHdr_toClient(req->fd, &req->msg->hdr);

    //controllo l'esito dell'invio del messaggio di errore
    //se errore
    if (check == -1) return -1;
    else if (check == 1) {
        //aggiorno le statistiche
        int checklock = lock_stats();
        err_check_return(checklock != 0, checklock, "lock_stats", -1);
        chattyStats.nerrors++;
        checklock = unlock_stats();
        err_check_return(checklock != 0, checklock, "unlock_stats", -1);
    }

    return 0;
}



/* ------------------------ funzioni per esguire le richieste dei client -------------------------- */


/**
 * @function usrlist_fun
 * @brief Invia la lista degli utenti online all' utente
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param user  utente a cui inviare la lista
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int usrlist_fun(request_t *req, user_t *user) {
    //creo la struttura per i parametri necessari alla funzione 
    //get_listname
    param_get_listname_t *prm = init_param_get_listname(10);
    err_return_msg(prm,NULL,-1,"Errore: init_param_get_listname\n");

    //prendo la lista degli utenti online
    int check = apply_fun_param(us_on, get_listname, (void*)prm);
    err_return_msg_clean(check, -1, -1, "Errore: get_listname\n", free(prm));

    //per risparmiare un po' di spazio
    prm->all_names = realloc(prm->all_names, sizeof(char)*(prm->len));
    err_return_msg_clean(prm->all_names, NULL, -1, "Errore: realloc\n", free(prm));
    
    //preparo il messaggio da inviare all'utente
    setHeader(&req->msg->hdr, OP_OK, "");
    setData(&req->msg->data, "", prm->all_names, prm->len);

    //invio il messaggio all'utente
    int sent = 0;
    if (sendMsg_toUser(user, req->msg, &sent) == -1) {
        free(prm);
        return -1;
    }

    free(prm);
    return 0;
}


/**
 * @function register_fun
 * @brief Si occupa della registrazione di un utente
 * 
 * @param req richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int register_fun(request_t *req) {
    errno = 0;
    //controllo che non esista un gruppo con tale nome
    group_t *gr = search_data_ht(hash_gr, (void*)req->msg->hdr.sender);
    if (gr != NULL) {
        //invio il messaggio di errore al client
        return send_error(req, NULL, OP_NICK_ALREADY);
    }
    //se errore
    else if (gr == NULL && errno != 0) return -1;

    //creo l'utente
    user_t *user = create_user(req->msg->hdr.sender, req->fd);
    err_return_msg(user,NULL,-1,"Errore: creazione utente\n");

    //provo ad aggiugerlo nella tabella hash
    int check = add_data_ht(hash_us, (void*)user, (void*)user->nickname);

    //se errore
    if (check == -1 && errno != 0) {
        clean_user(user);
        return -1;
    }
    //se esiste già un utente con tale nome
    else if (check == 0 && errno == 0) {
        clean_user(user);
        //invio il messaggio di errore al client
        return send_error(req, NULL, OP_NICK_ALREADY);
    }
    //se OK
    else {
        //aggiungo l'utente nella lista di quelli online
        if (add_data(us_on, (void*)user, (void*)&user->fd) == -1) return -1;

        //aggiorno le statistiche
        int checklock = lock_stats();
        err_check_return(checklock != 0, checklock, "lock_stats", -1);
        chattyStats.nusers++;
        chattyStats.nonline++;
        checklock = unlock_stats();
        err_check_return(checklock != 0, checklock, "unlock_stats", -1);

        //invio all'utente l'ack OK + la lista degli utenti online
        return usrlist_fun(req, user);
    }
}


/**
 * @function connect_fun
 * @brief Si occupa di mettere online l'utente
 * 
 * @param req richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int connect_fun(request_t *req) {
    errno = 0;
    user_t *user = search_data_ht(hash_us, (void*)req->msg->hdr.sender);

    //se errore 
    if (user == NULL && errno != 0) return -1;
    //se non esiste nessun utente con tale nome
    else if (user == NULL && errno == 0) {
        //invio il messaggio di errore al client
        return send_error(req, NULL, OP_NICK_UNKNOWN);
    }
    //se OK
    else {
        //setto online l'utente
        int check = set_online(user, req->fd);

        if(check == 1) {
            //aggiungo l'utente nella lista di quelli online
            if(add_data(us_on, (void*)user, (void*)&req->fd) == -1) return -1;

            //aggiorno le statistiche
            int checklock = lock_stats();
            err_check_return(checklock != 0, checklock, "lock_stats", -1);
            chattyStats.nonline++;
            checklock = unlock_stats();
            err_check_return(checklock != 0, checklock, "unlock_stats", -1);

            //invio all'utente l'ack OK + la list degli utenti online
            return usrlist_fun(req, user);
        }
        //se già online o disattivo
        else if (check == 0) {
            //comunico al client che tale utente è già online
            return send_error(req, NULL, OP_FAIL);
        }
        //se errore
        else return -1;
    }
}


/**
 * @function posttxt_fun
 * @brief Si occupa di inviare un messaggio testuale da un utente ad un altro oppure 
 *        ad un gruppo a cui l'utente appartiene 
 * 
 * @param req        richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param us_sender  utente che invia il messaggio testuale
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int posttxt_fun(request_t *req, user_t *us_sender) {
    errno = 0;
    //variabili di appoggio
    int check = 1, delivered = 0, same_user = 0;
   
    //se la size del messaggio è troppo grande
    if (req->msg->data.hdr.len > conf_server.max_msg_size) {
        return send_error(req, us_sender, OP_MSG_TOOLONG);
    }


    //cerco l'utente receiver o il gruppo destinatario
    user_t *us_receiver  = NULL;
    group_t *gr_receiver = NULL;
    check = strcmp(req->msg->hdr.sender, req->msg->data.hdr.receiver);
    //se sender e receiver sono lo stesso utente
    if(check == 0) {
        us_receiver = us_sender;
        same_user = 1;
    }
    //altrimenti cerco tra gli utenti
    else {
        us_receiver = search_data_ht(hash_us, (void*)req->msg->data.hdr.receiver);
        //se errore
        if (us_receiver == NULL && errno != 0) return -1;
        //se non esiste nessun utente con tale nome
        if (us_receiver == NULL && errno == 0) {
            //controllo se il destinatario è in realtà un gruppo a cui è iscritto l'utente
            gr_receiver = check_subscription(us_sender, req->msg->data.hdr.receiver);
            //se non è iscritto a tale gruppo mando il messaggio di errore
            if (gr_receiver == NULL && errno == 0) return send_error(req, us_sender, OP_NICK_UNKNOWN);
            //se errore
            if (gr_receiver == NULL && errno != 0) return -1;
        }
    }


    //creo il messaggio da inviare
    message_t *msg = duplicate_msg(req->msg, TXT_MESSAGE);
    err_return_msg(msg,NULL,-1,"Errore: duplicate_msg\n");


    //se il destinatario era un utente 
    if (us_receiver != NULL) {

        //invio il messaggio all'utente receiver
        check = sendMsg_toUser(us_receiver, msg, &delivered);
        //se c'è stato un errore
        if (check == -1) return -1;
        //se si è disconnesso
        else if (check == 0) {
            if (same_user == 1) return 0;
        }

        //aggiorno le statistiche
        int checklock = lock_stats();
        err_check_return(checklock != 0, checklock, "lock_stats", -1);
        if (delivered == 1) chattyStats.ndelivered++;
        else chattyStats.nnotdelivered++;
        checklock = unlock_stats();
        err_check_return(checklock != 0, checklock, "unlock_stats", -1);

    }
    //se invece il destinarario è un gruppo
    else {

        //creo la struttura che contiene i parametri per la funzione postmsg_all_group
        param_postmsg_all_t *prm = init_param_postmsg_all(msg);
        err_return_msg_clean(prm,NULL,-1,"Errore: init_param_postmsg_all\n",free_msg(msg));

        //invio il messaggio a tutti i membri del gruppo
        check = postmsg_all_group(gr_receiver, prm);
        //in caso di errore
        if (check == -1){
            fprintf(stderr, "Errore: postmasg_all_group\n");
            free_msg(msg);
            free(prm);
            return -1;
        }
        //se il gruppo è in fase di cancellazione lo considero inesistente
        else if (check == 0) {
            free_msg(msg);
            free(prm);
            return send_error(req, us_sender, OP_NICK_UNKNOWN);
        }

        //aggiorno le statistiche
        int checklock = lock_stats();
        if (checklock != 0) {free_msg(msg); free(prm);} 
        err_check_return(checklock != 0, checklock, "lock_stats", -1);
        chattyStats.ndelivered = chattyStats.ndelivered + prm->delivered;
        chattyStats.nnotdelivered = chattyStats.nnotdelivered + prm->notdelivered;
        checklock = unlock_stats();
        if (checklock != 0) {free_msg(msg); free(prm);} 
        err_check_return(checklock != 0, checklock, "unlock_stats", -1);

        free_msg(msg);
        free(prm);
    }
    

    //invio il messaggio di buon esito all'utente sender
    setHeader(&req->msg->hdr, OP_OK, "");
    if (sendHdr_toUser(us_sender, &req->msg->hdr) == -1) return -1;

    return 0;
}


/**
 * @function posttxtall_fun
 * @brief Si occupa di inviare un messaggio testuale a tutti gli utenti registrati
 * 
 * @param req        richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param us_sender  utente che invia il messaggio testuale
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int posttxtall_fun(request_t *req, user_t *us_sender) {
    //se la size del messaggio è troppo grande
    if (req->msg->data.hdr.len > conf_server.max_msg_size) {
        return send_error(req, us_sender, OP_MSG_TOOLONG);
    }

    //creo il messaggio da inviare
    message_t *msg = duplicate_msg(req->msg, TXT_MESSAGE);
    err_return_msg(msg,NULL,-1,"Errore: duplicate_msg\n");

    //creo la struttura che contiene i parametri per la funzione postmsg_all
    param_postmsg_all_t *prm = init_param_postmsg_all(msg);
    err_return_msg_clean(prm,NULL,-1,"Errore: init_param_postmsg_all\n",free_msg(msg));

    //invio il messaggio a tutta la tabella hash degli utenti
    if (apply_fun_param_ht(hash_us, postmsg_all, (void*)prm) == -1){
        fprintf(stderr, "Errore: postmsg_all\n");
        free_msg(msg);
        free(prm);
        return -1;
    }

    //aggiorno le statistiche
    int checklock = lock_stats();
    if (checklock != 0) {free_msg(msg); free(prm);} 
    err_check_return(checklock != 0, checklock, "lock_stats", -1);
    chattyStats.ndelivered = chattyStats.ndelivered + prm->delivered;
    chattyStats.nnotdelivered = chattyStats.nnotdelivered + prm->notdelivered;
    checklock = unlock_stats();
    if (checklock != 0) {free_msg(msg); free(prm);} 
    err_check_return(checklock != 0, checklock, "unlock_stats", -1);

    free_msg(msg);
    free(prm);

    //invio il messaggio di buon esito all'utente sender
    setHeader(&req->msg->hdr, OP_OK, "");
    if (sendHdr_toUser(us_sender, &req->msg->hdr) == -1) return -1;

    return 0;
}


/**
 * @function postfile_fun
 * @brief Si occupa di inviare un file da un utente ad un altro oppure ad un gruppo
 *        a cui è iscritto
 * 
 * @param req        richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param us_sender  utente che vuole inviare il file
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int postfile_fun(request_t *req, user_t *us_sender) {
    errno = 0;
    //variabili di appoggio
    int check = 1, delivered = 0, same_user = 0;

    //se la size del file è troppo grande
    if (req->data_file->hdr.len > conf_server.max_file_size) {
        return send_error(req, us_sender, OP_MSG_TOOLONG);
    }

    //prendo le informazioni riguardanti il file
    char *buf    = req->data_file->buf;
    int  len_buf = req->data_file->hdr.len;
    //prendo il nome del file rispetto al path completo che manda il sender
    char *name_file = get_filename(req->msg->data.buf);
    //nome del file non valido
    if (name_file == NULL && errno == 0) {
        return send_error(req, us_sender, OP_FAIL);
    }
    //se errore
    else if (errno != 0) {
        if (name_file != NULL) free(name_file);
        return -1;
    }
    
    if (req->msg->data.buf != NULL) free(req->msg->data.buf);
    req->msg->data.buf = name_file;
    req->msg->data.hdr.len = strlen(name_file) + 1;


    //cerco l'utente receiver o il gruppo destinatario
    user_t *us_receiver = NULL;
    group_t *gr_receiver = NULL;
    check = strcmp(req->msg->hdr.sender, req->msg->data.hdr.receiver);
    //se sender e receiver sono lo stesso utente
    if(check == 0) {
        us_receiver = us_sender;
        same_user   = 1;
    }
    //altrimenti cerco receiver
    else {
        us_receiver = search_data_ht(hash_us, (void*)req->msg->data.hdr.receiver);
        //se errore
        if (us_receiver == NULL && errno != 0) return -1;
        //se non esiste nessun utente con tale nome
        if (us_receiver == NULL && errno == 0) {
            //controllo se il destinatario è in realtà un gruppo a cui è iscritto l'utente
            gr_receiver = check_subscription(us_sender, req->msg->data.hdr.receiver);
            //se non è iscritto a tale gruppo mando il messaggio di errore
            if (gr_receiver == NULL && errno == 0) return send_error(req, us_sender, OP_NICK_UNKNOWN);
            //se errore 
            if (gr_receiver == NULL && errno != 0) return -1;
        }
    }


    //salvo il file nella directory dei files
    if (save_file(conf_server.dir_name, name_file, buf, len_buf) == -1) return -1;

    //creo il messaggio da inviare
    message_t *msg = duplicate_msg(req->msg, FILE_MESSAGE);
    err_return_msg(msg,NULL,-1,"Errore: duplicate_msg\n");


    //se il destinatario è un utente
    if (us_receiver != NULL) {
        //invio il messaggio al destinatario
        check = sendMsg_toUser(us_receiver, msg, &delivered);
        //se c'è stato un errore
        if (check == -1) return -1;
        //se si è disconnesso
        else if (check == 0) {
            if (same_user == 1) return 0;
        }

        //aggiorno le statistiche
        int checklock = lock_stats();
        err_check_return(checklock != 0, checklock, "lock_stats", -1);
        if (delivered == 1) chattyStats.nfiledelivered++;
        else chattyStats.nfilenotdelivered++;
        checklock = unlock_stats();
        err_check_return(checklock != 0, checklock, "unlock_stats", -1);

    }
    //se invece il destinatario è un gruppo
    else {

        //creo la struttura che contiene i parametri per la funzione postmsg_all_group
        param_postmsg_all_t *prm = init_param_postmsg_all(msg);
        err_return_msg_clean(prm,NULL,-1,"Errore: init_param_postmsg_all\n",free_msg(msg));

        //invio il messaggio a tutti i membri del gruppo
        check = postmsg_all_group(gr_receiver, prm);
        //in caso di errore
        if (check == -1){
            fprintf(stderr, "Errore: postmasg_all_group\n");
            free_msg(msg);
            free(prm);
            return -1;
        }
        //se il gruppo è in fase di cancellazione lo considero inesistente
        else if (check == 0) {
            free_msg(msg);
            free(prm);
            return send_error(req, us_sender, OP_NICK_UNKNOWN);
        }

        //aggiorno le statistiche
        int checklock = lock_stats();
        if (checklock != 0) {free_msg(msg); free(prm);} 
        err_check_return(checklock != 0, checklock, "lock_stats", -1);
        chattyStats.nfiledelivered = chattyStats.nfiledelivered + prm->delivered;
        chattyStats.nfilenotdelivered = chattyStats.nfilenotdelivered + prm->notdelivered;
        checklock = unlock_stats();
        if (checklock != 0) {free_msg(msg); free(prm);} 
        err_check_return(checklock != 0, checklock, "unlock_stats", -1);

        free_msg(msg);
        free(prm);

    }


    //invio il messaggio di buon esito all'utente sender
    setHeader(&req->msg->hdr, OP_OK, "");
    if (sendHdr_toUser(us_sender, &req->msg->hdr) == -1) return -1;

    return 0;
}


/**
 * @function getfile_fun
 * @brief Si occupa di inviare il file richiesto dall'utente
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param user  utente che vuole scaricare il file 
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int getfile_fun(request_t *req, user_t *user) {
    //variabili di appoggio
    int check = 1, delivered = 0;
    char *mappedfile = NULL;
    
    //mappo il file da inviare in memoria
    int size_file = get_mappedfile(&mappedfile, conf_server.dir_name, req->msg->data.buf);
    //in caso di errore
    if (size_file == -1) {
        if (mappedfile != NULL) munmap(mappedfile, size_file);
        return -1;
    }
    //se non esiste nessun file con tale nome
    if (mappedfile == NULL) return send_error(req, user, OP_NO_SUCH_FILE);

    //se il file esiste preparo il messaggio da inviare all'utente
    setHeader(&req->msg->hdr, OP_OK, "");
    if (req->msg->data.buf != NULL) free(req->msg->data.buf);
    setData(&req->msg->data, "", mappedfile, size_file);

    //invio il messaggio
    check = sendMsg_toUser(user, req->msg, &delivered);

    //libero la memoria 
    munmap(mappedfile, size_file);
    //per evitare che venga fatta una free su una parte di memoria già deallocata
    req->msg->data.buf = NULL;

    //se c'è stato un errore nell' invio 
    if (check == -1) return -1;

    return 0;
}


/**
 * @function getprevmsgs_fun
 * @brief Si occupa di inviare all'utente i messaggi nella sua history
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param user  utente che vuole scaricare il file
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int getprevmsgs_fun(request_t *req, user_t *user) {
    //variabili di appoggio
    int msgsdelivered = 0, filesdelivered = 0;
    
    //invio la history
    if (send_history(user, &msgsdelivered, &filesdelivered) == -1) return -1;

    //aggiorno le statistiche
    int checklock = lock_stats();
    err_check_return(checklock != 0, checklock, "lock_stats", -1);
    chattyStats.ndelivered        = chattyStats.ndelivered + msgsdelivered;
    chattyStats.nnotdelivered     = chattyStats.nnotdelivered - msgsdelivered;
    chattyStats.nfiledelivered    = chattyStats.nfiledelivered + filesdelivered;
    chattyStats.nfilenotdelivered = chattyStats.nfilenotdelivered - filesdelivered;
    checklock = unlock_stats();
    err_check_return(checklock != 0, checklock, "unlock_stats", -1);

    return 0;
}


/**
 * @function cancgroup_fun
 * @brief Si occupa della cancellazione di un gruppo
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 *              (può essere NULL nel caso in cui non sia una richiesta diretta di 
 *              cancellazione del gruppo)
 * @param user  utente che vuole cancellare il gruppo
 * @param gr    gruppo da cancellare (se NULL va cercato)
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int cancgroup_fun(request_t *req, user_t *user, group_t *gr) {
    errno = 0;
    group_t *group = gr;
    if (group == NULL) {
        //cerco il gruppo nella lista di quelli a cui è iscritto l'utente
        group_t *group = check_subscription(user, (void*)req->msg->data.hdr.receiver);
        //se l'utente non è iscritto a tale gruppo
        if (group == NULL && req != NULL && errno == 0) {
            //invio il messaggio di errore all' utente
            return send_error(req, user, OP_NICK_UNKNOWN);
        }
        //se errore
        if (group == NULL && errno != 0) return -1;
    }

    //'disabilito' il gruppo
    int check = disable_group(group, user->nickname);

    //se errore
    if (check == -1) return -1;
    //se l'utente non è il creatore
    else if (check == 0 && req != NULL) return send_error(req, user, OP_NO_CREATOR);

    //elimino il gruppo dalla tabella hash dei gruppi
    check = remove_data_ht(hash_gr, (void*)group->groupname);

    //se non esiste nessun gruppo con tale nome nella tabella hash
    //(controllo solo precauzionale)
    if (check != 1) {
        fprintf(stderr, "Errore: problema nella cancellazione gruppo\n");
        return -1;
    }
    
    //aggiorno le statistiche
    int checklock = lock_stats();
    err_check_return(checklock != 0, checklock, "lock_stats", -1);
    chattyStats.ngroups--;
    checklock = unlock_stats();
    err_check_return(checklock != 0, checklock, "unlock_stats", -1);

    //se era un richiesta diretta devo comunicare l'esito all'utente
    if (req != NULL) {
        //invio il messaggio di ok al client
        setHeader(&req->msg->hdr, OP_OK, "");
        if (sendHdr_toClient(req->fd, &req->msg->hdr) == -1) return -1;
    }

    return 0;
}


/**
 * @function unregister_fun
 * @brief Si occupa della deregistrazione di un utente
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param user  utente che vuole deregistrarsi
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int unregister_fun(request_t *req, user_t *user) {
    errno = 0;
    //controllo che l'utente stia deregistrando sè stesso e non altri utenti
    int check = strcmp(req->msg->hdr.sender, req->msg->data.hdr.receiver);
    if(check != 0) {
        //invio il messaggio di errore perchè sta provando a deregistrare
        //un altro utente
        return send_error(req, user, OP_FAIL);
    }

    //rimuovo l'utente dalla lista online
    user_t *p = remove_data(us_on, (void*)&req->fd);

    //disattivo l'utente (e lo rimuovo da tutti i gruppi a cui è iscritto)
    check = disable_user(user);
    if (check != 1) return check;

    //cancello gli eventuali gruppi di cui l'utente era creatore
    //(gli unici rimasti nella sua lista)
    group_t *group_to_canc = (group_t*)pop_data(user->groups);
    if (group_to_canc == NULL && errno != 0) return -1;
    while(group_to_canc != NULL){
        if (cancgroup_fun(NULL, user, group_to_canc) == -1) {
            fprintf(stderr, "Errore: problema nella deregistrazione\n");
            return -1;
        }
        group_to_canc = (group_t*)pop_data(user->groups);
        if (group_to_canc == NULL && errno != 0) return -1;
    }

    //rimuovo l'utente dalla tabella hash degli utenti registrati
    check = remove_data_ht(hash_us, (void*)req->msg->hdr.sender);

    //se non esiste nessun utente con tale nome nella tabella hash o nella lista degli
    //utenti online (controllo solo precauzionale)
    if (check != 1 || p == NULL) {
        fprintf(stderr, "Errore: problema nella deregistrazione\n");
        return -1;
    }
    //se OK
    else {
        //aggiorno le statistiche
        int checklock = lock_stats();
        err_check_return(checklock != 0, checklock, "lock_stats", -1);
        chattyStats.nonline--;
        chattyStats.nusers--;
        checklock = unlock_stats();
        err_check_return(checklock != 0, checklock, "unlock_stats", -1);

        //invio il messaggio di ok al client
        setHeader(&req->msg->hdr, OP_OK, "");
        if (sendHdr_toClient(req->fd, &req->msg->hdr) == -1) return -1;

        return 0;
    }
}


/**
 * @function creategroup_fun
 * @brief Si occupa della creazione di un gruppo
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param user  utente che vuole creare un gruppo
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int creategroup_fun(request_t *req, user_t *user) {
    errno = 0;
    //controllo che non esista già un utente con quel nome 
    user_t *us = search_data_ht(hash_us, (void*)req->msg->data.hdr.receiver);
    if (us != NULL) {
        //invio il messaggio di errore al client
        return send_error(req, user, OP_NICK_ALREADY);
    }
    //se errore
    else if (us == NULL && errno != 0) return -1;

    //creo il gruppo
    group_t *group = create_group(req->msg->data.hdr.receiver, user);
    err_return_msg(group,NULL,-1,"Errore: create_group\n");

    //provo ad aggiugerlo nella tabella hash
    int check = add_data_ht(hash_gr, (void*)group, (void*)group->groupname);

    //se errore
    if (check == -1) {
        clean_group(group);
        return -1;
    }
    //se esiste già un gruppo con tale nome
    else if (check == 0) {
        clean_group(group);
        //invio il messaggio di errore all'utente
        return send_error(req, user, OP_NICK_ALREADY);
    }
    //se OK
    else {
        //inserisco il gruppo nella lista dei gruppi d'iscrizione dell'utente
        if(subscribe(user, group) == -1) return -1;

        //aggiorno le statistiche
        int checklock = lock_stats();
        err_check_return(checklock != 0, checklock, "lock_stats", -1);
        chattyStats.ngroups++;
        checklock = unlock_stats();
        err_check_return(checklock != 0, checklock, "unlock_stats", -1);

        //invio il messaggio di buon esito all'utente
        setHeader(&req->msg->hdr, OP_OK, "");
        if (sendHdr_toUser(user, &req->msg->hdr) == -1) return -1;

        return 0;
    }
}


/**
 * @function addgroup_fun
 * @brief Si occupa dell'iscrizione di un utente ad un gruppo
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param user  utente che vuole iscriversi ad un gruppo
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int addgroup_fun(request_t *req, user_t *user) {
    errno = 0;
    //controllo che l'utente non sia già iscritto a tale gruppo
    group_t *group = check_subscription(user, (void*)req->msg->data.hdr.receiver);
    //se l'utente non è iscritto a tale gruppo
    if (group != NULL && errno == 0) {
        //invio il messaggio di errore all' utente
        return send_error(req, user, OP_FAIL);
    }
    //se errore
    if (group == NULL && errno != 0) return -1;

    //cerco il gruppo
    group = search_data_ht(hash_gr, (void*)req->msg->data.hdr.receiver);
    //se non esiste
    if (group == NULL && errno == 0) {
        //invio il messaggio di errore all' utente
        return send_error(req, user, OP_NICK_UNKNOWN);
    }
    //se errore
    else if (group == NULL && errno != 0) return -1;

    //provo ad aggiugere l'utente tra i membri del gruppo
    int check = add_member(group, user);

    //se errore
    if (check == -1) return -1;
    //se il gruppo è in fase di cancellazione lo considero come inesistente
    else if (check == 0) return send_error(req, user, OP_NICK_UNKNOWN);
    //se OK
    else {
        //inserisco il gruppo nella lista dei gruppi d'iscrizione dell'utente
        if(subscribe(user, group) == -1) return -1;

        //invio il messaggio di buon esito all'utente
        setHeader(&req->msg->hdr, OP_OK, "");
        if (sendHdr_toUser(user, &req->msg->hdr) == -1) return -1;

        return 0;
    }
}


/**
 * @function delgroup_fun
 * @brief Si occupa della disiscrizione di un utente da un gruppo
 * 
 * @param req   richiesta dalla quale ricavare le informazioni per eseguire la funzione
 * @param user  utente che vuole disiscriversi da un gruppo
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 altrimenti
 */
static int delgroup_fun(request_t *req, user_t *user) {
    errno = 0;
    //elimino il gruppo dalla lista dei gruppi a cui è iscritto l'utente
    group_t *group = unsubscribe(user, (void*)req->msg->data.hdr.receiver);
    //se non è iscritto a quel gruppo
    if (group == NULL && errno == 0) {
        //invio il messaggio di errore all' utente
        return send_error(req, user, OP_NICK_UNKNOWN);
    }
    //errore
    else if (group == NULL && errno != 0) return -1;

    //elimino l'utente tra i membri del gruppo
    int is_creator = 0;
    user_t *us = remove_member(group, user->nickname, &is_creator);
    //se errore
    if (us == NULL && errno != 0) return -1;

    //se l'utente rimosso è il creatore del gruppo cancello il gruppo
    if (us != NULL && is_creator == 1) {
        if (cancgroup_fun(NULL, user, group) == -1) {
            fprintf(stderr, "Errore: problema nella rimozione del creatore\n");
            return -1;
        }
    }

    //invio il messaggio di buon esito all'utente
    setHeader(&req->msg->hdr, OP_OK, "");
    if (sendHdr_toUser(user, &req->msg->hdr) == -1) return -1;

    return 0;
}


/* ---------------------------- interfaccia worker -------------------------------- */

/**
 * @function worker
 * @brief Funzione eseguita dal thread worker 
 * 
 * @param arg argomenti necessari al thread worker
 * 
 * @return 0 in caso di successo, altrimenti error
 */
void* worker(void* arg){
    //prendo gli argomenti passati alla funzione
    tid      = ((args_worker_t*)arg)->tid;
    fd_queue = ((args_worker_t*)arg)->fd_queue;
    us_on    = ((args_worker_t*)arg)->us_on;
    hash_us  = ((args_worker_t*)arg)->hash_us;
    hash_gr  = ((args_worker_t*)arg)->hash_gr;
    pipe_fd  = ((args_worker_t*)arg)->pipe_fd;
    tid_sh   = ((args_worker_t*)arg)->tid_sh;


    //variabile di ritorno pthread_exit
    long ret = 0;

    //variabili di appoggio
    int check = 0, checklock = 0;

    while(1) {
        errno = 0;
        //prelevo dalla coda un fd pronto ad inviare una richiesta
        long connfd = pop_fd(fd_queue);
        //in caso di errore
        if (connfd == -1 && errno != 0) {
            ret = errno;
            pthread_exit((void *) ret);
        }
        //se l'fd è -1 (ma errno non è settato) significa che il worker thread deve terminare
        if (connfd == -1 && errno == 0) pthread_exit((void *) ret);

        //controllo se l'fd appartiene ad un utente già online
        user_t *user = search_data(us_on, (void *) &connfd);
        if (user == NULL && errno != 0) quit_worker(tid_sh);

        //alloco la memoria per la richiesta
        request_t *req = malloc(sizeof(request_t));
        if (req == NULL) quit_worker(tid_sh);

        //leggo la richiesta del client 
        if(user != NULL) {
            //lock sull'utente 
            checklock = lock_user(user);
            if (checklock != 0) {
                errno = checklock;
                quit_worker(tid_sh);
            }

            check = read_request(connfd, req);
            //se si è disconnesso lo metto offline
            if (check == 0 || (check == -1 && errno == ECONNRESET)) user->status = OFFLINE;

            //unlock utente 
            checklock = unlock_user(user);
            if (checklock != 0) {
                errno = checklock;
                quit_worker(tid_sh);
            }
        }
        else check = read_request(connfd, req);

        //controllo l'esito della lettura della richiesta 
        //se errore
        if (check == -1 && errno != ECONNRESET) {
            fprintf(stderr, "Errore: read request\n");
            quit_worker(tid_sh);
        }
        //se fd chiuso durante la lettura della richiesta
        else if (check == 0 || (check == -1 && errno == ECONNRESET)) {
            //rimuovo l'utente collegato con tale fd dalla lista degli utenti online
            if (disconnect_fun(connfd) == -1) quit_worker(tid_sh);
            //comunico al listener di chiudere tale fd
            if (write_pipe(pipe_fd, (int)connfd, CLOSE) == -1) quit_worker(tid_sh);
        }
        //se ok eseguo la richiesta
        else {
            //prendo l'operazione richiesta 
            op_t op = req->msg->hdr.op;

            //l'utente deve essere online (ad eccezione della connessione/registrazione)
            if (op != REGISTER_OP && op != CONNECT_OP && user == NULL ) {
                //invio l'errore al client che ha fatto la richiesta
                if (send_error(req, NULL, OP_NICK_UNKNOWN) != 0) {
                    free_request(req);
                    quit_worker(tid_sh);
                }
            }
            //se è stata fatta una richiesta lecita
            else {
                errno = 0;
                //controllo quale operazione è richiesta
                switch(op) {
                    case REGISTER_OP:
                        check = register_fun(req);
                        break;
                    case CONNECT_OP:
                        check = connect_fun(req);
                        break;
                    case POSTTXT_OP:
                        check = posttxt_fun(req, user);
                        break;
                    case POSTTXTALL_OP:
                        check = posttxtall_fun(req, user);
                        break;
                    case POSTFILE_OP:
                        check = postfile_fun(req, user);
                        break;
                    case GETFILE_OP:
                        check = getfile_fun(req, user);
                        break;
                    case GETPREVMSGS_OP:
                        check = getprevmsgs_fun(req, user);
                        break;
                    case USRLIST_OP:
                        check = usrlist_fun(req, user);
                        break;
                    case UNREGISTER_OP:
                        check = unregister_fun(req, user);
                        break;
                    case CREATEGROUP_OP:
                        check = creategroup_fun(req, user);
                        break;
                    case ADDGROUP_OP:
                        check = addgroup_fun(req, user);
                        break;
                    case DELGROUP_OP:
                        check = delgroup_fun(req, user);
                        break;
                    case CANCGROUP_OP:
                        check = cancgroup_fun(req, user, NULL);
                        break;
                    default:
                        ;
                    break;
                }
            }

            free_request(req);

            //comunico al listener che è stata eseguita la richiesta 
            if (check == 0) {
                if (write_pipe(pipe_fd, (int)connfd, UPDATE) == -1) quit_worker(tid_sh);
            }
            //se c'è stato qualche errore
            else if (check != 0) quit_worker(tid_sh);
        }
    }
}