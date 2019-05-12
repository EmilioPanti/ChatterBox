/**
 * @file user.c
 * @brief File contenente l'implementazioni delle funzioni dichiarate in user.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <string.h>
#include <stdlib.h>

#include <error_handler.h>
#include <user.h>
#include <connections.h>
#include <config.h>
#include <group.h>

//funzioni necessarie per la creazione e gestione della lista 
//dei messaggi dell'utente (history)
extern void clean_msg_node(void *node);
extern int send_list_msgs(void *node, void *param);

//funzioni necessarie per la creazione e gestione della lista 
//dei gruppi a cui si iscriverà l'utente
extern int cmp_group(void *gr, void *name);
extern int gen_remove_member(void *gr, void *us);


/* ------------------------- implementazione interfaccia user ---------------------------- */

/* ------------- funzioni strettamente legate alla strutture 'user_t' ---------------- */

/**
 * @function lock_user
 * @brief prende la lock dell'utente
 * 
 * @param user puntatore all'utente
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
int lock_user(user_t *user){ 
    return pthread_mutex_lock(user->mtx);
}


/**
 * @function unlock_user
 * @brief rilascia la lock dell'utente
 * 
 * @param user puntatore all'utente
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
int unlock_user(user_t *user){ 
    return pthread_mutex_unlock(user->mtx);
}


/**
 * @function create_user
 * @brief Crea ed alloca la memoria necessaria per un utente
 * 
 * @param name   nome utente
 * @param fd     descrittore dell'utente al momento della sua creazione
 * 
 * @return p puntatore al nuovo utente, NULL in caso di fallimento (errno settato)
 */
