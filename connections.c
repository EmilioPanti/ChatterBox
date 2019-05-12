/**
 * @file connections.c
 * @brief File contenente l'implementazioni di alcune funzioni dichiarate in connections.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <connections.h>


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
int readn(long fd, void *buf, size_t size) {
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "readn", -1);
    err_check_return(buf == NULL, EINVAL, "readn", -1);
    err_check_return(size < 0, EINVAL, "readn", -1);

    //variabili di appoggio
    size_t left = size;
    char *bufptr = (char*)buf;

    //numero bytes letti dalla funzione read
    int r;

    while(left > 0) {
        r = read((int)fd,bufptr,left);

        //se c'è stato un errore (che non sia un'interruzione da un segnale)
        if((r == -1) && (errno != EINTR)) return -1;
        //se il socket è chiuso
        if (r == 0) return 0; 

        left = left - r;
        bufptr = bufptr + r;
    }

    return size;
}


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
int writen(long fd, void *buf, size_t size) {
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "writen", -1);
    err_check_return(buf == NULL, EINVAL, "writen", -1);
    err_check_return(size < 0, EINVAL, "writen", -1);

    //variabili di appoggio
    size_t left = size;
    char *bufptr = (char*)buf;

    //numero bytes scritti dalla funzione write
    int r;

    while(left>0) {
        r = write((int)fd,bufptr,left);

        //se c'è stato un errore (che non sia un'interruzione da un segnale)
        if((r == -1) && (errno != EINTR)) return -1;

        left = left - r;
        bufptr = bufptr + r;
    }

    return 1;
}


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
int openConnection(char* path, unsigned int ntimes, unsigned int secs){
    //controllo gli argomenti
    err_check_return(path == NULL, EINVAL, "openConnection", -1);

    int fd_skt;
    struct sockaddr_un sa;

    //controllo che i parametri non superino i valori max
    unsigned int retries = MAX_RETRIES;
    unsigned int seconds = MAX_SLEEPING;
    if (ntimes < MAX_RETRIES) retries = ntimes;
    if (secs < MAX_SLEEPING) seconds = secs;


    //prova a connettersi con il server
    int i = 0, check = -1, ack = 0;
    while ((check == -1) && (i < retries)) {
        //crea un socket
        fd_skt = socket(AF_UNIX,SOCK_STREAM,0);
        if (fd_skt == -1) return -1;

        strncpy(sa.sun_path, path ,UNIX_PATH_MAX);
        sa.sun_family=AF_UNIX;

        if (connect(fd_skt, (struct sockaddr*)&sa, sizeof(sa)) != -1) {
            //devo ricevere l'ack
            check = readn(fd_skt, &ack, sizeof(int));
            //se il server non ha accettato la connessione
            if (check <= 0 || ack != 1) {
                check = -1; 
                sleep(seconds);
            }
            //connessione accettata
            else check = fd_skt;
        }
        else sleep(seconds);
        i++;
    }

    errno = 0;
    return check;
}


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
int readHeader(long fd, message_hdr_t *hdr){
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "readHeader", -1);
    err_check_return(hdr == NULL, EINVAL, "readHeader", -1);

    int check, len;

    //leggo il tipo di operazione
    check = readn(fd, &(hdr->op), sizeof(op_t));
    if(check<=0) return check;

    //leggo la lunghezza del nickname del mandante
    check = readn(fd, &len, sizeof(int));
    if(check<=0) return check;

    //leggo il nickname del mandante
    check = readn(fd, hdr->sender, sizeof(char)*len);
    if(check<=0) return check;

    return 1;
}


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
int readData(long fd, message_data_t *data){
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "readData", -1);
    err_check_return(data == NULL, EINVAL, "readData", -1);

    int check, len;

    //leggo la lunghezza del nickname del ricevente
    check = readn(fd, &len, sizeof(int));
    if(check<=0) return check;

    //leggo il nickname del ricevente
    check = readn(fd, (data->hdr).receiver, sizeof(char)*len);
    if(check<=0) return check;

    //leggo la lunghezza del buffer dati
    check = readn(fd, &((data->hdr).len), sizeof(int));
    if(check<=0) return check;

    if((data->hdr).len != 0) {
        //alloco memoria per il buffer
        data->buf = malloc(sizeof(char)*((data->hdr).len));
        if (data->buf == NULL) return -1;
        //leggo il buffer dati
        check = readn(fd, data->buf, sizeof(char)*((data->hdr).len));
        if(check<=0) return check;
    }
    else data->buf = NULL;

    return 1;
}


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
int readMsg(long fd, message_t *msg){
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "readMsg", -1);
    err_check_return(msg == NULL, EINVAL, "readMsg", -1);

    int check;

    //leggo l'header del messaggio
    check = readHeader(fd, &(msg->hdr));
    if(check<=0) return check;

    //leggo il body del messaggio
    check = readData(fd, &(msg->data));
    if(check<=0) return check;

    return 1;
}


/**
 * @function sendHeader
 * @brief Invia l'header del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore (-1 e errno settato)
 */
