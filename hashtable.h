#include "xerrori.h"
#define QUI "__LINE__,__FILE__"

//prototipi delle funzioni in hashtable.c

//struttura dati per la gestione della tabella hash
typedef struct {
  int *dati_aggiunti;               //numero totale delle stringhe aggiunte alla tabella
  int *lettori_tabella;             //lettori nella tabella 
  int *scrittori_tabella_attesa;    //scrittori in attesa
  bool *scrittori_tabella;          //presenza scrittore in tabella
  pthread_mutex_t *mutabella;       //mutex per la tabella hash
  pthread_cond_t *condStabella;     //cv per scrittori
  pthread_cond_t *condLtabella;     //cv per lettori
} tabella_hash;

//funzione che crea una entry per la tabella hash
ENTRY *crea_entry(char *s, int n);

//funzione che distrugge una entry della tabella hash
void distruggi_entry(ENTRY *e);

//funzione che aggiunge una entry alla tabella hash
void aggiungi(char *s, tabella_hash *tab);

//funzione che restituisce valore associato ad una entry nella tabella hash
int conta(char *s);

//funzione che inizializza una tabella hash
void table_init(tabella_hash *tab);

//funzione che distrugge una tabella hash
void table_destroy(tabella_hash *tab);

//funzione che permette al lettore l'accesso alla tabella
void readtable_lock(tabella_hash *tab);

//funzione che permette ad un lettore di uscire dalla tabella e segnalare la sua uscita
void readtable_unlock(tabella_hash *tab);
  
//funzione che permette ad uno scrittore di accedere alla tabella
void writetable_lock(tabella_hash *tab);

//funzione che permette ad uno scrittore di uscire dalla tabella e segnalare la sua uscita
void writetable_unlock(tabella_hash *tab);