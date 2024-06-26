# Relazione del Progetto di Laboratorio 2 di Chicca Julie

## File che compongono il progetto

- **hashtable.c (e hashtable.h)** : collezione di funzioni (e header) per gestire creazione e distruzione di entry, accesso e modifica di una tabella hash.
- **archivio.c** : programma C per la gestione dell'archivio.
- **server.py** : programma in Python che implementa il server.
- **client1** : programma in Python che implementa il client1.
- **client2** : programma in Python che implementa il client2.
- **Makefile** : makefile per compilare archivio.c, hashtable.c, xerrori.c.
- **xerrori.c (e xerrori.h)** : collezione di funzioni (e header) come da lezione per gestire gli errori durante le chiamate di funzioni.
- **file1, file2, file3** : file forniti per testare il progetto.

## File prodotti dall'esecuzione dei programmi

- **error_file.txt** : file dove viene ridiretto lo stderr del programma archivio.c. 
- **server.log** : file di log dove vengono monitorate le righe inviate per ogni connessione. 
- **lettori.log** : file dove i lettori scrivono il risultato delle loro interrogazioni all'archivio.
- **valgrind-xxxxxxx** : file di log prodotto dall'esecuzione di archivio con valgrind (con "xxxxxxx" PID del processo archivio).

Tutti i file (tranne quello relativo a valgrind) vengono sovrascritti ad ogni esecuzione. Presenteranno quindi solo i dati relativi all'ultima esecuzione. Per quanto riguarda quello relativo a valgrind, ne viene generato uno nuovo ad ogni esecuzione

## Strutture dati utilizzate all'interno di Archivio.c ed in hashtable.c

**dati_capo**  : questa struttura dati viene passata come argomento alla **capo_lett_body** e alla
**capo_scritt_body** in modo che in una sola struttura ho tutte le informazioni che riguardano il 
capo lettore/scrittore.

```c
typedef struct {
    char **buffer;                    //buffer di stringhe prod/cons
    int *ppindex;                     //primo indice disponibile per produttore
    sem_t *sem_free_slots;            //semaforo per attesa produttore
    sem_t *sem_data_items;            //semaforo per attesa consumatori
    int aux;                          //numero dei thread ausiliari
    tabella_hash *dati_tab;           //puntatore alla struttura dati relativa alla tabella hash 
} dati_capo;  
```       

**dati_consumatori** : questa struttura dati viene passata come argomento alla **consumer_lett_body** e 
alla **consumer_lett_body** in modo che in una sola struttura ho tutte le informazioni che riguardano il 
consumatore lettore/scrittore.

```c
typedef struct {
char **buffer;                    //buffer di stringhe prod/cons
int *pcindex;                     //primo indice disponibile per i consumatori
sem_t *sem_free_slots;            //semaforo per attesa produttore nel buffer
sem_t *sem_data_items;            //semaforo per attesa consumatori nel buffer
pthread_mutex_t *mutex;           //mutex che gestisce conflitti tra consumatori nel buffer
pthread_mutex_t *mutexlog;        //mutex per scrivere nel file di log
FILE *outfile;                    //puntatore al file di log per i lettori
tabella_hash *dati_tab;           //puntatore alla struttura dati relativa alla tabella hash 
} dati_consumatori;   
```

**tabella_hash** : questa struttura dati serve per la gestione dell'accesso alla tabella hash e per tenere traccia
del numero totale di dati aggiunti alla tabella e dei lettori attualmente in tabella.

```c
typedef struct {
int *dati_aggiunti;               //numero totale delle stringhe aggiunte alla tabella
int *lettori_tabella;             //lettori nella tabella 
pthread_mutex_t *mutabella;       //mutex per accesso alla tabella hash e ai dati (sopra)
pthread_cond_t *condStabella;     //cv per scrittori
} tabella_hash;
```
**dati_gestore** : questa struttura dati viene passata come argomento a **gestione** in modo che in una sola struttura ho tutte le informazioni che mi servono per gestire i thread capi e i segnali.