int sendHeader(long fd, message_hdr_t *hdr){
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "sendHeader", -1);
    err_check_return(hdr == NULL, EINVAL, "sendHeader", -1);

    int check, len;

    //invio il tipo di operazione
    check = writen(fd, &(hdr->op), sizeof(op_t));
    if(check<=0) return check;

    //invio la lunghezza del nickname del mandante
    len = strlen(hdr->sender) + 1;
    check = writen(fd, &len, sizeof(int));
    if(check<=0) return check;

    //invio il nickname del mandante
    check = writen(fd, hdr->sender, sizeof(char)*len);
    if(check<=0) return check;

    return 1;
}


/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore (-1 e errno settato)
 */
int sendData(long fd, message_data_t *msg){
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "sendData", -1);
    err_check_return(msg == NULL, EINVAL, "sendData", -1);

    int check, len;

    //invio la lunghezza del nickname del ricevente
    len = strlen((msg->hdr).receiver) + 1;
    check = writen(fd, &len , sizeof(int));
    if(check<=0) return check;

    //invio il nickname del ricevente
    check = writen(fd, (msg->hdr).receiver, sizeof(char)*len);
    if(check<=0) return check;

    //invio la lunghezza del buffer dati
    check = writen(fd, &((msg->hdr).len), sizeof(int));
    if(check<=0) return check;

    if((msg->hdr).len != 0) {
        //invio il buffer dati
        check = writen(fd, msg->buf, sizeof(char)*((msg->hdr).len));
        if(check<=0) return check;
    }

    return 1;
}


/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server 
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore (-1 e errno settato)
 */
int sendRequest(long fd, message_t *msg){
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "sendRequest", -1);
    err_check_return(msg == NULL, EINVAL, "sendRequest", -1);

    int check;

    //invio l'header del messaggio
    check = sendHeader(fd, &(msg->hdr));
    if(check<=0) return check;

    //invio il body del messaggio
    check = sendData(fd, &(msg->data));
    if(check<=0) return check;

    return 1;
}


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
int sendHdr_toClient(long client_fd, message_hdr_t *hdr) {
    //controllo gli argomenti
    err_check_return(client_fd < 0, EINVAL, "sendHdr_toClient", -1);
    err_check_return(hdr == NULL, EINVAL, "sendHdr_toClient", -1);

    errno = 0;
    if (sendHeader(client_fd, hdr) <= 0) {
        if (errno == EPIPE) {
            //client disconnesso
            return 0;
        }
        else {
            //errore
            return -1;
        }
    }

    return 1;
}


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
int sendData_toClient(long client_fd, message_data_t *data) {
    //controllo gli argomenti
    err_check_return(client_fd < 0, EINVAL, "sendData_toClient", -1);
    err_check_return(data == NULL, EINVAL, "sendData_toClient", -1);

    errno = 0;
    //invio data
    if (sendData(client_fd, data) <= 0) {
        if (errno == EPIPE) {
            //client disconnesso
            return 0;
        }
        else {
            //errore
            return -1;
        }
    }

    return 1;
}


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
int sendMsg_toClient(long client_fd, message_t *msg) {
    //controllo gli argomenti
    err_check_return(client_fd < 0, EINVAL, "sendMsg_toClient", -1);
    err_check_return(msg == NULL, EINVAL, "sendMsg_toClient", -1);

    int check = sendHdr_toClient(client_fd, &msg->hdr);
    if (check != 1) return check;
    return sendData_toClient(client_fd, &msg->data);
}