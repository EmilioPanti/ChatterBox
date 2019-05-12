/**
 * @file group.h
 * @brief File per la gestione/creazione di gruppi
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef GROUP_H_
#define GROUP_H_

#include <user.h>
#include <abs_list.h>
#include <config.h>


//stati assumibili da una struttura gruppo
typedef enum {
    ACTIVE    = 0,   //il gruppo è 'attivo'
    DELETION  = 1,   //il gruppo è in fase di cancellazione
} status_gr_t;


/**
 * @struct group_t
 * @brief Struttura dati gruppo
 * 
 * @var groupname  nome del gruppo
 * @var creator    nome dell'utente che ha creato il gruppo
 * @var status     indica se il gruppo è attivo o in fase di cancellazione
 * @var members    lista degli utenti membri del gruppo
 * @var mtx        puntatore al mutex per controllare l'accesso alla struttura del gruppo
 */
typedef struct group {
    char             groupname[MAX_NAME_LENGTH+1];
    char             creator[MAX_NAME_LENGTH+1];
    status_gr_t      status;
    list_t           *members;
    pthread_mutex_t  *mtx;
} group_t;



/* ---------------------------- interfaccia group ------------------------- */

/* ------------- funzioni strettamente legate alla strutture 'group_t' ---------------- */

/**
 * @function create_group
 * @brief Crea ed alloca la memoria necessaria per un gruppo. Iscrive l'utente
 *        creatore al gruppo
 * 
 * @param name   nome del gruppo da creare
 * @param user   utente creatore del gruppo
 * 
 * @return p puntatore al nuovo gruppo, NULL in caso di fallimento (errno settato)
 */
group_t *create_group(char *name, user_t *user);


/**
 * @function clean_group
 * @brief Libera la memoria allocata per il gruppo
 * 
 * @param group puntatore al gruppo da cancellare
 */
void clean_group(void *gr);


/**
 * @function disable_group
 * @brief Pone a 'cancellazione' lo stato del gruppo ed lo rimuove da tutti i gruppi
 *        d'iscrizione dei propri membri
 * 
 * @param group      gruppo da 'disabilitare'
 * @param user_name  nome dell'utente che ha richiesto la cancellazione del gruppo
 * 
 * @return 1 se successo, 0 se era già in fase di cancellazione il gruppo o se l'utente 
 *         che ha fatto richiesta non è il creatore del gruppo, -1 se in caso di errore
 */
int disable_group(group_t *group, char* user_name);


/**
 * @function add_member
 * @brief Aggiunge un utente tra i membri del gruppo
 * 
 * @param group   gruppo in cui aggiungere l'utente
 * @param user    utente da aggiungere al gruppo
 * 
 * @return 1 in caso di successo, 0 se l'utente è già membro del gruppo
 *         o se il gruppo è in fase di cancellazione, -1 in caso di errore
 */
int add_member(group_t *group, user_t *user);


/**
 * @function remove_member
 * @brief Rimuove un utente tra i membri del gruppo
 * 
 * @param group       gruppo da cui rimuovere l'utente
 * @param name        nome dell' utente da rimuovere dal gruppo
 * @param is_creator  per sapere all'esterno se l'utente rimosso dal gruppo era
 *                    il creatore di esso
 * 
 * @return puntatore all'utente rimosso, NULL ed errno non settato se non esiste 
 *         nessun membro con tale nome o se il gruppo è in fase di cancellazione,
 *         NULL ed errno settato se errore
 */
user_t *remove_member(group_t *group, char *name, int *is_creator);


/**
 * @function postmsg_all_group
 * @brief Invia il messaggio param->msg_to_send a tutti i membri nel gruppo
 *        
 * @param group   puntatore al gruppo
 * @param param   struttura in cui ci sono i dati da aggiornare/usare per la funzione
 * 
 * @return 1 in caso di successo, 0 se il gruppo è in fase di cancellazione,
 *        -1 in caso di errore
 */
int postmsg_all_group(group_t *group, param_postmsg_all_t *param);



/*------------ funzioni per la creazione/gestione/uso di liste ed hashtable -----------------
----------------- generiche che hanno come elementi dei gruppi 'group_t' ------------------------*/

/**
 * @function setmutex_group
 * @brief Assegna la mutex passata alla struttura gruppo passata
 * 
 * @param gr     gruppo a cui assegnare la mutex
 * @param mutex  mutex da assegnare
 * 
 * @note: setta errno in caso di errore
 */
void setmutex_group(void *gr, pthread_mutex_t *mutex);


/**
 * @function hashfun_group
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
int hashfun_group(int dim, void *name);


/**
 * @function cmp_group
 * @brief Compara il nome di un gruppo con un altro nome
 * 
 * @param gr     gruppo di cui comparare il nome 
 * @param name   nome da comparare con il nome di gr
 * 
 * @return un valore < 0 se (group->groupname < name),
 *         un valore = 0 se (group->groupname = name),
 *         un valore > 0 se (group->groupname > name),
 *         -1 ed errno settato in caso di errore
 */
int cmp_group(void *gr, void *name);


/**
 * @function gen_remove_member
 * @brief Rimuove l'utente passato dai membri del gruppo
 * 
 * @param gr   puntatore al gruppo
 * @param us   puntatore all' utente che vuole essere rimosso dal gruppo
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int gen_remove_member(void *gr, void *us);


#endif /* GROUP_H_ */