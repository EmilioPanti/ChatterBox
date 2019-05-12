/**
 * @file group.c
 * @brief File contenente l'implementazioni delle funzioni dichiarate in group.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <string.h>
#include <stdlib.h>

#include <error_handler.h>
#include <group.h>
#include <user.h>


//funzioni necessarie per la creazione e la gestione della lista 
//degli utenti membri del gruppo
extern int cmp_user_by_name(void *us, void *name);
extern int postmsg_all(void *us, void *param);
extern int gen_unsubscribe(void *us, void *str);


/* ------------------------- funzioni di utilità ------------------------- */

/**
 * @function lock_group
 * @brief prende la lock del gruppo
 * 
 * @param group puntatore al gruppo
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
static int lock_group(group_t *group){ 
    return pthread_mutex_lock(group->mtx);
}


/**
 * @function unlock_group
 * @brief rilascia la lock del gruppo
 * 
 * @param group puntatore al gruppo
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
static int unlock_group(group_t *group){ 
    return pthread_mutex_unlock(group->mtx);
}



/* ------------------------- implementazione interfaccia group ------------------------- */

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
group_t *create_group(char *name, user_t *user){
    //controllo gli argomenti
    err_check_return(name == NULL, EINVAL, "create_group", NULL);
    err_check_return(user == NULL, EINVAL, "create_group", NULL);

    //alloco la memoria per il gruppo
    group_t *gr = malloc(sizeof(group_t));
    err_return_msg(gr,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri del gruppo
    gr->status   = ACTIVE;
    gr->mtx      = NULL;
    strncpy(gr->groupname, name, MAX_NAME_LENGTH+1);
    strncpy(gr->creator, user->nickname, MAX_NAME_LENGTH+1);
    gr->members = init_list(DEFAULT_LEN, NULL, NULL, cmp_user_by_name);
    err_return_msg_clean(gr->members, NULL, NULL, "Errore: init_list\n", clean_group(gr));

    //aggiungo il creatore ai membri del gruppo
    int check = add_data(gr->members, user, user->nickname);
    err_return_msg_clean(check, -1, NULL, "Errore: add_data\n", clean_group(gr));

    return gr;
}


/**
 * @function clean_group
 * @brief Libera la memoria allocata per il gruppo
 * 
 * @param group puntatore al gruppo da cancellare
 */
void clean_group(void *gr){
    if (gr == NULL) return;
    group_t *group = (group_t *)gr;
    if (group->members != NULL) clean_list(group->members);
    free(group);
}


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
int disable_group(group_t *group, char* user_name){
    //controllo gli argomenti
    err_check_return(group == NULL, EINVAL, "disable_group", -1);
    err_check_return(user_name == NULL, EINVAL, "disable_group", -1);

    //variabile di appoggio
    int check = 0, checklock = 0;

    //metto lo status del gruppo in 'cancellazione' se l'utente è il creatore
    checklock = lock_group(group);
    err_check_return(checklock != 0, checklock, "lock_group", -1);

    if(strncmp(group->creator, user_name, MAX_NAME_LENGTH) == 0) {
        if (group->status != DELETION) {
            group->status = DELETION;
            check = 1;
        }
    }

    checklock = unlock_group(group);
    err_check_return(checklock != 0, checklock, "unlock_group", -1);

    //se l'utente passato non è il creatore o se il gruppo è già in fase di cancellazione
    if (check == 0) return 0;

    //rimuovo il gruppo da tutti i gruppi d'iscrizione dei propri membri
    check = apply_fun_param(group->members, gen_unsubscribe, (void *)group->groupname);
    err_return_msg(check,-1,-1,"Errore: disable user\n");

    return 1;
}


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
int add_member(group_t *group, user_t *user){
    //controllo gli argomenti
    err_check_return(group == NULL, EINVAL, "add_member", -1);
    err_check_return(user == NULL, EINVAL, "add_member", -1);

    //variabile di appoggio
    int check = 0, checklock = 0;

    checklock = lock_group(group);
    err_check_return(checklock != 0, checklock, "lock_group", -1);

    //se è inattivo non faccio niente
    if (group->status == DELETION) {
        checklock = unlock_group(group);
        err_check_return(checklock != 0, checklock, "unlock_group", -1);
        return 0;
    }

    check = add_data(group->members, user, user->nickname);

    checklock = unlock_group(group);
    err_check_return(checklock != 0, checklock, "unlock_group", -1);

    return check;
}


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
user_t *remove_member(group_t *group, char *name, int *is_creator){
    //controllo gli argomenti
    err_check_return(group == NULL, EINVAL, "remove_member", NULL);
    err_check_return(name == NULL, EINVAL, "remove_member", NULL);
    err_check_return(is_creator == NULL, EINVAL, "remove_member", NULL);

    //variabile di appoggio
    user_t *us = NULL;
    *is_creator = 0;

    int checklock = lock_group(group);
    err_check_return(checklock != 0, checklock, "lock_group", NULL);

    //se è inattivo non faccio niente
    if (group->status == DELETION) {
        checklock = unlock_group(group);
        err_check_return(checklock != 0, checklock, "unlock_group", NULL);
        return NULL;
    }

    //rimuovo l'utente dalla lista dei membri
    us = remove_data(group->members, name);

    //se l'utente rimosso è il creatore del gruppo
    if(strncmp(group->creator, name, MAX_NAME_LENGTH) == 0) *is_creator = 1;

    checklock = unlock_group(group);
    err_check_return(checklock != 0, checklock, "unlock_group", NULL);

    return us;
}


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
int postmsg_all_group(group_t *group, param_postmsg_all_t *param){
    //controllo gli argomenti
    err_check_return(group == NULL, EINVAL, "postmsg_all_group", -1);
    err_check_return(param == NULL, EINVAL, "postmsg_all_group", -1);

    //variabile di appoggio
    int check = -1, checklock = 0;

    checklock = lock_group(group);
    err_check_return(checklock != 0, checklock, "lock_group", -1);

    //se è inattivo non faccio niente
    if (group->status == DELETION) {
        checklock = unlock_group(group);
        err_check_return(checklock != 0, checklock, "unlock_group", -1);
        return 0;
    }

    //mando il messaggio a tutti gli utenti membri
    if (apply_fun_param(group->members, postmsg_all, (void*)param) == 0) check = 1;

    checklock = unlock_group(group);
    err_check_return(checklock != 0, checklock, "unlock_group", -1);

    return check;
}



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
void setmutex_group(void *gr, pthread_mutex_t *mutex){
    //controllo gli argomenti
    if (gr == NULL) {
        errno = EINVAL; 
        perror("setmutex_group"); 
        return;
    }
    group_t *group = (group_t *)gr;
    group->mtx = mutex;
}


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
int hashfun_group(int dim, void *name){
    //controllo gli argomenti
    err_check_return(dim < 0, EINVAL, "hashfun_group", -1);
    err_check_return(name == NULL, EINVAL, "hashfun_group", -1);

    char *str = (char *)name;
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    
    return hash % dim;
}


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
int cmp_group(void *gr, void *name){
    //controllo gli argomenti
    err_check_return(gr == NULL, EINVAL, "cmp_group", -1);
    err_check_return(name == NULL, EINVAL, "cmp_group", -1);

    group_t *group = (group_t *)gr;
    char *name_gr = (char*)name;
    return strncmp(group->groupname, name_gr, MAX_NAME_LENGTH);
}


/**
 * @function gen_remove_member
 * @brief Rimuove l'utente passato dai membri del gruppo
 * 
 * @param gr   puntatore al gruppo
 * @param us   puntatore all' utente che vuole essere rimosso dal gruppo
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int gen_remove_member(void *gr, void *us){
    //controllo gli argomenti
    err_check_return(gr == NULL, EINVAL, "gen_remove_member", -1);
    err_check_return(us == NULL, EINVAL, "gen_remove_member", -1);

    group_t *group = (group_t*)gr;
    user_t  *user = (user_t*)us;

    //flag che mi indicherà se l'utente che ho rimosso dal gruppo
    //è anche il creatore di esso
    int is_creator = 0;

    //rimuovo l'utente
    errno = 0;
    user_t *u = remove_member(group, user->nickname, &is_creator);
    //se errore
    if (u == NULL && errno != 0) return -1;

    //se l'utente rimosso dal gruppo non era il creatore di esso
    //elimino il gruppo dalla sua lista di gruppi
    //l'intento è quello di far rimanere nella lista dei gruppi 
    //solo i gruppi di cui è anche il creatore 
    //(si presuppone che questa funzione sia chiamata al momento
    //della de-registrazione dell'utente)
    if (is_creator == 0) {
        void *v = remove_data(user->groups, (void*)group->groupname);
        //in caso di errore
        if (v == NULL && errno != 0) return -1;
    }

    return 0;
}