```c
//struttura dati da passare come argomento al thread gestore per gestire i capi e la tabella hash
typedef struct{
  pthread_t *capo_lettore;          //ID del thread capo_lettore per fare la join
  pthread_t *capo_scrittore;        //ID del thread capo_scrittore per fare la join
  tabella_hash *dati_tab;           //puntatore alla struttura dati relativa alla tabella hash
} dati_gestore;
```

## Gestione della connesione nel server.py

La funzione **gestisci_connessione** gestisce una singola connessione con un client. Questa funzione prende come parametri una connessione, un indirizzo, e tre file descriptors: le due FIFO e il Pid dell'archivio.

Il server riceve un byte dal client che descrive il tipo di connessione, che può essere 'A' o 'B':

- Se il tipo di connessione è 'A':
Riceve la lunghezza dell’input e la riga del file dal client.
Invia questi dati sulla FIFO capolet.

- Se il tipo di connessione è 'B':
Entra in un ciclo infinito in cui continua a ricevere dati dal client fino a quando non riceve una lunghezza di zero, che indica la fine della trasmissione.
Per ogni iterazione del ciclo, riceve la lunghezza dell’input e la riga del file, e invia questi dati sulla FIFO caposc.

- Se il tipo di connessione non è né 'A' né 'B':
Stampa un messaggio che indica che il tipo di connessione non è supportato.

## Gestione della connessione nel client1

**client1** si connette al server ed invia tutte le righe di un file di testo. Il client stabilisce una connessione di tipo 'A' per ogni riga del file.

Nel main apre il file specificato, legge ogni riga e stabilisce una connessione con il server.
Per ogni connessione:
- Invia un byte che indica il tipo di connessione ('A').
- Controlla se la lunghezza della riga supera il limite massimo (2048 byte). Se la riga è troppo lunga, esce.
- Invia la lunghezza della riga e la riga stessa al server sulla FIFO con la lock acquisita, poichè ci sono più thread che potrebbero scrivere sulla FIFO contemporaneamente.

## Gestione della connessione nel client2

**client2** si connette al server e invia tutte le righe di un file di testo. Il client2 stabilisce una connessione di tipo 'B' per ogni file di testo.

Nel main stabilisce connessione con il server.
Per ogni connessione:
- Invia un byte che indica il tipo di connessione ('B').
- Apre il file e per ogni riga del file, controlla se la lunghezza della riga supera il limite massimo (2048 byte), e se la riga non è troppo lunga, invia la lunghezza della riga e la riga stessa al server con la lock acquisita, poichè ci sono più thread che potrebbero scrivere sulla FIFO contemporaneamente.
- Al termine del file, invia una lunghezza di 0 per comunicare la fine del file.

Fuori dal main, viene creato un ThreadPoolExecutor. Il numero massimo di thread nel pool è il numero di file passati come argomento. Itera su ogni argomento passato e per ogni file, sottomette una chiamata alla funzione main, passando il file come parametro. Questo avvia un nuovo thread per ogni file passato come argomento, e ogni thread esegue la funzione main su un file diverso.

## archivio.c

Il programma **archivio** utilizza un modello produttore-consumatori multi-thread, con produttore il capo e come consumatori i thread ausiliari. Dal server arrivano le stringhe: se sono stringhe "da leggere", quindi su cui chiamare la funzione conta, il server le invia sulla FIFO capolet (al capo lettore), se sono stringhe da inserire nella tabella le invia sulla FIFO caposc (al capo scrittore). 

Nel main, dopo aver inizializzato le varie strutture dati per gestire i capi, la tabella hash e il gestore dei segnali, fa partire i capi ed il gestore, attendendo la terminazione del gestore. Blocca inoltre tutti i segnali per consentire solo al gestore di gestirle.

Ognuno dei capi esegue sostanzialmente le solite operazioni:

- Estrae i dati dal parametro arg e inizializza vari mutex e array per i thread consumatori ausiliari.

- Vengono creati i thread consumatori ausiliari, ognuno dei quali ha la sua struttura dati (vedere **dati_consumatore**)

- Viene aperta una FIFO in sola lettura e comincia la lettura. Itera nel ciclo finche la FIFO non viene chiusa in scrittura dal server. Leggo un intero **dim** e una stringa **max** lunga dim. Poi, per ogni stringa tokenizzata, il capo produttore, con una wait su **a->sem_free_slots**, aspetta che ci siano slot liberi per inserire un elemento e dopo aver scritto, segnala con una post su **a->sem_data_items** la presenza di elementi nel buffer. Simmetricamente, i consumatori aspettano con una wait su **a->sem_data_items**che ci sia almeno un elemento da prelevare ma, essendoci tanti consumatori, acquisisce anche la lock del buffer prima di scriverci. Successivamente segnala uno slot libero con una post su **a->sem_free_slots**.

- Quando la FIFO viene chiusa, viene scritto NULL nel buffer per ogni thread consumatore per segnalare che non ci sono più dati da consumare e si attende che tutti i thread lettori terminino.

Nel main e negli altri thread blocco tutti i segnali, per far sì che sia il thread gestore a gestirli.

Il thread gestore esegue **gestore_body** in un thread separato e gestisce i segnali in un ciclo continuo. Imposta **gestore** come funzione invocata dall'handler per SIGINT e **gestore_term**  per SIGTERM. Poi entra in un ciclo infinito in cui aspetta un segnale. Quando riceve un segnale, acquisisce la lock della tabella, aggiorna dati_aggiunti_global con il valore corrente di dati_tab->dati_aggiunti, rilascia la lock, e poi solleva il segnale per invocare il gestore appropriato. Se il segnale è SIGTERM, esce dal ciclo. Se il segnale non è né SIGINT né SIGTERM, imposta l’handler di default per quel segnale e poi lo solleva. Dopo aver terminato il ciclo, attende che i thread capo_lettore e capo_scrittore terminino. Per rendere le procedure assegnate all'handler più async-signal-safe possibile, ho usato solo funzioni async-signal-safe come la write e non ho formattato molto la stringa restituita, stampando solo il numero di stringhe nella tabella. Ho aggiunto la perror che non è safe solo per togliere un warning, ma è improbabile che ci siano errori della write su stdout e stderr. Inoltre sto usando una variabile globale volatile sig_atomic, ho fatto questa scelta per poter passare alle procedure il valore dei dati aggiunti e per rendere le operazioni di lettura e scrittura atomiche, non interrompibili. Inoltre, viene modificata solo dal thread gestore proprio per poter passare un dato consistente, quindi non ci dovrebbe essere bisogno di una ulteriore lock dedicata.

## Logica di accesso alla tabella hash

Alla tabella hash accedono i consumatori lettori e i consumatori scrittori, rispettivamente per chiamare la funzione **conta** e la procedura **aggiungi**. Quando un lettore deve chiamare conta, prima chiama **readtable_access**, funzione che permette di incrementare in modo safe il numero di lettori presenti (**lettori_tabella**) acquisendo la lock. Se c'è uno scrittore in tabella si mette in attesa, e poi incrementa il numero di lettori. Poi, rilascia subito la lock poichè deve soltanto leggere, e non scrivere modificando la tabella. Successivamente chiama **readtable_exit** per decrementare lettori_tabella e per svegliare, se ci sono e se non ci sono lettori, scrittori in attesa.

Quando uno scrittore deve chiamare aggiungi, chiamando **writetable_lock**, acquisisce la lock e attende finchè non ci sono più lettori e scrittore presenti nella tabella. Successivamente modifica **writing** e rilascia la lock. Al termine della scrittura, chiamando **writetable_unlock**, acquisisce la lock e modifica **writing**, segnala a tutti quelli in attesa e rilascia la lock.