user_t *create_user(char *name, long fd){
    //controllo gli argomenti 
    err_check_return(fd < 0, EINVAL, "create_user", NULL);
    err_check_return(name == NULL, EINVAL, "create_user", NULL);

    //configurazioni del server (definita in chatty.c)
    extern configs_t conf_server;

    //alloco la memoria per l'utente
    user_t *us = malloc(sizeof(user_t));
    err_return_msg(us,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri dell'utente
    us->status   = ONLINE;
    us->fd       = fd;
    us->mtx      = NULL;
    strncpy(us->nickname, name, MAX_NAME_LENGTH+1);
    us->msg_list = init_list(conf_server.max_hist_msg, NULL, clean_msg_node, NULL);
    err_return_msg_clean(us->msg_list, NULL, NULL, "Errore: init_list\n", clean_user(us));
    us->groups = init_list(DEFAULT_LEN, NULL, NULL, cmp_group);
    err_return_msg_clean(us->groups, NULL, NULL, "Errore: init_list\n", clean_user(us));

    return us;
}


/**
 * @function clean_user
 * @brief Libera la memoria allocata per l'utente
 * 
 * @param us puntatore all'utente da cancellare
 */
void clean_user(void *us){
    if(us == NULL) return;
    user_t *user = (user_t *)us;
    if (user->msg_list != NULL) clean_list(user->msg_list);
    if (user->groups != NULL) clean_list(user->groups);
    free(user);
}


/**
 * @function set_online
 * @brief Cerca di mettere online l'utente passato 
 * 
 * @param user   utente da mettere online
 * @param fd     descrittore da assegnare all'utente
 * 
 * @return 1 se successo, 0 se era già online oppure se inattivo, 
 *         -1 in caso di errore
 */
int set_online(user_t *user, long fd){
    //controllo gli argomenti 
    err_check_return(fd < 0, EINVAL, "set_online", -1);
    err_check_return(user == NULL, EINVAL, "set_online", -1);

    //variabile di ritorno
    int ret = 0;

    int checklock = lock_user(user);
    err_check_return(checklock != 0, checklock, "lock_user", -1);

    //se offline 
    if (user->status == OFFLINE) {
        user->fd = fd;
        user->status = ONLINE;
        ret = 1;
    }
    //altrimenti
    else ret = 0;

    checklock = unlock_user(user);
    err_check_return(checklock != 0, checklock, "unlock_user", -1);

    return ret;
}


/**
 * @function disable_user
 * @brief Pone ad inattivo lo stato dell' utente ed elimina quest'ultimo da tutti i gruppi
 *        a cui è iscritto
 * 
 * @param user              utente da disattivare
 * @param groupnames_list   struttura dati per salvare i nomi dei gruppi di cui l'utente
 *                          era creatore
 * 
 * @return 1 se successo, 0 se era già inattivo l'utente, -1 se in caso di errore
 */
int disable_user(user_t *user){
    //controllo gli argomenti
    err_check_return(user == NULL, EINVAL, "disable_user", -1);

    //variabile di appoggio
    int check = 0, checklock = 0;

    //metto lo status dell' utente ad inattivo
    checklock = lock_user(user);
    err_check_return(checklock != 0, checklock, "lock_user", -1);

    if (user->status != INACTIVE) user->status = INACTIVE;
    else check = 1;

    checklock = unlock_user(user);
    err_check_return(checklock != 0, checklock, "unlock_user", -1);

    //se era già inattivo
    if (check == 1) return 0;

    //rimuovo l'utente da tutti i gruppi a cui appartiene
    check = apply_fun_param(user->groups, gen_remove_member, (void *)user);
    err_return_msg(check,-1,-1,"Errore: disable user\n");

    return 1;
}


/**
 * @function sendMsg_toUser
 * @brief Spedisce il messaggio passato da parametro all'utente specificato.
 * 
 * @param user       utente a cui inviare il messaggio
 * @param msg        messaggio da inviare
 * @param sent       per sapere all'esterno se è stato consegnato o meno il messaggio 
 * 
 * @return 1 se successo, 0 se l'utente si è disconnesso durante l'invio del messaggio o se era inattivo,
 *         -1 in caso di errore 
 */
int sendMsg_toUser(user_t *user, message_t *msg, int *sent){
    //controllo gli argomenti
    err_check_return(user == NULL, EINVAL, "sendMsg_toUser", -1);
    err_check_return(msg == NULL, EINVAL, "sendMsg_toUser", -1);
    err_check_return(sent == NULL, EINVAL, "sendMsg_toUser", -1);

    //variabile di appoggio
    int check = 1, checklock = 0;
    //operazione nel messaggio
    op_t op = msg->hdr.op;

    checklock = lock_user(user);
    err_check_return(checklock != 0, checklock, "lock_user", -1);

    //se è inattivo non faccio niente
    if (user->status == INACTIVE) {
        free_msg(msg);
        checklock = unlock_user(user);
        err_check_return(checklock != 0, checklock, "unlock_user", -1);
        return 0;
    }

    //se è online invio il messaggio 
    if (user->status == ONLINE) {
        check = sendMsg_toClient(user->fd, msg);
        //se il messaggio è stato inviato
        if (check == 1) *sent = 1;
        //se si è disconnesso durante l'invio
        else if (check == 0) user->status = OFFLINE;
        //in caso di errore
        else {
            unlock_user(user);
            free_msg(msg);
            return -1;
        }
    }
    //se è offline deve ritornare 0 alla fine 
    else check = 0;

    //se è un messaggio testuale o un file devo aggiungerlo alla history
    if (op == TXT_MESSAGE || op == FILE_MESSAGE) {
        //creo il nodo messaggio da inserire nella history
        message_node_t *msg_node = init_msg_node(msg, *sent);
        if (msg_node == NULL) {
            unlock_user(user);
            free_msg(msg);
            return -1;
        }
        //aggiungo il nuovo messaggio nella history
        if (add_data(user->msg_list, (void*)msg_node, NULL) == -1) {
            unlock_user(user);
            clean_msg_node(msg_node);
            return -1;
        }
    }

    checklock = unlock_user(user);
    err_check_return(checklock != 0, checklock, "unlock_user", -1);
    
    return check;
}


/**
 * @function sendHdr_toUser
 * @brief Spedisce l'header del messaggio passato da parametro all'utente specificato.
 * 
 * @param user   utente a cui inviare il messaggio
 * @param hdr    header del messaggio da inviare
 * 
 * @return 1 se successo, 0 se l'utente si è disconnesso durante l'invio del messaggio o se era inattivo,
 *         -1 in caso di errore 
 */
int sendHdr_toUser(user_t *user, message_hdr_t *hdr){
    //controllo gli argomenti
    err_check_return(user == NULL, EINVAL, "sendHdr_toUser", -1);
    err_check_return(hdr == NULL, EINVAL, "sendHdr_toUser", -1);

    //variabile di appoggio
    int check = 0, checklock = 0;

    checklock = lock_user(user);
    err_check_return(checklock != 0, checklock, "lock_user", -1);

    //se è online invio il messaggio 
    if (user->status == ONLINE) {
        check = sendHdr_toClient(user->fd, hdr);
        if (check == 0) user->status = OFFLINE;
    }

    checklock = unlock_user(user);
    err_check_return(checklock != 0, checklock, "unlock_user", -1);
    
    return check;
}


/**
 * @function send_history
 * @brief Spedisce la history dei messaggi all'utente passato da parametro
 * 
 * @param user            utente a cui inviare la history
 * @param msgsdelivered   contatore dei messaggi testuali inviati all'utente in questa funzione
 * @param filesdelivered  contatore dei messaggi files inviati all'utente in questa funzione
 * 
 * @return 1 se successo, 0 se l'utente si è disconnesso durante l'invio dei messaggi o se era inattivo,
 *         -1 in caso di errore 
 */
int send_history(user_t *user, int *msgsdelivered, int *filesdelivered){
    //controllo gli argomenti
    err_check_return(user == NULL, EINVAL, "send_history", -1);
    err_check_return(msgsdelivered == NULL, EINVAL, "send_history", -1);
    err_check_return(filesdelivered == NULL, EINVAL, "send_history", -1);

    //variabile di appoggio
    int check = 1, checklock = 0;

    checklock = lock_user(user);
    err_check_return(checklock != 0, checklock, "lock_user", -1);

    //se è inattivo o offline non faccio niente
    if (user->status == INACTIVE || user->status == OFFLINE) {
        checklock = unlock_user(user);
        err_check_return(checklock != 0, checklock, "unlock_user", -1);
        return 0;
    }


    //prendo il numero di messaggi nella history
    int n = get_len_list(user->msg_list);
    if (n == -1) {
        unlock_user(user);
        return -1;
    }

    //buffer contenente il numero di messaggi in lista
    char *buf = malloc(sizeof(size_t));
    if (buf == NULL) {
        unlock_user(user);
        return -1;
    }
    memcpy(buf, (char*)&n, sizeof(size_t));

    //messaggio contenente il numero di messaggi da inviare
    message_t *message = malloc(sizeof(message_t));
    if (message == NULL) {
        fprintf(stderr, "Errore: malloc\n");
        free(buf);
        unlock_user(user);
        return -1;
    }
    setHeader(&message->hdr, OP_OK, "");
    setData(&message->data, "", buf, strlen(buf)+1);

    //invio il messaggio con il numero di messaggi da inviare
    check = sendMsg_toClient(user->fd, message);
    free_msg(message);
    //in caso di errore
    if (check == -1) {
        unlock_user(user);
        return -1;
    }
    //se si è disconnesso il client 
    else if (check == 0) {
        user->status = OFFLINE;
        checklock = unlock_user(user);
        err_check_return(checklock != 0, checklock, "unlock_user", -1);
        return 0;
    }
    

    //creo la struttura che contiene i parametri per la funzione send_list_msgs
    param_send_msgs_t *prm = init_param_send_msgs(user->fd);
    if (prm == NULL) {
        fprintf(stderr, "Errore: init_param_postmsg_all\n");
        unlock_user(user);
        return -1;
    }

    //invio tutta la history all' utente
    if (apply_fun_param(user->msg_list, send_list_msgs, (void*)prm) == -1){
        fprintf(stderr, "Errore: send_list_msgs\n");
        free(prm);
        unlock_user(user);
        return -1;
    }

    //se si è disconnesso durante l'invio della history
    if (prm->disconnected == 1) user->status = OFFLINE;

    checklock = unlock_user(user);
    err_check_return(checklock != 0, checklock, "unlock_user", -1);
    
    //aggiorno i dati per le statistiche
    *msgsdelivered  = prm->msgsdelivered;
    *filesdelivered = prm->filesdelivered;
    free(prm);

    return check;
}


/**
 * @function subscribe
 * @brief Inserisce il gruppo passato nella lista gruppi dell' utente
 * 
 * @param user    utente 
 * @param group   gruppo da inserire nella lista dei gruppi dell'utente
 * 
 * @return 1 se successo, 0 se l'utente è già iscritto a tale gruppo o se era inattivo,
 *         -1 in caso di errore 
 */
int subscribe(user_t *user, group_us_t *group){
    //controllo gli argomenti
    err_check_return(user == NULL, EINVAL, "subscribe", -1);
    err_check_return(group == NULL, EINVAL, "subscribe", -1);

    //variabile di appoggio
    int check = 1, checklock = 0;

    checklock = lock_user(user);
    err_check_return(checklock != 0, checklock, "lock_user", -1);

    //se è inattivo non faccio niente
    if (user->status == INACTIVE) {
        checklock = unlock_user(user);
        err_check_return(checklock != 0, checklock, "unlock_user", -1);
        return 0;
    }

    //inserisco il gruppo nella lista dell'utente
    check = add_data(user->groups, (void*)group, (void*)group->groupname);

    checklock = unlock_user(user);
    err_check_return(checklock != 0, checklock, "unlock_user", -1);
    
    return check;
}


/**
 * @function unsubscribe
 * @brief Rimuove il gruppo passato dalla lista gruppi dell' utente
 * 
 * @param user        utente 
 * @param groupname   nome del gruppo da rimuovere dalla lista dei gruppi dell'utente
 * 
 * @return puntatore al gruppo rimosso, NULL ed errno non modificato nel caso non sia presente
 *         nella lista dei gruppi dell' utente o se l'utente è inattivo, NULL ed errno settato
 *         in caso di errore
 */
group_us_t* unsubscribe(user_t *user, char *groupname){
    //controllo gli argomenti
    err_check_return(user == NULL, EINVAL, "unsubscribe", NULL);
    err_check_return(groupname == NULL, EINVAL, "unsubscribe", NULL);

    //variabile di appoggio
    group_us_t *gr = NULL;

    int checklock = lock_user(user);
    err_check_return(checklock != 0, checklock, "lock_user", NULL);

    //se è inattivo non faccio niente
    if (user->status == INACTIVE) {
        checklock = unlock_user(user);
        err_check_return(checklock != 0, checklock, "unlock_user", NULL);
        return NULL;
    }

    //rimuovo il gruppo dalla lista dell'utente
    gr = remove_data(user->groups, (void*)groupname);

    checklock = unlock_user(user);
    err_check_return(checklock != 0, checklock, "unlock_user", NULL);
    
    return gr;
}


/**
 * @function check_subscription
 * @brief Controlla che l'utente sia iscritto al gruppo passato
 * 
 * @param user        utente 
 * @param groupname   nome del gruppo da cercare nella lista dei gruppi dell'utente
 * 
 * @return puntatore al gruppo trovato, NULL ed errno non modificato nel caso non sia presente
 *         nella lista dei gruppi dell' utente o se l'utente è inattivo, NULL ed errno settato
 *         in caso di errore
 */
group_us_t* check_subscription(user_t *user, char *groupname){
    //controllo gli argomenti
    err_check_return(user == NULL, EINVAL, "check_subscription", NULL);
    err_check_return(groupname == NULL, EINVAL, "check_subscription", NULL);

    //variabile di appoggio
    group_us_t *gr = NULL;

    int checklock = lock_user(user);
    err_check_return(checklock != 0, checklock, "lock_user", NULL);

    //se è inattivo non faccio niente
    if (user->status == INACTIVE) {
        checklock = unlock_user(user);
        err_check_return(checklock != 0, checklock, "unlock_user", NULL);
        return NULL;
    }

    //cerco il gruppo dalla lista dell'utente
    gr = search_data(user->groups, (void*)groupname);

    checklock = unlock_user(user);
    err_check_return(checklock != 0, checklock, "unlock_user", NULL);
    
    return gr;
}



/* ------------- funzioni di inizializzazione delle varie struct  ---------------- */

/**
 * @function init_param_get_listname
 * @brief Inizializza un struttura 'param_get_listname_t'
 * 
 * @param  max_user_in_list  numero massimo di nomi utente da inserire nella lista dei nomi
 * 
 * @return p puntatore alla nuova struttura, NULL in caso di fallimento
 */
param_get_listname_t *init_param_get_listname(int max_user_in_list){
    //controllo gli argomenti
    err_check_return(max_user_in_list < 0, EINVAL, "init_param_get_listname", NULL);

    //alloco la memoria per struttura 'param_get_listname_t'
    param_get_listname_t *prm = malloc(sizeof(param_get_listname_t));
    err_return_msg(prm,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri della lista
    prm->len = 0;
    prm->maxlen = (MAX_NAME_LENGTH+1) * (max_user_in_list);
    prm->all_names = malloc(sizeof(char)*(prm->maxlen));
    err_return_msg_clean(prm->all_names, NULL, NULL, "Errore: malloc\n", free(prm));

    return prm;
}


/**
 * @function init_param_postmsg_all
 * @brief Inizializza un struttura 'param_postmsg_all_t'
 * 
 * @param msg  messaggio da inviare
 * 
 * @return p puntatore alla nuova struttura, NULL in caso di fallimento
 */
param_postmsg_all_t *init_param_postmsg_all(message_t *msg){
    //controllo gli argomenti
    err_check_return(msg == NULL, EINVAL, "init_param_postmsg_all", NULL);

    //alloco la memoria per struttura 'param_postmsg_all_t'
    param_postmsg_all_t *prm = malloc(sizeof(param_postmsg_all_t));
    err_return_msg(prm,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri della lista
    prm->msg_to_send  = msg;
    prm->delivered    = 0;
    prm->notdelivered = 0;

    return prm;
}



/*------------ funzioni per la creazione/gestione/uso di liste ed hashtable -----------------
----------------- generiche che hanno come elementi degli utenti 'user_t' ------------------------*/

/**
 * @function setmutex_user
 * @brief Assegna la mutex passata alla struttura utente passata
 * 
 * @param us     utente a cui assegnare la mutex
 * @param mutex  mutex da assegnare
 * 
 * @note: setta errno in caso di errore
 */
void setmutex_user(void *us, pthread_mutex_t *mutex){
    //controllo gli argomenti
    if (us == NULL) {
        errno = EINVAL; 
        perror("setmutex_user"); 
        return;
    }
    user_t *user = (user_t *)us;
    user->mtx = mutex;
}


/**
 * @function hashfun_user
 * @brief funzione hash che restituisce un valore tra 0 e (dim-1)
 * 
 * @param dim   modulo da applicare alla funzione hash calcolata su name
 * @param name  stringa a cui applicare la funzione hash
 * 
 * @return ind  valore compreso tra 0 e (dim-1),
 *         -1 in caso di errore e setta errno
 * 
 * @note: è stato ripreso l'algoritmo "djb2" by Dan Bernstein.
 */
int hashfun_user(int dim, void *name){
    //controllo gli argomenti
    err_check_return(dim < 0, EINVAL, "hashfun_user", -1);
    err_check_return(name == NULL, EINVAL, "hashfun_user", -1);

    char *str = (char *)name;
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    
    return hash % dim;
}


/**
 * @function cmp_user_by_name
 * @brief Compara il nickname di un utente con un altro nome
 * 
 * @param us     utente di cui comparare il nickname 
 * @param name   nome da comparare con il nickname di user
 * 
 * @return un valore < 0 se (us->nickname < name),
 *         un valore = 0 se (us->nickname = name),
 *         un valore > 0 se (us->nickname > name),
 *         -1 ed errno settato in caso di errore
 */
int cmp_user_by_name(void *us, void *name){
    //controllo gli argomenti
    err_check_return(us == NULL, EINVAL, "cmp_user_by_name", -1);
    err_check_return(name == NULL, EINVAL, "cmp_user_by_name", -1);

    user_t *user = (user_t *)us;
    char *nick = (char*)name;
    return strncmp(user->nickname, nick, MAX_NAME_LENGTH);
}


/**
 * @function cmp_user_by_fd
 * @brief Compara l'fd di un utente con un altro passato come parametro
 * 
 * @param us   utente di cui comparare l'fd
 * @param fd   fd da comparare con quello di us
 * 
 * @return (user->fd - *fd) se successo, -1 ed errno settato in caso di errore
 */
int cmp_user_by_fd(void *us, void *fd){
    //controllo gli argomenti
    err_check_return(us == NULL, EINVAL, "cmp_user_by_fd", -1);
    err_check_return(fd == NULL, EINVAL, "cmp_user_by_fd", -1);

    user_t *user = (user_t *)us;
    int *fd_int = (int*)fd;
    return (user->fd - *fd_int);
}


/**
 * @function get_listname
 * @brief Inserisce in param->all_names anche us->nickname e aggiunge
 *        a param->len un valore pari a (MAX_NAME_LENGTH+1)
 *        
 * @param us      puntatore all'utente 
 * @param param   struttura in cui ci sono i dati da aggiornare/usare per la funzione
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int get_listname(void *us, void *param){
    //controllo gli argomenti
    err_check_return(us == NULL, EINVAL, "get_listname", -1);
    err_check_return(param == NULL, EINVAL, "get_listname", -1);

    user_t *user = (user_t *)us;
    param_get_listname_t *prm = (param_get_listname_t *)param;

    //se non ho già riempito prm->all_names
    if(prm->len < prm->maxlen) {
        //copio il nickname dell'utente 
        strncpy(((prm->all_names) + prm->len), user->nickname, MAX_NAME_LENGTH+1);
        //aggiorno prm->len
        prm->len = prm->len + (MAX_NAME_LENGTH+1);
    }

    return 0;
}


/**
 * @function postmsg_all
 * @brief Invia il messaggio param->msg_to_send all'utente us e poi aggiorna
 *        i valori param->delivered o param->notdelivered a seconda dell'esito
 *        dell' invio del messaggio
 *        
 * @param us      puntatore all'utente 
 * @param param   struttura in cui ci sono i dati da aggiornare/usare per la funzione
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int postmsg_all(void *us, void *param){
    //controllo gli argomenti
    err_check_return(us == NULL, EINVAL, "postmsg_all", -1);
    err_check_return(param == NULL, EINVAL, "postmsg_all", -1);

    user_t *user = (user_t *)us;
    param_postmsg_all_t *prm = (param_postmsg_all_t *)param;

    //duplico il messaggio da inviare
    message_t *msg = duplicate_msg(prm->msg_to_send, prm->msg_to_send->hdr.op);
    err_return_msg(msg,NULL,-1,"Errore: duplicate_msg\n");

    //spedisco il messaggio all'utente
    int sent = 0;
    if (sendMsg_toUser(user, msg, &sent) == -1) return -1;
        
    //aggiorno i contatori all'interno di param
    if (sent == 1) prm->delivered = prm->delivered +1;
    else prm->notdelivered = prm->notdelivered +1;

    return 0;
}


/**
 * @function gen_unsubscribe
 * @brief Rimuove il gruppo passato dalla lista gruppi dell' utente
 * 
 * @param us    puntatore all' utente
 * @param str   nome del gruppo da rimuovere dai membri del gruppo
 * 
 * @return 0 se successo, -1 in caso di errore
 */
int gen_unsubscribe(void *us, void *str){
    //controllo gli argomenti
    err_check_return(us == NULL, EINVAL, "gen_unsubscribe", -1);
    err_check_return(str == NULL, EINVAL, "gen_unsubscribe", -1);

    user_t *user = (user_t*)us;
    char *name = (char*)str;

    //rimuovo il gruppo
    unsubscribe(user, name);

    return 0;
}