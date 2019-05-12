/**
 * @file error_handler.h
 * @brief File contenente alcune define per il controllo/gestione errori
 * @author Emilio Panti 531844 
 * Si dichiara che il contenuto di questo file e' in ogni sua parte opera
 * originale dell'autore
 */
#if !defined(ERROR_HANDLER_)
#define ERROR_HANDLER_

#include <stdio.h>
#include <errno.h>


#define err_return(val,err,ret) \
    if( (val) == (err) ) {return (ret);}


#define err_return_msg(val,err,ret,msg) \
    if( (val) == (err) ) {fprintf(stderr,msg); return (ret);}


#define err_return_msg_clean(val,err,ret,msg,clean) \
    if( (val) == (err) ) {fprintf(stderr,msg); clean; return (ret);}


#define err_check_return(check, errno_value , msg, ret) \
    if( check ) {errno = errno_value ; perror(msg); return (ret);}


#define err_exit(val,err,fun_clean) \
    if( (val) == (err) ) {perror("Errore"); fun_clean; exit(EXIT_FAILURE);}


#endif /* ERROR_HANDLER_ */