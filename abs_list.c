/**
 * @file abs_list.c
 * @brief File contenente l'implementazioni delle funzioni dichiarate in abs_list.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <unistd.h>
#include <error_handler.h>
#include <abs_list.h>
#include <stdlib.h>


/* ------------------- funzioni di utilita' -------------------- */

/**
 * @function lock_list
 * @brief prende la lock della lista
 * 
 * @param list puntatore alla lista
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore 
 */
static int lock_list(list_t *list){ 
    if (list->mtx != NULL) return pthread_mutex_lock(list->mtx);
    return 0;
}


/**
 * @function unlock_list
 * @brief rilascia la lock della lista
 * 
 * @param list puntatore alla lista
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore 
 */
static int unlock_list(list_t *list){ 
    if (list->mtx != NULL) return pthread_mutex_unlock(list->mtx);
    return 0;
}



/* ---------------------- implementazione interfaccia della lista  --------------------- */

/**
 * @function init_list
 * @brief Inizializza la lista
 * 
 * @param max_len       massima lunghezza della lista. Quando viene raggiunto il limite massimo
 *                      e viene richiesto un nuovo inserimento:
 *                       - se è stata passata una funzione 'compare_data' non viene inserito
 *                       - se NON è stata passata nessuna funzione di comparazione per gli
 *                         elementi, il nuovo elemento  verrà inserito in fondo alla lista
 *                         mentre quello in testa verrà rimosso.
 * @param mutex         puntatore alla mutex per controllare l'accesso alla lista, se 
 *                      viene passato NULL non viene garantito un accesso in mutua esclusione
 * @param clean_data    funzione da chiamare per eliminare gli elementi di tipo data
 *                      presenti nella lista, se viene passato NULL invece  
 *                      non verrà applicata nessuna funzione di pulizia per gli 
 *                      elementi data al momento della eliminazione della lista
 * @param compare_data  funzione per comparare un elemento della lista con un parametro
 *                      di confronto (può essere un altro elemento della lista come altro).
 *                      La funzione data deve garantire che la chiamata 
 *                      compare_data(el, param_to_compare) restituisca:
 *                       - un valore < 0 se (el < param_to_compare),
 *                       - un valore = 0 se (el = param_to_compare),
 *                       - un valore > 0 se (el > param_to_compare)
 *                      La lista verrà ordinata in base a questa funzione di comparazione,
 *                      se viene passato un valore NULL per questo parametro i nuovi 
 *                      elementi saranno inseriti in fondo alla lista indistintamente.
 * 
 * @return p puntatore alla nuova lista, NULL in caso di fallimento
 */
