/**
 * @file message.c
 * @brief File contenente l'implementazioni delle funzioni dichiarate in message.h
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#include <message.h>
#include <connections.h>


/**
 * @function duplicate_msg
 * @brief Duplica un messaggio
 * 
 * @param msg  messaggio da duplicare
 * @param op   operazione da inserire nel messaggio duplicato
 * 
 * @return puntatore al messaggio duplicato, NULL se errore
 */
message_t *duplicate_msg(message_t *msg, op_t op) {
    //controllo gli argomenti
    err_check_return(msg == NULL,  EINVAL, "duplicate_msg", NULL);

    //alloco la memoria per il duplicato
    message_t *msg_dup = malloc(sizeof(message_t));
    err_return_msg(msg_dup,NULL,NULL,"Errore: malloc\n");

    //copio il buffer dei dati
    char *buff_data = malloc(sizeof(char)*(msg->data.hdr.len));
    if (buff_data == NULL){
        fprintf(stderr, "Errore: malloc\n");
        free(msg);
        return NULL;
    }
    strncpy(buff_data, msg->data.buf, msg->data.hdr.len);

    //setto il duplicato come il messaggio originale ad eccezione della op
    setHeader(&msg_dup->hdr, op, msg->hdr.sender);
    if (op == TXT_MESSAGE || op == FILE_MESSAGE) {
        //non mi importa del receiver
        setData(&msg_dup->data, "", buff_data, msg->data.hdr.len);
    }
    else {
        setData(&msg_dup->data, msg->data.hdr.receiver, buff_data, msg->data.hdr.len);
    } 

    return msg_dup;
}


/**
 * @function free_msg
 * @brief Libera la memoria allocata per il messaggio
 * 
 * @param msg messaggio da cancellare
 */
void free_msg(message_t *msg) {
    if (msg == NULL) return;
    if (msg->data.buf != NULL) free(msg->data.buf);
    free(msg);
}


/**
 * @function clean_msg_node
 * @brief Libera la memoria allocata per il nodo della lista messaggi
 * 
 * @param msg_node nodo della lista messaggi da cancellare
 */
void clean_msg_node(void *node) {
    if (node == NULL) return;
    message_node_t *msg_node = (message_node_t *)node;
    if (msg_node->msg != NULL) free_msg(msg_node->msg);
    free(msg_node);
}


/**
 * @function init_msg_node
 * @brief Inizializza un struttura 'message_node_t'
 * 
 * @param msg        messaggio da inserire nel nodo 
 * @param delivere   variabile per le statistiche (0 = da consegnare, 1 = già consegnato)
 * 
 * @return p puntatore alla nuova struttura, NULL in caso di fallimento
 */
message_node_t *init_msg_node(message_t *msg, int delivered){
    //controllo gli argomenti
    err_check_return(msg == NULL, EINVAL, "init_msg_node", NULL);
    err_check_return(delivered != 0 && delivered != 1, EINVAL, "init_msg_node", NULL);

    //alloco la memoria per la struttura 'message_node_t'
    message_node_t *msg_node = malloc(sizeof(message_node_t));
    err_return_msg(msg_node,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri della lista
    msg_node->msg = msg;
    msg_node->delivered = delivered;

    return msg_node;
}


/**
 * @function init_param_send_msgs
 * @brief Inizializza un struttura 'param_send_msgs_t'
 * 
 * @param fd  descrittore a cui verranno inviati i messaggi
 * 
 * @return p puntatore alla nuova struttura, NULL in caso di fallimento
 */
param_send_msgs_t *init_param_send_msgs(long fd){
    //controllo gli argomenti
    err_check_return(fd < 0, EINVAL, "init_param_send_msgs", NULL);

    //alloco la memoria per la struttura 'param_send_msgs_t'
    param_send_msgs_t *prm = malloc(sizeof(param_send_msgs_t));
    err_return_msg(prm,NULL,NULL,"Errore: malloc\n");

    //inizializzo i parametri della lista
    prm->fd             = fd;
    prm->msgsdelivered  = 0;
    prm->filesdelivered = 0;
    prm->disconnected   = 0;

    return prm;
}


/**
 * @function send_list_msgs
 * @brief Invia il messaggio presente in node all'fd specificato in param
 *        
 * @param us      puntatore al nodo contenente il messaggio da inviare
 * @param param   struttura in cui ci sono i dati da aggiornare/usare per la funzione
 * 
 * @return 0 in caso di successo, -1 in caso di errore
 */
int send_list_msgs(void *node, void *param){
    //controllo gli argomenti
    err_check_return(node == NULL, EINVAL, "send_list_msgs", -1);
    err_check_return(param == NULL, EINVAL, "send_list_msgs", -1);

    message_node_t *msg_node = (message_node_t *)node;
    param_send_msgs_t *prm = (param_send_msgs_t *)param;

    //se l'fd si è disconnesso non spedisco niente
    if (prm->disconnected == 1) return 0;

    //invio il messaggio in 'msg_node'
    int check = sendMsg_toClient(prm->fd, msg_node->msg);
    //se l'invio del messaggio è avvenuto correttamente
    if (check == 1) {
        //se ancora non era mai stato consegnato
        if (msg_node->delivered == 0){
            msg_node->delivered = 1;
            //aggiorno i contatori passati da parametro
            if (msg_node->msg->hdr.op == TXT_MESSAGE) prm->msgsdelivered++;
            else if (msg_node->msg->hdr.op == FILE_MESSAGE) prm->filesdelivered++;
        }
    }
    //se si è disconnesso il client 
    else if (check == 0) prm->disconnected = 1;
    //in caso di errore
    else return -1;

    return 0;
}