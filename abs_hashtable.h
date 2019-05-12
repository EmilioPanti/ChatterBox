/**
 * @file abs_hashtable.h
 * @brief File per la gestione/creazione di tabelle hash per dati generici
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef ABS_HASHTABLE_H_
#define ABS_HASHTABLE_H_

#include <abs_list.h>


/**
 * @struct hash_t
 * @brief Gestore della hash table
 * 
 * @var dim           dimensione della tabella (quante liste dati contiene)
 * @var n_mtx         numero di mutex da allocare ed inserire in mtx_array
 * @var factor        numero di liste dati da assegnare ad ogni mutex
 * @var lists         vettore delle liste dati
 * @var mtx_array     vettore delle mutex utilizzate
 * @clean_data        funzione per eliminare un elemento data 
 * @compare_data      funzione per comparare un elemento data della tabella hash con
 *                    un parametro di confronto (può essere qualsiasi cosa)
 * @param hash_fun    funzione che calcola l'indice della tabella hash
 * @setmutex_data     funzione per settare la mutex degli elementi data che contiene la tabella hash,
 *                    un elemento data avrà la stessa mutex della lista in cui verrà inserito
 * 
 * @note: ogni elemento della tabella hash è costituito da una lista,
 *        la mtx assegnata alla lista dipende dalla sua posizione in tabella.
 */
typedef struct {
    int              dim;
    int              n_mtx;
    int              factor;
    list_t           **lists;
    pthread_mutex_t  *mtx_array;
    void (* clean_data )(void *);
    int  (* compare_data )(void *element, void *param);
    int (* hash_fun)(int dim_hashtable, void *param);
    void (* setmutex_data )(void *, pthread_mutex_t *);
} hashtable_t;



/**
 * @function init_hashtable
 * @brief Inizializza la tabella hash per dati generici
 * 
 * @param num_mutex     numero di mutex da usare nella tabella hash.
 * @param factor        liste di trabocco da assegnare ad ogni mutex
 * @param clean_data    funzione da chiamare per eliminare gli elementi di tipo data
 *                      contenuti nella tabella hash, se viene passato NULL invece  
 *                      non verrà applicata nessuna funzione di pulizia per gli 
 *                      elementi data al momento della eliminazione della struttura
 * @param compare_data  funzione per comparare un elemento interno ad una lista della tabella hash
 *                      con un parametro di confronto (non per forza un altro elemento della lista).
 *                      La funzione data deve garantire che la chiamata 
 *                      compare_data(element, params) restituisca:
 *                       - un valore < 0 se (element < param),
 *                       - un valore = 0 se (element = param),
 *                       - un valore > 0 se (element > param)
 * @param hash_fun      funzione che calcola l'indice della tabella hash per il relativo param
 *                      passato, la tabella hash passerà la sua dimensione quando chiamerà
 *                      questa funzione
 * @param setmutex_data funzione da chiamare per assegnare la mutex all'elemento data al 
 *                      momento del suo inserimento nella tabella hash, se viene passato NULL
 *                      non verrà chiamata nessuna funzione per settare la mutex 
 * 
 * @return p puntatore alla nuova tabella hash, NULL in caso di fallimento
 * 
 * @note: la tabella hash avrà una dimensione totale (ovvero di liste dati) = num_mutex * factor
 * @note: le funzioni compare_data(element, param) e hash_fun(dim_hashtable, param) devono utlizzare
 *        ed interpretare param allo stesso modo. Altrimenti non è garantito un uso corretto della
 *        tabella hash
 */
hashtable_t *init_hashtable(int num_mutex, int factor, void (* clean_data )(void *), 
                            int (* compare_data )(void *element, void *param),
                            int (* hash_fun)(int dim_hashtable, void *param),
                            void (* setmutex_data )(void *, pthread_mutex_t *)
                            );


/**
 * @function clean_hashtable
 * @brief libera la memoria allocata per la hash table
 * 
 * @param ht puntatore alla tabella hash da cancellare
 */
void clean_hashtable(hashtable_t *ht);


/**
 * @function add_data_ht
 * @brief Inserisce un elemento data nella tabella hash 
 * 
 * @param ht    puntatore alla tabella hash
 * @param data  puntatore all'elemento da inserire
 * @param param puntatore al parametro che verrà utilizzato nelle
 *              funzioni compare_data(element, param) e hash_fun(dim_hashtable, param)
 * 
 * @return 1 se data è stato inserito,
 *         0 se data è già presente nella tabella hash,
 *         -1 in caso di errore e errno settato
 */
int add_data_ht(hashtable_t *ht, void *data, void *param);


/**
 * @function remove_data_ht
 * @brief Rimuove un elemento data dalla tabella hash
 * 
 * @param ht     puntatore alla tabella hash
 * @param param  puntatore al parametro che verrà utilizzato nelle
 *               funzioni compare_data(element, param) per trovare l' elemento
 *               da rimuovere e hash_fun(dim_hashtable, param)
 * 
 * @return 1 se successo, 0 se non è stato rimosso nessun elemento, -1 in caso di errore
 */
int remove_data_ht(hashtable_t *ht, void *param);


/**
 * @function search_data_ht
 * @brief Cerca e restituisce l'elemento el nella tabella hash
 *        per cui vale compare_data(el, param) = 0 , con param
 *        puntatore passato dall'esterno
 * 
 * @param ht     puntatore alla tabella hash
 * @param param  puntatore al parametro che verrà utilizzato nelle
 *               funzioni compare_data(element, param) per trovare l' elemento
 *               da rimuovere e hash_fun(dim_hashtable, param)
 * 
 * @return puntatore all'elemento cercato in caso di successo,
 *         NULL se tale elemento non è presente (errno non modificato),
 *         NULL in caso di errore (ERRNO MODIFICATO)
 */
void *search_data_ht(hashtable_t *ht, void *param);


/**
 * @function apply_fun_ht
 * @brief Applica la funzione passata a tutti gli elementi presenti 
 *        nella tabella hash
 * 
 * @param ht    puntatore alla tabella hash
 * @param fun   funzione da applicare agli elementi
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 * 
 * @note: la funzione fun passata deve ritornare -1 in caso di errore
 */
int apply_fun_ht(hashtable_t *ht, int (* fun )(void *));


/**
 * @function apply_fun_param_ht
 * @brief Applica la funzione passata a tutti gli elementi presenti 
 *        nella tabella hash
 * 
 * @param ht          puntatore alla tabella hash
 * @param fun         funzione da applicare agli elementi
 * @param fun_param   puntatore alla  struttura che contiene i parametri necessari a fun
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 * 
 * @note: la funzione fun passata deve ritornare -1 in caso di errore
 */
int apply_fun_param_ht(hashtable_t *ht, int (* fun )(void *, void *), void *fun_param);


#endif /* ABS_HASHTABLE_H_ */