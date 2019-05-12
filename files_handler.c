/**
 * @file files_handler.c
 * @brief Implementazione delle funzioni dichiarate in files_handler.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#include <error_handler.h>
#include <files_handler.h>


//mutex per operare nella directory dei file del server
pthread_mutex_t mtx_dirfile = PTHREAD_MUTEX_INITIALIZER;



/* ---------------------------- funzioni di utilita' -------------------------------- */

/**
 * @function lock_dirfile
 * 
 * @brief prende la lock della directory dei file
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore 
 */
static int lock_dirfile(){ 
    return pthread_mutex_lock(&mtx_dirfile);
}


/**
 * @function unlock_dirfile
 * 
 * @brief rilascia la lock della directory dei file
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore 
 */
static int unlock_dirfile(){ 
    return pthread_mutex_unlock(&mtx_dirfile);
}



/* -------------------------- interfaccia files_handler ------------------------------ */

/**
 * @function get_filename
 * @brief Restituisce solo il nome del file rispetto al suo path completo
 * 
 * @param pathfile  path del file
 * 
 * @return puntatore al filename se successo, NULL ed errno settato se errore,
 *         NULL ed errno non settato se il file non è valido
 */
char* get_filename(char *pathfile){
    //controllo gli argomenti
    err_check_return(pathfile == NULL, EINVAL, "get_filename", NULL);
    
    //rimuovo eventuali punti all'inizio del path
    int stop = 0;
    while (stop == 0){
        if (*pathfile == '.') pathfile++;
        else if (*pathfile == '\0') return NULL;
        else stop = 1;
    }

    char *token = strtok(pathfile,"/");

    while(token != NULL) {
        pathfile = token;
        token = strtok(NULL,"/");
    }

    //adesso pathfile contiene solo il file name
    int len = strlen(pathfile) + 1;
    char *file_name = malloc(sizeof(char)*len);
    err_return_msg(file_name,NULL,NULL,"Errore: malloc\n");
    strncpy(file_name, pathfile, len);
    
    return file_name;
}


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
int save_file(char *dir_file, char *file_name, char *buf, int len_buf){
    //controllo gli argomenti
    err_check_return(dir_file == NULL, EINVAL, "save_file", -1);
    err_check_return(file_name == NULL, EINVAL, "save_file", -1);
    err_check_return(buf == NULL, EINVAL, "save_file", -1);
    err_check_return(len_buf < 0, EINVAL, "save_file", -1);

    //creo il path per il nuovo file
    int len = strlen(dir_file) + strlen(file_name) + 2;
    char *new_file = malloc(sizeof(char)*len);
    err_return_msg(new_file,NULL,-1,"Errore: malloc\n");
    if (snprintf(new_file, len, "%s/%s", dir_file, file_name) < 0) {
        fprintf(stderr, "Errore: snprintf\n");
        free(new_file);
        return -1;
    }

    int check = lock_dirfile();
    if (check != 0 )free(new_file);
    err_check_return(check != 0, check, "lock_dirfile", -1);

    //creo (o sovrascrivo) il file
    FILE *fp = fopen(new_file, "w");
    if(fp == NULL) {
        fprintf(stderr, "Errore: impossibile creare/aprire file %s\n", new_file);
        free(new_file);
        unlock_dirfile();
        return -1;
    }
    else {
        //scrivo nel file il buffer dati
        if (fwrite(buf, len_buf, 1, fp) <= 0) {
            fprintf(stderr, "Errore: impossibile scrivere sul file %s\n", new_file);
            fclose(fp);
            free(new_file);
            unlock_dirfile();
            return -1;
        }
        fflush(fp);
        fclose(fp);
    }

    free(new_file);

    check = unlock_dirfile();
    err_check_return(check != 0, check, "unlock_dirfile", -1);

    return 0;
}


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
int get_mappedfile(char **mappedfile, char *dir_file, char *file_name){
    //controllo gli argomenti
    err_check_return(dir_file == NULL, EINVAL, "get_mappedfile", -1);
    err_check_return(file_name == NULL, EINVAL, "get_mappedfile", -1);
    err_check_return(mappedfile == NULL, EINVAL, "get_mappedfile", -1);

    //creo il path del file da mappare
    int len = strlen(dir_file) + strlen(file_name) + 2;
    char *file_tomap = malloc(sizeof(char)*len);
    err_return_msg(file_tomap,NULL,-1,"Errore: malloc\n");
    if (snprintf(file_tomap, len, "%s/%s", dir_file, file_name) < 0) {
        fprintf(stderr, "Errore: snprintf\n");
        free(file_tomap);
        return -1;
    }

    int check = lock_dirfile();
    err_check_return(check != 0, check, "lock_dirfile", -1);
    //controllo che esista il file
    struct stat st;
	if (stat(file_tomap, &st) == -1) {
        check = unlock_dirfile();
        if(check != 0) free(file_tomap);
        err_check_return(check != 0, check, "unlock_dirfile", -1);
        return 0;
    }

    int fd = open(file_tomap, O_RDONLY);
	if (fd<0) {
		fprintf(stderr, "ERRORE: aprendo il file %s\n", file_tomap);
		close(fd);
        free(file_tomap);
        unlock_dirfile();
		return -1;
	}
	//mappo il file in memoria
	*mappedfile = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (*mappedfile == MAP_FAILED) {
		fprintf(stderr, "ERRORE: mappando il file %s in memoria\n", file_tomap);
		close(fd);
        free(file_tomap);
        unlock_dirfile();
		return -1;
	}
	close(fd);
    free(file_tomap);

    check = unlock_dirfile();
    err_check_return(check != 0, check, "unlock_dirfile", -1);

    return st.st_size;
}


/**
 * @function clean_dirfile
 * @brief Rimuove tutti i file all'interno della directory passata da parametro
 * 
 * @param path   path della directory da ripulire
 * 
 * @return 0 se successo, -1 in caso di fallimento (errno settato)
 */
int clean_dirfile(char *path) {
    //controllo gli argomenti
    err_check_return(path == NULL, EINVAL, "clean_dirfile", -1);

    DIR *d       = NULL;
    int path_len = strlen(path);

    struct dirent *file = NULL;

    //apro la directory
    d = opendir(path);
    err_return_msg(d, NULL, -1, "Errore: opendir\n");

    //scorro tutti i file nella directory
    while ( (errno = 0, file = readdir(d)) != NULL) {

        //non considero i file "." e ".."
        if (strcmp(file->d_name, ".") != 0  &&  strcmp(file->d_name, "..") != 0) {

            //prendo il path del file
            int len_path_file = path_len + strlen(file->d_name) + 2;
            char *path_file = malloc(len_path_file);
            if (path_file == NULL) {
                closedir(d);
                fprintf(stderr, "Errore: malloc\n");
                return -1;
            }
            if (snprintf(path_file, len_path_file, "%s/%s", path, file->d_name) < 0) {
                free(path_file);
                closedir(d);
                fprintf(stderr, "Errore: snprintf\n");
                return -1;
            }

            //unlink del file
            if (unlink(path_file) != 0) {
                free(path_file);
                closedir(d);
                fprintf(stderr, "Errore: unlink file\n");
                return -1;
            }

            free(path_file);
        }
    }

    //se c'è stato un errore
    if (errno != 0) {
        closedir(d);
        fprintf(stderr, "Errore: eliminazione file\n");
        return -1;
    }

    //chiudo la directory
    if (closedir(d) == -1) {
        fprintf(stderr, "Errore: closedir\n");
        return -1;
    }

    return 0;
}
