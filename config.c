/**
 * @file config.c
 * @brief File contenente l'implementazioni di alcune funzioni dichiarate in config.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <config.h>
#include <error_handler.h>



/**
 * @function assegna_var
 * @brief assegna la variabile alla struttura di configurazione (se var è ok)
 * 
 * @param nomevar      nome della variabile  di configurazione
 * @param varvar       valore della variabile  di configurazione
 * @param conf_server  struttura in cui salvo la variabile di configurazione
 * 
 * @return 0 in caso di successo, -1 in caso di fallimento
 */
static int assegna_var(char *nomevar, char *valvar, configs_t *conf_server) {
    if (strncmp("UnixPath",nomevar,strlen("UnixPath"))==0){
        conf_server->socket_path=valvar;
        free(nomevar);
        return 0;
    }
    else if (strncmp("MaxConnections",nomevar,strlen("MaxConnections"))==0){
        conf_server->max_conn=strtol(valvar,NULL,10);
        free(nomevar);
        free(valvar);
        return 0;
    }
    else if (strncmp("ThreadsInPool",nomevar,strlen("ThreadsInPool"))==0){
        conf_server->threads=strtol(valvar,NULL,10);
        free(nomevar);
        free(valvar);
        return 0;
    }
    else if (strncmp("MaxMsgSize",nomevar,strlen("MaxMsgSize"))==0){
        conf_server->max_msg_size=strtol(valvar,NULL,10);
        free(nomevar);
        free(valvar);
        return 0;
    }
    else if (strncmp("MaxFileSize",nomevar,strlen("MaxFileSize"))==0){
        conf_server->max_file_size=strtol(valvar,NULL,10);
        free(nomevar);
        free(valvar);
        return 0;
    }
    else if (strncmp("MaxHistMsgs",nomevar,strlen("MaxHistMsgs"))==0){
        conf_server->max_hist_msg=strtol(valvar,NULL,10);
        free(nomevar);
        free(valvar);
        return 0;
    }
    else if (strncmp("DirName",nomevar,strlen("DirName"))==0){
        conf_server->dir_name=valvar;
        free(nomevar);
        return 0;
        }
    else if (strncmp("StatFileName",nomevar,strlen("StatFileName"))==0){
        conf_server->stat_file_name=valvar;
        free(nomevar);
        return 0;
    }
    //se non ho trovato nessuna corrispondenza
    free(nomevar);
    free(valvar);
    return -1;
}



/**
 * @function parser
 * @brief prende dalla linea (se scritta correttamente) un valore di configurazione
 *  e lo salva nell'apposita struttura passata da parametro
 * 
 * @param line         puntatore linea da parsare
 * @param conf_server  struttura in cui salvo le configurazioni specificate in file_name
 * 
 * @return 0 in caso di successo, -1 in caso di fallimento
 */
static int parser(char *line, configs_t *conf_server) {

    //nome e valore della variabile di configurazione
    char *nomevar = NULL;
    char *valvar = NULL;

    //variabili di supporto
    char str[MAX_LINE+1];
    int i = 0, j = 0;
    int stop = 0;
    int n = strlen(line)+1;
    
    //prendo il nome della variabile e lo inserisco in str
    while (i < n && stop == 0) {
        if (line[i]=='\0') {
            fprintf(stderr, "Per ogni variabile che si vuole definire: NomeVar = Valore(int>0 o stringa)\n");
            if(nomevar!=NULL) free(nomevar);
            if(valvar!=NULL) free(valvar);
            return -1;
        }
        if (line[i]==' ') {
            str[i] = '\0';
            stop = 1;
        }
        else {
            str[i] = line[i];
            i++;
        }
    }
  
    //salvo in nomevar il valore presente in str
    nomevar = malloc(sizeof(char)*i+1);
    err_return_msg(nomevar,NULL,-1,"Errore: malloc\n");
    strncpy(nomevar,str,i+1);

    //scorro finchè non giungo al valore dell variabile
    stop = 0;
    while (i < n && stop == 0) {
        if (line[i]=='\0') {
            fprintf(stderr, "Per ogni variabile che si vuole definire: NomeVar = Valore(int>0 o stringa)\n");
            if(nomevar!=NULL) free(nomevar);
            if(valvar!=NULL) free(valvar);
            return -1;
        }
        if (line[i]!=' ' && line[i]!='=') stop = 1;
        else i++;
    }

    //se la variabile non è definita
    if(stop != 1) {
        fprintf(stderr, "Per ogni variabile che si vuole definire: NomeVar = Valore(int>0 o stringa)\n");
        if(nomevar!=NULL) free(nomevar);
        if(valvar!=NULL) free(valvar);
        return -1;
    }

    //prendo il valore della variabile e lo inserisco in str
    stop = 0;
    while (i < n && stop == 0) {
        if (line[i]==' ' || line[i]=='\0' || line[i]=='\n') stop = 1;
        else {
            str[j] = line[i];
            j++;
            i++;
        }
    }
    str[j] = '\0';

    //salvo in valvar il valore presente in str
    valvar = malloc(sizeof(char)*j+1);
    err_return_msg(valvar,NULL,-1,"Errore: malloc\n");
    strncpy(valvar,str,j+1);

    //Se la variabile non è scritta bene
    if (nomevar==NULL || valvar==NULL) {
        if(nomevar!=NULL) free(nomevar);
        if(valvar!=NULL) free(valvar);
        fprintf(stderr, "Per ogni variabile che si vuole definire: NomeVar = Valore(int>0 o stringa)\n");
        return -1;
    }
    //Se OK
    else {  
        return assegna_var(nomevar, valvar, conf_server);
    }
}