list_t *init_list(int max_len, pthread_mutex_t  *mutex, void (* clean_data )(void *) , 
                    int (* compare_data )(void *el, void *param_to_compare)){
    //controllo gli argomenti
    err_check_return(max_len < 1, EINVAL, "init_list", NULL);

    //alloco la memoria per la lista
    list_t *list = malloc(sizeof(list_t));
    err_return_msg(list,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri della lista
    list->max_len = max_len;
    list->len     = 0;
    list->head    = NULL;
    list->mtx     = mutex;
    list->clean_data   = clean_data;
    list->compare_data = compare_data;

    return list;
}


/**
 * @function clean_list
 * @brief libera la memoria allocata per la lista passata da parametro
 * 
 * @param list  puntatore alla lista da cancellare
 */
void clean_list(list_t *list){
    if (list == NULL) return;

    //variabile di appoggio
    node_t *curr = list->head;

    //cancello tutti i nodi della lista
    while(curr != NULL) {
        list->head = curr->next;
        //se la funzione clean_data è definita la chiamo 
        if (list->clean_data != NULL) list->clean_data(curr->data);
        free(curr);
        curr = list->head;
    }

    //cancello la lista
    free(list);
}


/**
 * @function add_data
 * @brief Inserisce un dato nella lista
 * 
 * @param list          puntatore alla lista
 * @param data          puntatore al dato da inserire in lista
 * @param param_to_cmp  parametro di confronto per l' inserimento (può essere
 *                      NULL nel caso in cui non sia stata definita nessuna 
 *                      funzione di comparazione al momento della creazione
 *                      della lista)
 * 
 * @return  1 in caso di successo, 
 *         -1 ed errno settato in caso di errore,
 *          0 ed errno NON settato se la funzione 'compare_data' è stata definita e:
 *           - esiste già un elemento uguale a data nella lista
 *           - oppure è stato raggiunto il numero massimo di elementi in lista
 * 
 * @note: l'inserimento avviene nell'ordine deciso dalla funzione 'compare_data' se 
 *        è stata definita, altrimenti il nuovo elemento viene inserito in fondo. Se
 *        è già stato raggiunto il numero massimo di elementi, ma non è stata definita nessuna
 *        funzione 'compare_data', l'elemento in testa alla lista viene cancellato dopo il
 *        nuovo inserimento.
 */
int add_data(list_t *list, void *data, void *param_to_cmp){
    //controllo gli argomenti
    err_check_return(list == NULL, EINVAL, "add_data", -1);
    err_check_return(data == NULL, EINVAL, "add_data", -1);
    err_check_return(param_to_cmp == NULL && list->compare_data != NULL, EINVAL, "add_data", -1);

    //controllo se è stato raggiunto il numero massimo di elementi
    if (list->len == list->max_len) {
        //se la funzione 'compare_data' è definita non faccio niente
        if (list->compare_data != NULL) return 0;
        //se non è definita elimino l'elemento in testa alla lista prima
        //di effettuare il nuovo inserimento
        else {
            node_t *first = list->head;
            //nel caso la lista abbia max_len = 1 
            if (list->tail == list->head) list->tail = NULL;
            list->head = first->next;
            //se la funzione clean_data è definita la chiamo 
            if (list->clean_data != NULL) list->clean_data(first->data);
            free(first);
            list->len--;
        }
    }

    //creo un nuovo nodo
    node_t *new = malloc(sizeof(node_t));
    err_return_msg(new,NULL,-1,"Errore: malloc\n");
    new->data = data;
    new->next = NULL;


    //lock sulla lista
    int check = lock_list(list);
    if (check != 0) free(new);
    err_check_return(check != 0, check, "lock_list", -1);

    //se la lista è vuota
    if(list->head == NULL) {
        //lo inserisco nella lista
        list->head = new;
        list->tail = new;
        list->len++;
    }
    //se la lista ha almeno un elemento
    else {
        //se non è stata definita 'compare_data' inserisco direttamente
        //in fondo alla lista
        if (list->compare_data == NULL) {
            //aggiungo new in fondo
            list->tail->next = new;
            list->tail = new;
            list->len++;
        }
        //se invece è stata definita 'compare_data' devo inserire
        //l'elemento in base alla sua politica
        else {
            //variabili di appoggio
            node_t *curr = NULL;
            node_t *prec = NULL;
            int added = 0;

            curr = list->head;
            int ris;

            while (curr != NULL && added == 0) {
                ris = list->compare_data(curr->data, param_to_cmp);
                if (ris > 0) {
                    //lo inserisco nella lista
                    if (prec == NULL) list->head = new;
                    else prec->next = new;
                    new->next = curr;
                    list->len++;
                    added = 1;
                }
                else if (ris == 0) {
                    //Esiste già un elemento data uguale
                    free(new);
                    check = unlock_list(list);
                    err_check_return(check != 0, check, "unlock_list", -1);
                    return 0;
                }
                else {
                    prec = curr;
                    curr = curr->next;
                }
            }

            //se deve essere inserito in fondo alla lista
            if(curr == NULL && added == 0){
                prec->next = new;
                list->tail = new;
                list->len++;
            }
        }
    }
    
    //unlock della lista
    check = unlock_list(list);
    err_check_return(check != 0, check, "unlock_list", -1);

    return 1;
}


/**
 * @function remove_data
 * @brief Rimuove un dato dalla lista utilizzando la funzione 'compare_data' e il 
 *        parametro di confronto fornito
 * 
 * @param list          puntatore alla lista
 * @param param_to_cmp  parametro di confronto per la rimozione
 * 
 * @return puntatore al dato rimosso, NULL se non è stato rimosso nessun dato 
 *         perhè non presente (errno non modificato), NULL ed errno modificato
 *         in caso di errore
 * 
 * @note: per chiamare questa funzione deve essere definita la funzione 'compare_data'
 */
void *remove_data(list_t *list, void *param_to_cmp){
    //controllo gli argomenti
    err_check_return(list == NULL, EINVAL, "remove_data", NULL);
    err_check_return(param_to_cmp == NULL || list->compare_data == NULL, EINVAL, "remove_data", NULL);

    //variabili di appoggio
    node_t *curr = NULL;
    node_t *prec = NULL;
    void   *ret  = NULL;
    int ris, found = 0;

    //lock sulla lista
    int check = lock_list(list);
    err_check_return(check != 0, check, "lock_list", NULL);

    curr = list->head;
    while (curr != NULL && found == 0) {
        ris = list->compare_data(curr->data, param_to_cmp);
        if (ris > 0) {
            //non c'è il dato ricercato, neanche negli elementi successivi
            curr = NULL; //per uscire dal while
        }
        else if (ris == 0) {
            //ho trovato il dato da rimuovere
            if (prec == NULL) {
                if (curr == list->tail) list->tail = NULL;
                list->head = curr->next;
            }
            else {
                if (curr == list->tail) list->tail = prec;
                prec->next = curr->next;
            }
            ret = curr->data;
            free(curr);
            list->len--;
            found = 1;
        }
        else {
            prec = curr;
            curr = curr->next;
        }
    }

    //unlock della lista
    check = unlock_list(list);
    err_check_return(check != 0, check, "unlock_list", NULL);

    return ret;
}


/**
 * @function search_data
 * @brief Cerca e restituisce un dato dalla lista utilizzando la funzione 'compare_data'
 *        e il parametro di confronto fornito
 * 
 * @param list  puntatore alla lista
 * @param name  parametro di confronto per la ricerca
 * 
 * @return puntatore al dato cercato in caso di successo,
 *         NULL se il dato non esiste (errno non è modificato),
 *         NULL ed errno modificato in caso di errore
 * 
 * @note: per chiamare questa funzione deve essere definita la funzione 'compare_data'
 */
void *search_data(list_t *list, void *param_to_cmp){
    //controllo gli argomenti
    err_check_return(list == NULL, EINVAL, "search_data", NULL);
    err_check_return(param_to_cmp == NULL || list->compare_data == NULL, EINVAL, "search_data", NULL);

    //variabili di appoggio
    node_t *curr = NULL;
    void   *ret  = NULL;
    int ris;

    //lock sulla lista
    int check = lock_list(list);
    err_check_return(check != 0, check, "lock_list", NULL);

    curr = list->head;

    while (curr != NULL && ret == NULL) {
        errno = 0;
        ris = list->compare_data(curr->data, param_to_cmp);
        //se errore
        if (errno != 0) return NULL;

        if (ris > 0) {
            //non esiste il dato cercato nemmeno dopo
            curr = NULL; //per uscire dal while
        }
        else if (ris == 0) {
            //ho trovato il dato cercato
            ret = curr->data;
        }
        else {
            curr = curr->next;
        }
    }

    //unlock della lista
    check = unlock_list(list);
    err_check_return(check != 0, check, "unlock_list", NULL);

    return ret;
}


/**
 * @function pop_data
 * @brief Rimuove il primo nodo della lista e restituisce l'elemento contenuto in esso
 * 
 * @param list  puntatore alla lista
 * 
 * @return puntatore al primo dato della lista,
 *         NULL se la lista è vuota (errno non modificato),
 *         NULL ed errno modificato se errore
 */
void *pop_data(list_t *list){
    //controllo gli argomenti
    err_check_return(list == NULL, EINVAL, "pop_data", NULL);

    //variabili di appoggio
    node_t *curr = NULL;
    void   *ret  = NULL;

    //lock sulla lista
    int check = lock_list(list);
    err_check_return(check != 0, check, "lock_list", NULL);

    curr = list->head;

    if(curr != NULL) {
        list->head = curr->next;
        if (list->head == NULL) list->tail = NULL;
        ret = curr->data;
        free(curr);
        list->len--;
    }

    //unlock della lista
    check = unlock_list(list);
    err_check_return(check != 0, check, "unlock_list", NULL);

    return ret;
}


/**
 * @function get_mutex_list
 * @brief Ritorna il valore della mutex della lista
 * 
 * @param list  puntatore alla lista
 * 
 * @return puntatore alla mutex della lista,,
 *         NULL ed errno modificato se errore
 */
pthread_mutex_t *get_mutex_list(list_t *list){
    //controllo gli argomenti
    err_check_return(list == NULL, EINVAL, "get_mutex_list", NULL);
    return list->mtx;
}


/**
 * @function get_len_list
 * @brief Ritorna la lunghezza della lista
 * 
 * @param list  puntatore alla lista
 * 
 * @return lunghezza della lista, -1 in caso di errore
 */
int get_len_list(list_t *list){
    //controllo gli argomenti
    err_check_return(list == NULL, EINVAL, "get_mutex_list", -1);
    return list->len;
}


/**
 * @function apply_fun
 * @brief Applica la funzione passata a tutti gli elementi della lista
 * 
 * @param list  puntatore alla lista
 * @param fun   funzione da applicare agli elementi
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 * 
 * @note: la funzione fun passata deve ritornare -1 in caso di errore
 */
int apply_fun(list_t *list, int (* fun )(void *)){
    //controllo gli argomenti
    err_check_return(list == NULL, EINVAL, "apply_fun", -1);
    err_check_return(fun == NULL, EINVAL, "apply_fun", -1);

    //variabili di appoggio
    node_t *curr = NULL;

    //lock sulla lista
    int check = lock_list(list);
    err_check_return(check != 0, check, "lock_list", -1);

    curr = list->head;

    while (curr != NULL) {
        if (fun(curr->data) == -1) {
            //si è verificato un errore 
            unlock_list(list);
            return -1;
        }
        curr = curr->next;
    }

    //unlock della lista
    check = unlock_list(list);
    err_check_return(check != 0, check, "unlock_list", -1);

    return 0;
}


/**
 * @function apply_fun_param
 * @brief Applica la funzione passata a tutti gli elementi della lista
 * 
 * @param list        puntatore alla lista
 * @param fun         funzione da applicare agli elementi
 * @param fun_param   struttura che contiene i parametri necessari a fun
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 * 
 * @note: la funzione fun passata deve ritornare -1 in caso di errore
 */
int apply_fun_param(list_t *list, int (* fun )(void *, void * ), void *fun_param){
    //controllo gli argomenti
    err_check_return(list == NULL, EINVAL, "apply_fun_param", -1);
    err_check_return(fun == NULL, EINVAL, "apply_fun_param", -1);
    err_check_return(fun_param == NULL, EINVAL, "apply_fun_param", -1);

    //variabili di appoggio
    node_t *curr = NULL;

    //lock sulla lista
    int check = lock_list(list);
    err_check_return(check != 0, check, "lock_list", -1);

    curr = list->head;

    while (curr != NULL) {
        if (fun(curr->data, fun_param) == -1) {
            //si è verificato un errore 
            unlock_list(list);
            return -1;
        }
        curr = curr->next;
    }

    //unlock della lista
    check = unlock_list(list);
    err_check_return(check != 0, check, "unlock_list", -1);

    return 0;
}