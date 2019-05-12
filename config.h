/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file config.h
 * @brief File contenente alcune define con valori massimi utilizzabili
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define MAX_NAME_LENGTH                  32

/* aggiungere altre define qui */
//max caratteri letti per ogni riga del file di configurazione
#define MAX_LINE                       1024 
//massimo numero threads nel pool
#define MAXTHREADS                      100

#define DEBUG


/**
 * @struct configs_t
 * @brief Struttura che contiene la configurazione del server
 * 
 * @var socket_path    path utilizzato per la creazione del socket AF_UNIX
 * @var max_           conn numero massimo di connessioni
 * @var threads        numero di thread nel pool
 * @var max_msg_size   dimensione massima di un messaggio
 * @var max_file_size  dimensione massima di un file
 * @var max_hist_msg   numero massimo di messaggi che il server 'ricorda' per ogni client
 * @var dir_name       directory per memorizzare i file inviati dagli utenti
 * @var stat_file_name file nel quale verranno scritte le statistiche del server
 */
typedef struct{
    char         *socket_path;          
    unsigned int max_conn;     
    unsigned int threads;             
    unsigned int max_msg_size;        
    unsigned int max_file_size;        
    unsigned int max_hist_msg;        
    char         *dir_name;           
    char         *stat_file_name; 
}configs_t;


/**
 * @function configura_server
 * @brief Salva in una struttura apposita le configurazioni specificate da un file 
 * 
 * @param file_name    nome del file di configurazione
 * @param conf_server  struttura in cui salvo le configurazioni specificate in file_name
 * 
 * @return 0 in caso di successo, -1 in caso di fallimento
 */
int configura_server(char *file_name, configs_t *conf_server);


/**
 * @function clean_configs
 * @brief Libera la memoria occupata da conf_server
 * 
 * @param conf_server struttura da liberare
 */
void clean_configs(configs_t *conf_server);


// to avoid warnings like "ISO C forbids an empty translation unit"
typedef int make_iso_compilers_happy;

#endif /* CONFIG_H_ */
