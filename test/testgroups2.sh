#!/bin/bash

if [[ $# != 1 ]]; then
    echo "usa $0 unix_path"
    exit 1
fi

rm -f valgrind_out
/usr/bin/valgrind --leak-check=full ./chatty -f DATA/chatty.conf1 >& ./valgrind_out &
pid=$!

# aspetto un po' per far partire valgrind
sleep 5

# registro un po' di nickname
./client -l $1 -c pippo &
clientpid+="$! "
./client -l $1 -c pluto &
clientpid+="$! "
./client -l $1 -c minni &
clientpid+="$! "
./client -l $1 -c topolino &
clientpid+="$! "
./client -l $1 -c paperino &
clientpid+="$! "
./client -l $1 -c qui &
clientpid+="$! "

wait $clientpid

# creo il gruppo1
./client -l $1 -k pippo -g gruppo1
if [[ $? != 0 ]]; then
    exit 1
fi
# creo il gruppo2
./client -l $1 -k pluto -g gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi

# aggiungo minni al gruppo1
./client -l $1 -k minni -a gruppo1
if [[ $? != 0 ]]; then
    exit 1
fi
# aggiungo topolino al gruppo1
./client -l $1 -k topolino -a gruppo1
if [[ $? != 0 ]]; then
    exit 1
fi
# aggiungo paperino al gruppo2
./client -l $1 -k paperino -a gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi
# aggiungo minni al gruppo2
./client -l $1 -k minni -a gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi

# pippo manda un messaggio a tutti gli iscritti al gruppo1 (pippo, minni e topolino)
./client -l $1  -k pippo -S "Ciao a tutti da pippo":gruppo1 -R 1
if [[ $? != 0 ]]; then
    exit 1
fi
# pluto manda un messaggio a tutti gli iscritti al gruppo2 (pluto, paperino e minni)
./client -l $1  -k pluto -S "Ciao a tutti da pluto":gruppo2 -R 1
if [[ $? != 0 ]]; then
    exit 1
fi

# messaggio di errore che mi aspetto dal prossimo comando
# l'utente qui non puo' mandare il messaggio perche' non e' inscritto al gruppo1
OP_NICK_UNKNOWN=27
./client -l $1  -k qui   -S "Ciao sono qui":gruppo1
e=$?
if [[ $((256-e)) != $OP_NICK_UNKNOWN ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

# minni manda un file a tutti gli utenti dei gruppi a cui appartiene
./client -l $1  -k minni -S "Vi mando un file":gruppo1 -s ./libchatty.a:gruppo1 -s ./libchatty.a:gruppo2 -S "Vi ho mandato un file":gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi

# ricevo i messaggi che mi sono perso
./client -l $1 -k pippo -p
if [[ $? != 0 ]]; then
    exit 1
fi
./client -l $1 -k pluto -p
if [[ $? != 0 ]]; then
    exit 1
fi
./client -l $1 -k minni -p
if [[ $? != 0 ]]; then
    exit 1
fi
./client -l $1 -k topolino -p
if [[ $? != 0 ]]; then
    exit 1
fi

./client -l $1 -k topolino -d gruppo1
if [[ $? != 0 ]]; then
    exit 1
fi

./client -l $1 -k minni -d gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi


sleep 1

# invio il segnale per generare le statistiche
kill -USR1 $pid

sleep 1

#---------------------------------------------------------------------------------------------

# messaggio di errore che mi aspetto dal prossimo comando
# gruppo già esistente
OP_NICK_ALREADY=26
./client -l $1 -k topolino -g gruppo1
e=$?
if [[ $((256-e)) != $OP_NICK_ALREADY ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

# messaggio di errore che mi aspetto dal prossimo comando
# impossibile creare un gruppo con il nome di un utente già esistente
./client -l $1 -k topolino -g minni
e=$?
if [[ $((256-e)) != $OP_NICK_ALREADY ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

# messaggio di errore che mi aspetto dal prossimo comando
# impossibile creare un utente con il nome di un gruppo già esistente
./client -l $1 -c gruppo1
e=$?
if [[ $((256-e)) != $OP_NICK_ALREADY ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

# messaggio di errore che mi aspetto dal prossimo comando
# impossibile riaggiungersi ad un gruppo di cui si è già membri
OP_FAIL=25
./client -l $1 -k minni -a gruppo1
e=$?
if [[ $((256-e)) != $OP_FAIL ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

# messaggio di errore che mi aspetto dal prossimo comando
# impossibile eliminarsi due volte da un gruppo
./client -l $1 -k minni -d gruppo2
e=$?
if [[ $((256-e)) != $OP_NICK_UNKNOWN ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

# rimuovo l'utente creatore del gruppo (il gruppo viene cancellato)
./client -l $1 -k pluto -d gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi

# messaggio di errore che mi aspetto dal prossimo comando
# paperino prova a mandare un messaggio al gruppo2 che è stato cancellato sopra
./client -l $1  -k pluto -S "Ciao a tutti da paperino":gruppo2
if [[ $((256-e)) != $OP_NICK_UNKNOWN ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

sleep 1

# invio il segnale per generare le statistiche
kill -USR1 $pid

sleep 1


# creo il gruppo2 
./client -l $1 -k pippo -g gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi
# creo il gruppo3
./client -l $1 -k pluto -g gruppo3
if [[ $? != 0 ]]; then
    exit 1
fi

# aggiungo minni al gruppo2
./client -l $1 -k minni -a gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi
# aggiungo topolino al gruppo2
./client -l $1 -k topolino -a gruppo2
if [[ $? != 0 ]]; then
    exit 1
fi
# aggiungo paperino al gruppo3
./client -l $1 -k paperino -a gruppo3
if [[ $? != 0 ]]; then
    exit 1
fi
# aggiungo minni al gruppo3
./client -l $1 -k minni -a gruppo3
if [[ $? != 0 ]]; then
    exit 1
fi

sleep 1

# invio il segnale per generare le statistiche
kill -USR1 $pid

sleep 1

# de-registro pippo che è creatore di gruppo1 e gruppo2 che vengono così cancellati
./client -l $1 -k pippo -C pippo
if [[ $? != 0 ]]; then
    exit 1
fi

# messaggio di errore che mi aspetto dal prossimo comando
# impossibile eliminarsi da un gruppo cancellato (sopra)
./client -l $1 -k topolino -d gruppo2
e=$?
if [[ $((256-e)) != $OP_NICK_UNKNOWN ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

# messaggio di errore che mi aspetto dal prossimo comando
# impossibile aggiungersi ad un gruppo cancellato (sopra)
./client -l $1 -k paperino -a gruppo1
e=$?
if [[ $((256-e)) != $OP_NICK_UNKNOWN ]]; then
    echo "Errore non corrispondente $e" 
    exit 1
fi

sleep 1

# invio il segnale per generare le statistiche
kill -USR1 $pid

sleep 1

# invio il segnale per far terminare il server
kill -TERM $pid

sleep 2

r=$(tail -10 ./valgrind_out | grep "ERROR SUMMARY" | cut -d: -f 2 | cut -d" " -f 2)

if [[ $r != 0 ]]; then
    echo "Test FALLITO"
    exit 1
fi


echo "Test OK!"
exit 0