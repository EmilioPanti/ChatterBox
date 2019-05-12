/**
 * @file signal_handler.c
 * @brief File contenente l'implementazioni di alcune funzioni dichiarate in signal_handler.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#define _POSIX_C_SOURCE 200809L
#include <signal_handler.h>
#include <errno.h>
#include <stats.h>
#include <config.h>


/**
 * @function signal_handler
 * @brief Funzione eseguita dal thread signal_handler 
 * 
 * @param arg argomenti necessari al thread signal_handler
 * 
 * @return 0 in caso di successo, altrimenti error
 */
void *signal_handler(void *arg) {
    //configurazioni del server (definita in chatty.c)
    extern configs_t conf_server;

    sigset_t *set         = ((args_sig_handler_t*)arg)->set;
    void (*fun_clean) ()  = ((args_sig_handler_t*)arg)->fun_clean;

    //variabile di ritorno pthread_exit
    long ret = 0;

    //apro il file delle statistiche in scrittura se è stato 
    //specificato il nome di esso nel file di configurazione
    FILE *fl = NULL;
    if (conf_server.stat_file_name != NULL) {
        fl = fopen(conf_server.stat_file_name, "a");
        if (fl == NULL) {
            perror("apertura file statistiche");
            fun_clean();
            ret = errno;
            pthread_exit((void *) ret);
        }
    }

    //variabile di appoggio
    int sig = 0, checklock = 0;

    while(1) {
        //aspetto un segnale del set
        int r = sigwait(set, &sig);
        if (r != 0) {
            errno = r;
            perror("sigwait");
            if (fl != NULL) fclose(fl);
            fun_clean();
            ret = errno;
            pthread_exit((void *) ret);
        }

        //controllo quale segnale è arrivato
        switch(sig) {
	        case SIGINT:
            case SIGTERM:
            case SIGQUIT:
                //terminazione per richiesta (successo)
                if (fl != NULL) fclose(fl);
	            fun_clean();
                ret = 0;
                pthread_exit((void *) ret);
	            break;
	        case SIGUSR1:
                //stampare le  statistiche
	            if (fl != NULL) {
                    checklock = lock_stats();
                    if (checklock != 0) {
                        fprintf(stderr,"errore lock_stats");
                        fclose(fl);
                        fun_clean();
                        ret = checklock;
                        pthread_exit((void *) ret);
                    }
                    if (printStats(fl) == -1) {
                        fprintf(stderr,"errore stampa statistiche nel relativo file");
                        unlock_stats();
                        fclose(fl);
                        fun_clean();
                        ret = errno;
                        pthread_exit((void *) ret);
                    }
                    checklock = unlock_stats();
                    if (checklock != 0) {
                        fprintf(stderr,"errore unlock_stats");
                        fclose(fl);
                        fun_clean();
                        ret = checklock;
                        pthread_exit((void *) ret);
                    }
                }
	            break;
	        case SIGPIPE:
                //da ignorare
	            ;
	            break;
            case SIGUSR2:
                //terminazione per errore (inviata dagli threads in caso di errore)
                if (fl != NULL) fclose(fl);
	            fun_clean();
                ret = -1;
                pthread_exit((void *) ret);
	            break;
	        default:
	            ;
	            break;
	    }
    }

    return 0;	   
}