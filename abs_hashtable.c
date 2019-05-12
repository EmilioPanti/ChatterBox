/**
 * @file abs_hashtable.c
 * @brief Implementazione delle funzioni dichiarate in abs_hashtable.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#define _POSIX_C_SOURCE 200809L
#include <error_handler.h>
#include <abs_hashtable.h>
#include <unistd.h>
#include <stdlib.h>

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
                            ) {
    //controllo gli argomenti 
    err_check_return(num_mutex < 0, EINVAL, "init_hashtable", NULL);
    err_check_return(factor < 0, EINVAL, "init_hashtable", NULL);
    err_check_return(compare_data == NULL, EINVAL, "init_hashtable", NULL);
    err_check_return(hash_fun == NULL, EINVAL, "init_hashtable", NULL);

    //alloco la memoria per la tabella hash degli utenti
    hashtable_t *ht = malloc(sizeof(hashtable_t));
    err_return_msg(ht,NULL,NULL,"Errore: malloc\n");

    ht->dim           = factor * num_mutex;
    ht->n_mtx         = num_mutex;
    ht->factor        = factor;
    ht->mtx_array     = NULL;
    ht->lists         = NULL;
    ht->clean_data    = clean_data;
    ht->compare_data  = compare_data;
    ht->hash_fun      = hash_fun;
    ht->setmutex_data = setmutex_data;

    //variabili di appoggio
    int i = 0, j = 0, checkmutex = 0;

    //creo l'array di mutex
    ht->mtx_array = malloc((ht->n_mtx)*sizeof(pthread_mutex_t));
    if (ht->mtx_array == NULL){
        clean_hashtable(ht);
        fprintf(stderr,"Errore: malloc\n");
        return NULL;
    }
    //creo l'attributo per le mutex ricorsive
    pthread_mutexattr_t   mta;
    checkmutex = pthread_mutexattr_init(&mta);
    if (checkmutex == 0) checkmutex = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
    if (checkmutex != 0){
        errno = checkmutex;
        clean_hashtable(ht);
        perror("pthread_mutexattr");
        return NULL;
    }
    //inizializzo l'array di mutex
    for (i = 0; i < ht->n_mtx; i++){
        checkmutex = pthread_mutex_init(&(ht->mtx_array)[i], &mta);
        if (checkmutex != 0){
            errno = checkmutex;
            clean_hashtable(ht);
            perror("pthread_mutex_init");
            return NULL;
        }
    }

    //creo l'array di liste data
    ht->lists = malloc((ht->dim)*sizeof(list_t*));
    if (ht->lists == NULL){
        clean_hashtable(ht);
        fprintf(stderr,"Errore: malloc\n");
        return NULL;
    }
    //inizializzo l'array di liste
    for (i = 0; i < ht->dim; i++) ht->lists[i] = NULL;
    //creo le liste data
    for (i = 0; i < ht->n_mtx; i++){
        for (j = 0; j < ht->factor; j++){
            int ind = (i * ht->factor) + j;
            ht->lists[ind] = init_list(DEFAULT_LEN, &(ht->mtx_array)[i], ht->clean_data, ht->compare_data);
            if (ht->lists[ind] == NULL){
                clean_hashtable(ht);
                return NULL;
            }
        }
    }

    return ht;
}


/**
 * @function clean_hashtable
 * @brief libera la memoria allocata per la hash table
 * 
 * @param ht puntatore alla tabella hash da cancellare
 */
