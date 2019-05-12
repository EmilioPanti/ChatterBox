/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
#if !defined(MEMBOX_STATS_)
#define MEMBOX_STATS_

#include <stdio.h>
#include <time.h>
#include <pthread.h>


struct statistics {
    unsigned long nusers;                       // n. di utenti registrati
    unsigned long nonline;                      // n. di utenti connessi
    unsigned long ndelivered;                   // n. di messaggi testuali consegnati
    unsigned long nnotdelivered;                // n. di messaggi testuali non ancora consegnati
    unsigned long nfiledelivered;               // n. di file consegnati
    unsigned long nfilenotdelivered;            // n. di file non ancora consegnati
    unsigned long nerrors;                      // n. di messaggi di errore
    unsigned long ngroups;                      // n. di gruppi utenti 
};


/* aggiungere qui altre funzioni di utilita' per le statistiche */

/**
 * @function lock_stats
 * @brief prende la lock delle statistiche
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
static inline int lock_stats() { 
    extern pthread_mutex_t mtx_stats;
    return pthread_mutex_lock(&mtx_stats); 
}

/**
 * @function unlock_stats
 * @brief rilascia la lock delle statistiche
 * 
 * @return 0 se successo, oppure un altro intero che rappresenta l'errore
 */
static inline int unlock_stats() { 
    extern pthread_mutex_t mtx_stats;
    return pthread_mutex_unlock(&mtx_stats); 
}


/**
 * @function printStats
 * @brief Stampa le statistiche nel file passato come argomento
 *
 * @param fout descrittore del file aperto in append.
 *
 * @return 0 in caso di successo, -1 in caso di fallimento 
 */
static inline int printStats(FILE *fout) {
    extern struct statistics chattyStats;

    if (fprintf(fout, "%ld - %ld %ld %ld %ld %ld %ld %ld %ld\n",
		(unsigned long)time(NULL),
		chattyStats.nusers, 
		chattyStats.nonline,
		chattyStats.ndelivered,
		chattyStats.nnotdelivered,
		chattyStats.nfiledelivered,
		chattyStats.nfilenotdelivered,
		chattyStats.nerrors,
        chattyStats.ngroups
		) < 0) return -1;
    fflush(fout);
    return 0;
}

#endif /* MEMBOX_STATS_ */
