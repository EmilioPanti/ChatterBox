/**
 * @file pipe_fd.h
 * @brief File per gestione/creazione di una pipe. Questa pipe è utilizzata dai thread worker 
 *        per comunicare al thread listener gli fd dei client connessi che devono essere chiusi
 *        o che devono essere riaggiunti al set dei descrittori attivi (gestito dal listener).
 *        Questa pipe può esssere usata anche per comunicare al thread listener di terminare 
 *        la sua esecuzione
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef PIPE_FD_H_
#define PIPE_FD_H_

#include <pthread.h>


/**
 * @struct pipe_fd_t
 * @brief Gestore della pipe
 * 
 * @var pipe_read   descrittore per la lettura della pipe
 * @var pipe_write  descrittore per la scrittura nella pipe
 * @var mtx         mutex per controllare la lettura/scrittura sulla pipe
 */
typedef struct {
    int              pipe_read;
    int              pipe_write;
    pthread_mutex_t  mtx;
} pipe_fd_t;


//operazioni da comunicare al listener
typedef enum {
    UPDATE      = 0,   //l'fd comunicato deve essere riaggiunto al set dei descrittori 
                       //(tenuto dal listener) per ascoltare nuove richieste

    CLOSE       = 1,   // l'fd comunicato è chiuso, deve essere aggiornato il numero
                       // dei client connessi (tenuto dal listener)

    TERMINATE   = 2,   // il thread listener deve terminare la sua esecuzione
} op_pipe_t;



/**
 * @function init_pipe_fd
 * @brief Inizializza il gestore della pipe
 * 
 * @return p puntatore al gestore della pipe, NULL in caso di fallimento
 */
pipe_fd_t *init_pipe_fd();


/**
 * @function clean_pipe_fd
 * @brief Cancella la memoria allocata per il gestore della pipe e chiude i descrittori della pipe
 * 
 * @param pipe puntatore al gestore della pipe
 */
void clean_pipe_fd(pipe_fd_t *pipe_fd);


/**
 * @function write_pipe
 * @brief Scrive sulla pipe passata un fd e l'operazione relativa ad esso
 * 
 * @param pipe puntatore al gestore della pipe
 * @param fd   fd da comunicare 
 * @param op   operazione relativa all'fd 
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int write_pipe(pipe_fd_t *pipe_fd, int fd, op_pipe_t op);


/**
 * @function read_pipe
 * @brief Legge dalla pipe l'fd e l'operazione relativa ad esso
 * 
 * @param pipe puntatore al gestore della pipe
 * @param fd   puntatore su cui scrivere l'fd letto nella pipe
 * @param op   puntatore su cui scrivere l'operazione letta nella pipe
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int read_pipe(pipe_fd_t *pipe_fd, int *fd, op_pipe_t *op);


#endif /* PIPE_FD_H_ */