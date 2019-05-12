#!/bin/bash

function usage() 
{ 
    echo "Usage: $0 [-f: chatty_conf_path] [-t: time (>=0)] [--help: instructions]"
    exit 1
}

#controllo il numero degli argomenti passati
if [[ $# != 4 ]]; then
    usage
fi

#prendo il path del file di configurazione ed il tempo
conf_path=""
time=-1
while getopts ":f:t:" opt; do
    case "${opt}" in
        f)
            conf_path=${OPTARG}
            ;;
        t)
            time=${OPTARG}
            ;;
        *)
            usage
            ;;
    esac
done

#controllo che il file di configuarazione esista
if ! [[ -s $conf_path ]]; then
    echo "Errore: il file di configurazione passato non esiste"
    usage
fi

#controllo che il parametro t sia >= 0
if (( $time < 0 )); then
    echo "Errore: il tempo inserito deve essere >= 0"
    usage
fi


#cerco la linea che contiente DirName nel file di configurazione (e che non sia un commento)
linea=$(grep "DirName" $conf_path | grep -v "#")
#rimuovo gli spazi nella linea
linea=$(echo $linea | tr -d ' ')
#prendo il nome specificato da DirName dopo '='
dirname=${linea##*=}

#controllo che dirname non sia vuoto
if [[ $dirname == "" ]]; then
    echo "Errore: nel file di configurazione non è specificata nessuna voce DirName"
    usage
fi

#controllo che la cartella DirName esista
if ! [[ -d $dirname ]]; then
    echo "Errore: la cartella $dirname non esiste"
    usage
fi


if (( $time == 0 )); then
    #devo solo stampare i file
    for FILE in $dirname/* ; do
        echo "${FILE##*/}"
    done
else 
    #cerco tutti i file più vecchi di t minuti
    files=()
    i=0
    for FILE in $dirname/* ; do
        if [[ $(find "$FILE" -mmin +$time) ]]; then
            files[$i]=$FILE
            (( i++ ))	
        fi
    done

    if (( ${#files[@]} != 0 )); then
        #path dell'archivio
        scriptpath=$(dirname "$0")
        archivio="$scriptpath/archivio.tar.gz"
        #creo l'archivio
        tar cfz $archivio ${files[*]}

        #elimino i file archiviati
        for FILE in ${files[*]} ; do
            #se directory
            if [[ -d $FILE ]]; then
                rm -fr "${FILE}"
            #se file
            else 
                rm -f "${FILE}"
            fi
        done
    fi
fi


echo "**** $0 eseguito correttamente!"
exit 0