void clean_hashtable(hashtable_t *ht){
    if(ht == NULL) return;

    //dealloco l'array di mutex
    if (ht->mtx_array != NULL) free(ht->mtx_array);

    //cancello le liste data
    if (ht->lists != NULL) {
        for (int i = 0; i < ht->dim; i++){
            if (ht->lists[i] != NULL) clean_list(ht->lists[i]);
        }
        free(ht->lists);
    }

    free(ht);
}


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
int add_data_ht(hashtable_t *ht, void *data, void *param){
    //controllo gli argomenti
    err_check_return(ht == NULL, EINVAL, "add_data_ht", -1);
    err_check_return(data == NULL, EINVAL, "add_data_ht", -1);
    err_check_return(param == NULL, EINVAL, "add_data_ht", -1);

    int pos = ht->hash_fun(ht->dim, param);
    err_return_msg(pos,-1,-1,"Errore: hash_fun\n");

    errno = 0;
    // setto la mutex per l'oggetto data (la stessa della lista
    // in cui verrà inserito)
    if (ht->setmutex_data != NULL) {
        pthread_mutex_t *mutex = get_mutex_list(ht->lists[pos]);
        if (mutex == NULL && errno != 0) return -1;
        ht->setmutex_data(data, mutex);
        if (errno != 0) return -1;
    }
    
    //aggiungo data nella lista di competenza
    return add_data(ht->lists[pos], data, param);
}


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
int remove_data_ht(hashtable_t *ht, void *param){
    //controllo gli argomenti
    err_check_return(ht == NULL, EINVAL, "remove_data_ht", -1);
    err_check_return(param == NULL, EINVAL, "remove_data_ht", -1);

    int pos = ht->hash_fun(ht->dim, param);
    err_return_msg(pos,-1,-1,"Errore: hash_fun\n");

    errno = 0;
    void *data = remove_data(ht->lists[pos], param);

    //se errore
    if(data == NULL && errno != 0) return -1;
    //se l'oggetto data ricercato non esiste
    else if(data == NULL && errno == 0) return 0;
    //se esiste
    else {
        if (ht->clean_data != NULL) ht->clean_data(data);
        return 1;
    }
}


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
 *         NULL ed errno settato in caso di errore 
 */
void *search_data_ht(hashtable_t *ht, void *param){
    //controllo gli argomenti
    err_check_return(ht == NULL, EINVAL, "search_data_ht", NULL);
    err_check_return(param == NULL, EINVAL, "search_data_ht", NULL);

    int pos = ht->hash_fun(ht->dim, param);
    err_return_msg(pos,-1,NULL,"Errore: hash_fun\n");

    return search_data(ht->lists[pos], param);
}


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
int apply_fun_ht(hashtable_t *ht, int (* fun )(void *)) {
    //controllo gli argomenti
    err_check_return(ht == NULL, EINVAL, "apply_fun_ht", -1);
    err_check_return(fun == NULL, EINVAL, "apply_fun_ht", -1);

    //variabili di appoggio
    int i = 0, j = 0, ind = 0, check = 0;

    //scorro tutte le liste data
    for (i = 0; i < ht->n_mtx; i++){
        //lock sulla mutex 
        check = pthread_mutex_lock(&(ht->mtx_array)[i]);
        err_check_return(check != 0, check, "pthread_mutex_lock", -1);

        //per tutte le liste data che hanno la mutex lockata sopra...
        for (j = 0; j < ht->factor; j++){
            ind = (i * (ht->factor)) + j;
            check = apply_fun(ht->lists[ind], fun);
            if (check == -1){
                pthread_mutex_unlock(&(ht->mtx_array)[i]);
                return -1;
            }
        }

        //unlock della mutex
        check = pthread_mutex_unlock(&(ht->mtx_array)[i]);
        err_check_return(check != 0, check, "pthread_mutex_unlock", -1);
    }

    return 0;
}


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
int apply_fun_param_ht(hashtable_t *ht, int (* fun )(void *, void *), void *fun_param){
    //controllo gli argomenti
    err_check_return(ht == NULL, EINVAL, "apply_fun_param_ht", -1);
    err_check_return(fun == NULL, EINVAL, "apply_fun_param_ht", -1);
    err_check_return(fun_param == NULL, EINVAL, "apply_fun_param_ht", -1);

    //variabili di appoggio
    int i = 0, j = 0, ind = 0, check = 0;

    //scorro tutte le liste data
    for (i = 0; i < ht->n_mtx; i++){
        //lock sulla mutex 
        check = pthread_mutex_lock(&(ht->mtx_array)[i]);
        err_check_return(check != 0, check, "pthread_mutex_lock", -1);

        //per tutte le liste data che hanno la mutex lockata sopra...
        for (j = 0; j < ht->factor; j++){
            ind = (i * (ht->factor)) + j;
            check = apply_fun_param(ht->lists[ind], fun, fun_param);
            if (check == -1){
                pthread_mutex_unlock(&(ht->mtx_array)[i]);
                return -1;
            }
        }

        //unlock della mutex
        check = pthread_mutex_unlock(&(ht->mtx_array)[i]);
        err_check_return(check != 0, check, "pthread_mutex_unlock", -1);
    }

    return 0;
}