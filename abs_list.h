/**
 * @file abs_list.h
 * @brief File per la gestione/creazione di liste generiche con varie possibilità di politiche di 
 *        ordinamento.
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef ABS_LIST_H_
#define ABS_LIST_H_

#include <pthread.h>

//numero di elementi in lista di default
#define  DEFAULT_LEN   1024

/**
 * @struct node_t
 * @brief Nodo della lista
 * 
 * @var data  puntatore al dato da inserire
 * @var next  puntatore al successivo nodo della lista
 */
typedef struct node {
    void         *data;
    struct node  *next;
} node_t;


/**
 * @struct list_t
 * @brief Gestore della lista
 * 
 * @var max_len    numero massimo di elementi in lista
 * @var len        numero di elementi in lista
 * @var head       puntatore al primo nodo della lista
 * @var tail       puntatore all' ultimo nodo della lista
 * @var mtx        puntatore alla mutex per controllare l'accesso alla lista
 * @clean_data     funzione per eliminare un elemento data
 * @compare_data   funzione per comparare un elemento data della lista con
 *                 un parametro di confronto (può essere qualsiasi cosa)
 */
typedef struct {
    unsigned int     max_len;
    unsigned int     len;
    node_t           *head;
    node_t           *tail;
    pthread_mutex_t  *mtx;
    void (* clean_data )(void *);
    int  (* compare_data )(void *el, void *param_to_compare);
} list_t;



/* ---------------------- interfaccia della lista  --------------------- */

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
                    int (* compare_data )(void *el, void *param_to_compare));


/**
 * @function clean_list
 * @brief libera la memoria allocata per la lista passata da parametro
 * 
 * @param list  puntatore alla lista da cancellare
 */
void clean_list(list_t *list);


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
int add_data(list_t *list, void *data, void *param_to_cmp);


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
void *remove_data(list_t *list, void *param_to_cmp);


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
void *search_data(list_t *list, void *param_to_cmp);


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
void *pop_data(list_t *list);


/**
 * @function get_mutex_list
 * @brief Ritorna il valore della mutex della lista
 * 
 * @param list  puntatore alla lista
 * 
 * @return puntatore alla mutex della lista,,
 *         NULL ed errno modificato se errore
 */
pthread_mutex_t *get_mutex_list(list_t *list);


/**
 * @function get_len_list
 * @brief Ritorna la lunghezza della lista
 * 
 * @param list  puntatore alla lista
 * 
 * @return lunghezza della lista, -1 in caso di errore
 */
int get_len_list(list_t *list);


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
int apply_fun(list_t *list, int (* fun )(void *));


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
int apply_fun_param(list_t *list, int (* fun )(void *, void * ), void *fun_param);


#endif /* ABS_LIST_H_ */