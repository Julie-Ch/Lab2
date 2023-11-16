#include "xerrori.h"
#define QUI "__LINE__,__FILE__"

typedef struct {
  int *dati_aggiunti;
  int *lettori_tabella;             //lettori nella tabella 
  int *scrittori_tabella_attesa;    //scrittori in attesa
  bool *scrittori_tabella;          //presenza scrittore in tabella
  pthread_mutex_t *mutabella;       //mutex per la tabella hash
  pthread_cond_t *condStabella;     //cv per scrittori
  pthread_cond_t *condLtabella;     //cv per lettori
} tabella_hash;

//funzione che crea una entry per la tabella hash
ENTRY *crea_entry(char *s, int n);

void distruggi_entry(ENTRY *e);

//funzione che aggiunge una entry alla tabella hash
void aggiungi(char *s, tabella_hash *tab);

//funzione che restituisce valore associato ad una entry nella tabella hash
int conta(char *s);

void table_init(tabella_hash *tab);

void table_destroy(tabella_hash *tab);

void readtable_lock(tabella_hash *tab);

void readtable_unlock(tabella_hash *tab);
  
// inizio uso da parte di writer  
void writetable_lock(tabella_hash *tab);

// fine uso da parte di un writer
void writetable_unlock(tabella_hash *tab);