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





