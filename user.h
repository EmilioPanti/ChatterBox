/**
 * @file user.h
 * @brief File per la gestione/creazione di utenti
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef USER_H_
#define USER_H_

#include <pthread.h>
#include <message.h>
#include <abs_list.h>


//per usare la struttura group definita in group.h
typedef struct group group_us_t;


//stati assumibili da una struttura utente
typedef enum {
    ONLINE    = 0,   //l'utente è online
    OFFLINE   = 1,   //l'utente è offline
    INACTIVE  = 2,   //l'utente è in fase di deregistrazione
} status_t;


/**
 * @struct user_t
 * @brief Struttura dati utente
 * 
 * @var nickname  nome dell'utente
 * @var status    status dell'utente
 * @var fd        descrittore aperto verso il client
 * @var mtx       puntatore al mutex per controllare l'accesso alla struttura utente
 * @var msg_list  lista dei messaggi arrivati all'utente
 * @var groups    lista di gruppi a cui è iscritto l'utente
 */
typedef struct user {
    char            nickname[MAX_NAME_LENGTH+1];
    status_t        status;
    long            fd;
    pthread_mutex_t *mtx;
    list_t          *msg_list;
    list_t          *groups;
} user_t;


/**
 * @struct param_get_listname_t
 * @brief Struttura dati per i parametri della funzione get_listname
 * 
 * @var len         lunghezza effettiva della stringa all_names
 * @var maxlen      lunghezza massima della stringa all_names
 * @var all_names   stringa contenente la lista nomi
 */
typedef struct {
    int   len;
    int   maxlen;
    char  *all_names;
} param_get_listname_t;


/**
 * @struct param_postmsg_all_t
 * @brief Struttura dati per i parametri della funzione postmsg_all
 * 
 * @var msg_to_send    messaggio da inviare
 * @var delivered      contatore degli utenti a cui è stato consegnato il messaggio
 * @var notdelivered   contatore degli utenti a cui non è stato consegnato il messaggio
 */
typedef struct {
    message_t *msg_to_send;
    int delivered;
    int notdelivered;
} param_postmsg_all_t;



/* ------------------------------ interfaccia user ------------------------------- */

/* ------------- funzioni strettamente legate alla strutture 'user_t' ---------------- */

/**
 * @function lock_user
 * @brief prende la lock dell'utente
 * 
 * @param user puntatore all'utente
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
int lock_user(user_t *user);


/**
 * @function unlock_user
 * @brief rilascia la lock dell'utente
 * 
 * @param user puntatore all'utente
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
int unlock_user(user_t *user);


/**
 * @function create_user
 * @brief Crea ed alloca la memoria necessaria per un utente
 * 
 * @param name   nome utente
 * @param fd     descrittore dell'utente al momento della sua creazione
 * 
 * @return p puntatore al nuovo utente, NULL in caso di fallimento (errno settato)
 */
user_t *create_user(char *name, long fd);


/**
 * @function clean_user
 * @brief Libera la memoria allocata per l'utente
 * 
 * @param us puntatore all'utente da cancellare
 */
void clean_user(void *us);


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
int set_online(user_t *user, long fd);


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
int disable_user(user_t *user);


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
int sendMsg_toUser(user_t *user, message_t *msg, int *sent);


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
int sendHdr_toUser(user_t *user, message_hdr_t *hdr);


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
int send_history(user_t *user, int *msgsdelivered, int *filesdelivered);


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
int subscribe(user_t *user, group_us_t *group);


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
group_us_t* unsubscribe(user_t *user, char *groupname);


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
group_us_t* check_subscription(user_t *user, char *groupname);



/* ------------- funzioni di inizializzazione delle varie struct  ---------------- */

/**
 * @function init_param_get_listname
 * @brief Inizializza un struttura 'param_get_listname_t'
 * 
 * @param  max_user_in_list  numero massimo di nomi utente da inserire nella lista dei nomi
 * 
 * @return p puntatore alla nuova struttura, NULL in caso di fallimento
 */
param_get_listname_t *init_param_get_listname(int max_user_in_list);


/**
 * @function init_param_postmsg_all
 * @brief Inizializza un struttura 'param_postmsg_all_t'
 * 
 * @param msg  messaggio da inviare
 * 
 * @return p puntatore alla nuova struttura, NULL in caso di fallimento
 */
param_postmsg_all_t *init_param_postmsg_all(message_t *msg);



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
void setmutex_user(void *us, pthread_mutex_t *mutex);


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
int hashfun_user(int dim, void *name);


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
int cmp_user_by_name(void *us, void *name);


/**
 * @function cmp_user_by_fd
 * @brief Compara l'fd di un utente con un altro passato come parametro
 * 
 * @param us   utente di cui comparare l'fd
 * @param fd   fd da comparare con quello di us
 * 
 * @return (user->fd - *fd) se successo, -1 ed errno settato in caso di errore
 */
int cmp_user_by_fd(void *us, void *fd);


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
int get_listname(void *us, void *param);


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
int postmsg_all(void *us, void *param);


/**
 * @function gen_unsubscribe
 * @brief Rimuove il gruppo passato dalla lista gruppi dell' utente
 * 
 * @param us    puntatore all' utente
 * @param str   nome del gruppo da rimuovere dai membri del gruppo
 * 
 * @return 0 se successo, -1 in caso di errore
 */
int gen_unsubscribe(void *us, void *str);


#endif /* USER_H_ */