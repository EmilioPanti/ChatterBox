/**
 * @file files_handler.h
 * @brief File contenente funzioni per la gestione dei files spediti/richiesti dagli utenti
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#if !defined(FILES_HANDLER_)
#define FILES_HANDLER_


/**
 * @function get_filename
 * @brief Restituisce solo il nome del file dal suo path completo
 * 
 * @param pathfile  path del file
 * 
 * @return filename se successo, NULL in caso di fallimento (o file non valido)
 */
char* get_filename(char *pathfile);


/**
 * @function save_file
 * @brief Salva il file passato da parametro nella directory indicata
 * 
 * @param dir_file    directory in cui salvare il file
 * @param file_name   nome del file da salvare
 * @param buf         puntatore al buffer dati da scrivere nel file
 * @param len_buf     lunghezza del buffer dati
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int save_file(char *dir_file, char *file_name, char *buf, int len_buf);


/**
 * @function get_mappedfile
 * @brief Mappa il file in memoria nel puntatore passato da parametro
 * 
 * @param mappedfile  puntatore al file mappato
 * @param dir_file    directory in cui salvare il file
 * @param file_name   nome del file da mappare
 * 
 * @return size del file in caso di successo (o se il file non esiste), -1 in caso di errore
 */
int get_mappedfile(char **mappedfile, char *dir_file, char *file_name);


/**
 * @function clean_dirfile
 * @brief Rimuove tutti i file all'interno della directory passata da parametro
 * 
 * @param path   path della directory da ripulire
 * 
 * @return 0 se successo, -1 in caso di fallimento (errno settato)
 */
int clean_dirfile(char *path);


#endif /* FILES_HANDLER_ */