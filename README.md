# Relazione del Progetto di Laboratorio 2 di Chicca Julie

## Strutture dati utilizzate all'interno di Archivio.c ed in hashtable.c

**dati_capo**  : questa struttura dati viene passata come argomento alla **void *capo_lett_body(void *arg)** e alla
**void *capo_scritt_body(void *arg)** in modo che in una sola struttura ho tutte le informazioni che riguardano il 
capo lettore/scrittore.

**dati_consumatori** : questa struttura dati viene passata come argomento alla **void *consumer_lett_body(void *arg)** e 
alla **void *consumer_lett_body(void *arg)** in modo che in una sola struttura ho tutte le informazioni che riguardano il 
consumatore lettore/scrittore.

**tabella_hash** : questa struttura dati serve per la gestione dell'accesso alla tabella hash e per tenere traccia
del numero totale di dati aggiunti alla tabella