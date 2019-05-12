/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES     10
#define MAX_SLEEPING     3
#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#define DEBUG
#include <message.h>

/**
 * @file  connection.h
 * @brief Contiene le funzioni che implementano il protocollo 
 *        tra i clients ed il server
 */


/**
 * @function readn
 * @brief Cerca di leggere size byte da fd ed inserirli in buf
 * 
 * @param fd    descrittore dal quale leggere
 * @param buf   dove inserire i bytes letti 
 * @param size  bytes da leggere
 * 
 * @return 0 se descrittore chiuso, -1 se errore (errno settato), size se ha successo.
 */
int readn(long fd, void *buf, size_t size);


/**
 * @function writen
 * @brief Cerca di scrivere size byte di buf in fd
 * 
 * @param fd    descrittore verso cui scrivere
 * @param buf   dove prelevare i bytes da inviare
 * @param size  bytes da inviare
 * 
 * @return -1 se errore (errno settato), 1 se ha successo.
 */
int writen(long fd, void *buf, size_t size);


/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server 
 *
 * @param path   Path del socket AF_UNIX 
 * @param ntimes numero massimo di tentativi di retry
 * @param secs   tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs);

// -------- server side ----- 
/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readHeader(long fd, message_hdr_t *hdr);

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readData(long fd, message_data_t *data);

/**
 * @function readMsg
 * @brief Legge l'intero messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readMsg(long fd, message_t *msg);

/* da completare da parte dello studente con altri metodi di interfaccia */


// ------- client side ------
/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server 
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore (-1 e errno settato)
 */
int sendRequest(long fd, message_t *msg);

/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore (-1 e errno settato)
 */
int sendData(long fd, message_data_t *msg);


/* da completare da parte dello studente con eventuali altri metodi di interfaccia */
/**
 * @function sendHeader
 * @brief Invia l'header del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore (-1 e errno settato)
 */
int sendHeader(long fd, message_hdr_t *hdr);


/**
 * @function sendHdr_toClient
 * @brief Invia l'header del messaggio ad un client
 * 
 * @param client_fd  fd del client a cui inviare l' header del messaggio
 * @param hdr        dati da inviare
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 se si è disconnesso il client durante l'invion 
 *          1 se inviato correttamente
 */
int sendHdr_toClient(long client_fd, message_hdr_t *hdr);


/**
 * @function sendData_toClient
 * @brief Invia la parte data di un messaggio ad un client 
 * 
 * @param client_fd  fd del client a cui inviare l' header del messaggio
 * @param data       parte dati da inviare al client
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 se si è disconnesso il client durante l'invion 
 *          1 se inviato correttamente
 */
int sendData_toClient(long client_fd, message_data_t *data);


/**
 * @function sendMsg_toClient
 * @brief Invia un messaggio ad un client
 * 
 * @param client_fd  fd del client a cui inviare l' header del messaggio
 * @param msg        msg da inviare al client
 * 
 * @return -1 se occorre qualche errore e si deve terminare il server chatty,
 *          0 se si è disconnesso il client
 *          1 se inviato correttamente
 */
int sendMsg_toClient(long client_fd, message_t *msg);


#endif /* CONNECTIONS_H_ */
