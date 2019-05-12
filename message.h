/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <assert.h>
#include <string.h>
#include <config.h>
#include <ops.h>
#include <stdlib.h>
#include <stdio.h>

#include <error_handler.h>

/**
 * @file  message.h
 * @brief Contiene il formato del messaggio
 */


/**
 *  @struct header
 *  @brief header del messaggio 
 *
 *  @var op tipo di operazione richiesta al server
 *  @var sender nickname del mittente 
 */
typedef struct {
    op_t     op;   
    char sender[MAX_NAME_LENGTH+1];
} message_hdr_t;


/**
 *  @struct header
 *  @brief header della parte dati
 *
 *  @var receiver nickname del ricevente
 *  @var len lunghezza del buffer dati
 */
typedef struct {
    char receiver[MAX_NAME_LENGTH+1];
    unsigned int   len;  
} message_data_hdr_t;


/**
 *  @struct data
 *  @brief body del messaggio 
 *
 *  @var hdr header della parte dati
 *  @var buf buffer dati
 */
typedef struct {
    message_data_hdr_t  hdr;
    char               *buf;
} message_data_t;


/**
 *  @struct messaggio
 *  @brief tipo del messaggio 
 *
 *  @var hdr header
 *  @var data dati
 */
typedef struct {
    message_hdr_t  hdr;
    message_data_t data;
} message_t;


/**
 * @struct message_node_t
 * @brief Nodo della lista messaggi degli utenti
 * 
 * @var msg         puntatore al messaggio
 * @var delivered   variabile per le statistiche (0 = da consegnare, 1 = già consegnato)
 */
typedef struct {
    message_t   *msg;
    int         delivered;
} message_node_t;


/**
 * @struct param_send_msgs_t
 * @brief Struttura dati per i parametri della funzione 'send_msgs'
 * 
 * @var fd                 descrittore a cui inviare i messaggi 
 * @var msgsdelivered      contatore dei messaggi testuali consegnati
 * @var filesdelivered     contatore dei messaggi di tipo file consegnati
 * @var disconnected       per sapere se l'fd si è già disconnesso (1) o meno (0)
 */
typedef struct {
    long  fd;
    int   msgsdelivered;
    int   filesdelivered;
    int   disconnected;
} param_send_msgs_t;


/* ------ funzioni di utilità ------- */

/**
 * @function setheader
 * @brief scrive l'header del messaggio
 *
 * @param hdr puntatore all'header
 * @param op tipo di operazione da eseguire
 * @param sender mittente del messaggio
 */
static inline void setHeader(message_hdr_t *hdr, op_t op, char *sender) {
#if defined(MAKE_VALGRIND_HAPPY)
    memset((char*)hdr, 0, sizeof(message_hdr_t));
#endif
    hdr->op  = op;
    strncpy(hdr->sender, sender, strlen(sender)+1);
}
/**
 * @function setData
 * @brief scrive la parte dati del messaggio
 *
 * @param msg puntatore al body del messaggio
 * @param rcv nickname o groupname del destinatario
 * @param buf puntatore al buffer 
 * @param len lunghezza del buffer
 */
static inline void setData(message_data_t *data, char *rcv, const char *buf, unsigned int len) {
#if defined(MAKE_VALGRIND_HAPPY)
    memset((char*)&(data->hdr), 0, sizeof(message_data_hdr_t));
#endif

    strncpy(data->hdr.receiver, rcv, strlen(rcv)+1);
    data->hdr.len  = len;
    data->buf      = (char *)buf;
}


/**
 * @function duplicate_msg
 * @brief Duplica un messaggio
 * 
 * @param msg  messaggio da duplicare
 * @param op   operazione da inserire nel messaggio duplicato
 * 
 * @return puntatore al messaggio duplicato, NULL se errore
 */
message_t *duplicate_msg(message_t *msg, op_t op);


/**
 * @function free_msg
 * @brief Libera la memoria allocata per il messaggio
 * 
 * @param msg messaggio da cancellare
 */
void free_msg(message_t *msg);


/**
 * @function clean_msg_node
 * @brief Libera la memoria allocata per il nodo della lista messaggi
 * 
 * @param msg_node nodo della lista messaggi da cancellare
 */
void clean_msg_node(void *node);


/**
 * @function init_msg_node
 * @brief Inizializza un struttura 'message_node_t'
 * 
 * @param msg        messaggio da inserire nel nodo 
 * @param delivere   variabile per le statistiche (0 = da consegnare, 1 = già consegnato)
 * 
 * @return p puntatore alla nuova struttura, NULL in caso di fallimento
 */
message_node_t *init_msg_node(message_t *msg, int delivered);


/**
 * @function init_param_send_msgs
 * @brief Inizializza un struttura 'param_send_msgs_t'
 * 
 * @param fd  descrittore a cui verranno inviati i messaggi
 * 
 * @return p puntatore alla nuova struttura, NULL in caso di fallimento
 */
param_send_msgs_t *init_param_send_msgs(long fd);


/**
 * @function send_list_msgs
 * @brief Invia il messaggio presente in node all'fd specificato in param
 *        
 * @param us      puntatore al nodo contenente il messaggio da inviare
 * @param param   struttura in cui ci sono i dati da aggiornare/usare per la funzione
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int send_list_msgs(void *node, void *param);


#endif /* MESSAGE_H_ */
