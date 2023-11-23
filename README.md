# Relazione del Progetto di Laboratorio 2 di Chicca Julie

## Strutture dati utilizzate all'interno di Archivio.c ed in hashtable.c

**dati_capo**  : questa struttura dati viene passata come argomento alla **void *capo_lett_body(void *arg)** e alla
**void *capo_scritt_body(void *arg)** in modo che in una sola struttura ho tutte le informazioni che riguardano il 
capo lettore/scrittore.

**dati_consumatori** : questa struttura dati viene passata come argomento alla **void *consumer_lett_body(void *arg)** e 
alla **void *consumer_lett_body(void *arg)** in modo che in una sola struttura ho tutte le informazioni che riguardano il 
consumatore lettore/scrittore.

**tabella_hash** : questa struttura dati serve per la gestione dell'accesso alla tabella hash e per tenere traccia
del numero totale di dati aggiunti alla tabella e dei lettori attualmente in tabella.

## Logica di accesso alla tabella hash

Alla tabella hash accedono i consumatori lettori e i consumatori scrittori, rispettivamente per chiamare la funzione **conta** e la procedura **aggiungi**.
Quando un lettore deve chiamare conta, prima chiama **readtable_lock**, in modo che possa incrementare in modo safe il numero di lettori presenti (lettori_tabella). Poi, rilascia subito la lock poichè deve soltanto leggere, e non scrivere, modificando la tabella. 
Successivamente chiama **readtable_unlock** per decrementare lettori_tabella e per svegliare, se ci sono, scrittori in attesa, che concorreranno per acquisire la lock.

Quando invece uno scrittore deve chiamare aggiungi, mantiene acquisita la lock(chiamando **writetable_lock**) per tutta la durata del suo accesso, poichè deve modificare la tabella e la variabile **dati_aggiunti**. Chiamando writetable_lock inoltre, lo scrittore aspetta finchè non ci sono più lettori presenti nella tabella, per poi acquisire la lock. Al termine della scrittura, chiamando **writetable_unlock**, esce dalla tabella e rilascia la lock.

## Gestione della connesione nel server.py

La funzione gestisci_connessione gestisce una singola connessione con un client. Questa funzione prende come parametri una connessione, un indirizzo, e tre file descriptors: le due FIFO e il Pid dell'Archivio.

Il server riceve un byte dal client che descrive il tipo di connessione, che può essere 'A' o 'B'.

Se il tipo di connessione è 'A':
-Invia un byte di conferma/ack al client.
-Riceve la lunghezza dell’input e la riga del file dal client.
-Invia questi dati sulla FIFO capolet.

Se il tipo di connessione è 'B':
-Entra in un ciclo infinito in cui continua a ricevere dati dal client fino a quando non riceve una lunghezza di zero, che indica la fine della trasmissione.
-Per ogni ciclo, invia un byte di conferma/ack al client, riceve la lunghezza dell’input e la riga del file, e invia questi dati sulla FIFO caposc.

Se il tipo di connessione non è né 'A' né 'B', stampa un messaggio che indica che il tipo di connessione non è supportato.

## Gestione della connessione nel client1

Client1 si connette al server ed invia tutte le righe di un file di testo. Il client stabilisce una connessione di tipo 'A' per ogni riga del file.

Nel main apre il file specificato, legge ogni riga e stabilisce una connessione con il server.
Per ogni connessione:
- Invia un byte che indica il tipo di connessione ('A').
- Controlla se la lunghezza della riga supera il limite massimo (2048 byte). Se la riga è troppo lunga, esce.
- Riceve un byte di conferma/ack dal server.
- Invia la lunghezza della riga e la riga stessa al server.

## Gestione della connessione nel client2

Client2 si connette a un server e invia righe di un file di testo. Il client2 stabilisce una connessione di tipo 'B' per ogni file di testo.

Nel main stabilisce connessione con il server.
Per ogni connessione:
- Invia un byte che indica il tipo di connessione ('B').
- Apre il file e per ogni riga del file, riceve un byte di conferma/ack dal server, controlla se la lunghezza della riga supera il limite massimo (2048 byte), e se la riga non è troppo lunga, invia la lunghezza della riga e la riga stessa al server.
- Al termine del file, riceve byte di ack, ed invia una lunghezza di 0 per comunicare la fine del file.

Fuori dal main, viene creato un ThreadPoolExecutor. Il numero massimo di thread nel pool è il numero di file passati come argomento. Itera su ogni argomento passato e per ogni file, sottomette una chiamata alla funzione main, passando il file come parametro. Questo avvia un nuovo thread per ogni file passato come argomento, e ogni thread esegue la funzione main su un file diverso.