/**
 * @function configura_server
 * @brief Salva in una struttura apposita le configurazioni specificate da un file 
 * 
 * @param file_name    nome del file di configurazione
 * @param conf_server  struttura in cui salvo le configurazioni specificate in file_name
 * 
 * @return 0 in caso di successo, -1 in caso di fallimento
 */
int configura_server(char *file_name, configs_t *conf_server) {
    //controllo gli argomenti
    err_check_return(file_name == NULL, EINVAL, "configura_server", -1);
    err_check_return(conf_server == NULL, EINVAL, "configura_server", -1);


    //apro il file in sola lettura
    FILE *fl = fopen(file_name, "r");
    err_return_msg(fl,NULL,-1,"Errore: impossibile aprire file di configurazione\n");


    //dove inserisco le righe che leggerò dal file
    char *line = malloc(MAX_LINE*sizeof(char));
    err_return_msg(fl,NULL,-1,"Errore: malloc\n");


    //leggo il file riga per riga
    while(fgets(line, MAX_LINE+1 , fl) != NULL) {
        //controllo che la riga letta non sia un commento o una riga vuota
        if ((line[0]!='#')&&(line[0]!=' ')&&(line[0]!='\n')&&(line[0]!='\0')) { 
            //per ogni linea chiamo la funzione tokenizer
            if(parser(line,conf_server)==-1) {
                free(line);
                return-1;
            }
        }
    }


    //controllo che non ci siano valori illeciti 
    //nota: può non esserci il nome del file per le statistiche (come da specifica del progetto)
    if (conf_server->socket_path==NULL ||
        conf_server->max_conn<1        ||
        conf_server->threads<1         ||
        conf_server->max_msg_size<1    ||
        conf_server->max_file_size<1   ||
        conf_server->max_hist_msg<1    ||
        conf_server->dir_name==NULL) {
            fprintf(stderr,"Manca qualche variabile o sono stati inseriti valori illeciti\n");
            free(line);
            fclose(fl);
            return-1;
    }

    free(line);
    fclose(fl);


    //la max size dei file è in kilobyte, devo averla in byte
    conf_server->max_file_size = conf_server->max_file_size * 1024;


    //creo il file.txt per le statistiche
    FILE *fp = fopen(conf_server->stat_file_name, "w");
    if(fp == NULL) {
        fprintf(stderr, " impossibile creare il file  %s\n", conf_server->stat_file_name);
        return -1;
    }
    else fclose(fp);

    //controllo che esista la directory per i file
    DIR* dir = opendir(conf_server->dir_name);
    //se esiste
    if (dir != NULL) closedir(dir);
    //se non esiste
    else {
        fprintf(stderr, " Directory %s inesistente\n", conf_server->stat_file_name);
        return -1;
    }

    return 0;
}



/**
 * @function clean_configs
 * @brief Libera la memoria occupata da conf_server
 * 
 * @param conf_server struttura da liberare
 */
void clean_configs(configs_t *conf_server) {
    if (conf_server == NULL) return;
    if(conf_server->socket_path != NULL) free(conf_server->socket_path);
    if(conf_server->dir_name != NULL) free(conf_server->dir_name);
    if(conf_server->stat_file_name != NULL) free(conf_server->stat_file_name);
}