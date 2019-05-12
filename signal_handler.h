/**
 * @file signal_handler.h
 * @brief File contenente le funzioni e le struct usate dal thread signal_handler, destinato alla
 *        gestione dei segnali
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#ifndef SIGNAL_HANDLER_
#define SIGNAL_HANDLER_

#define _POSIX_C_SOURCE 200809L
#include <signal.h>



/**
 * @struct args_sig_handler_t
 * @brief Argomenti necessari alla funzione signal_handler
 * 
 * @var set              signal mask dei segnali che dovrà gestire signal_handler
 * @var fun_clean        funzione da chiamare in caso di terminazione
 */
typedef struct args_sig_handler{
    sigset_t *set;
    void     (*fun_clean) ();
} args_sig_handler_t;



/**
 * @function signal_handler
 * @brief Funzione eseguita dal thread signal_handler 
 * 
 * @param arg argomenti necessari al thread signal_handler
 * 
 * @return 0 in caso di successo, altrimenti error
 */
void* signal_handler(void* arg);


#endif /* SIGNAL_HANDLER_ */