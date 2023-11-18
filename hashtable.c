#include "xerrori.h"
#include "hashtable.h"

//funzioni per la gestione della tabella hash e degli accessi da parte dei lettori / scrittori

//funzione che crea una entry per la tabella hash
ENTRY *crea_entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY));
  if(e==NULL) xtermina("errore malloc entry 1", __LINE__,__FILE__);
  e->key = strdup(s); // salva copia di s
  e->data = (int *) malloc(sizeof(int));
  if(e->key==NULL || e->data==NULL)
    termina("errore malloc entry 2");
  *((int *)e->data) = n;
  return e;
}

//funzione che distrugge una entry della tabella hash
void distruggi_entry(ENTRY *e)
{
  free(e->key); free(e->data); free(e);
}

//funzione che aggiunge una entry alla tabella hash
void aggiungi(char *s, tabella_hash *tab){

  ENTRY *e = crea_entry(s, 1);
  ENTRY *r = hsearch(*e, FIND);
  if(r == NULL){
    r = hsearch(*e, ENTER);
    if(r == NULL) xtermina("errore aggiungi o ht piena", __LINE__,__FILE__);
    (*(tab->dati_aggiunti))++;
  }else{
    assert(strcmp(e->key,r->key)==0); //controllo che sia la entry già esistente
      int *d = (int *)r->data; //con puntatore cambio valore 
      *d +=1;
      distruggi_entry(e); // questa non la devo memorizzare
  }
}

//funzione che restituisce valore associato ad una entry nella tabella hash
int conta(char *s){

  ENTRY *e = crea_entry(s, 1);
  ENTRY *r = hsearch(*e, FIND);
  if(r == NULL) return 0;
  else {
    distruggi_entry(e);
    return (*((int *)r->data));
    }
}

//funzione che inizializza una tabella hash
void table_init(tabella_hash *tab){

    *(tab->lettori_tabella) = 0;
    *(tab->dati_aggiunti) = 0;
    *(tab->scrittori_tabella_attesa) = 0;
    *(tab->scrittori_tabella) = false;
    xpthread_mutex_init(tab->mutabella,NULL,__LINE__,__FILE__);
    xpthread_cond_init(tab->condStabella,NULL,__LINE__,__FILE__);
    xpthread_cond_init(tab->condLtabella,NULL,__LINE__,__FILE__);
}

//funzione che distrugge una tabella hash
void table_destroy(tabella_hash *tab){
  xpthread_mutex_destroy(tab->mutabella, __LINE__, __FILE__);
  xpthread_cond_destroy(tab->condStabella, __LINE__, __FILE__);
  xpthread_cond_destroy(tab->condLtabella, __LINE__, __FILE__);
}

//funzione che permette al lettore l'accesso alla tabella
void readtable_lock(tabella_hash *tab){

  xpthread_mutex_lock(tab->mutabella, __LINE__,__FILE__);
  while((*(tab->scrittori_tabella)) == true || (*(tab->scrittori_tabella_attesa)) > 0)
    xpthread_cond_wait(tab->condLtabella, tab->mutabella, __LINE__, __FILE__);   // attende fine scrittura
  (*(tab->lettori_tabella))++;
  xpthread_mutex_unlock(tab->mutabella, __LINE__,__FILE__);

}

//funzione che permette ad un lettore di uscire dalla tabella e segnalare la sua uscita
void readtable_unlock(tabella_hash *tab){

  assert(*(tab->lettori_tabella)>0);  // ci deve essere almeno un reader (me stesso)
  xpthread_mutex_lock(tab->mutabella, __LINE__, __FILE__);
  (*(tab->lettori_tabella))--;                  // cambio di stato       
  if(*(tab->lettori_tabella)==0) 
    xpthread_cond_signal(tab->condStabella,__LINE__, __FILE__); // da segnalare ad un solo writer
  xpthread_mutex_unlock(tab->mutabella,__LINE__,__FILE__);
}
  
//funzione che permette ad uno scrittore di accedere alla tabella
void writetable_lock(tabella_hash *tab){

   xpthread_mutex_lock(tab->mutabella, __LINE__,__FILE__);
    while(*(tab->scrittori_tabella) || (*(tab->lettori_tabella))>0)
    // attende fine scrittura o lettura
      xpthread_cond_wait(tab->condStabella, tab->mutabella,__LINE__,__FILE__);   
    (*(tab->scrittori_tabella)) = true;
    (*(tab->scrittori_tabella_attesa))--;
    xpthread_mutex_unlock(tab->mutabella, __LINE__,__FILE__);
}

//funzione che permette ad uno scrittore di uscire dalla tabella e segnalare la sua uscita
void writetable_unlock(tabella_hash *tab){

  assert(*(tab->scrittori_tabella));
  xpthread_mutex_lock(tab->mutabella,__LINE__,__FILE__);
  (*(tab->scrittori_tabella)) = false;        
  if((*(tab->scrittori_tabella_attesa))>0)
    xpthread_cond_signal(tab->condStabella,__LINE__,__FILE__);
  else       
    xpthread_cond_broadcast(tab->condLtabella,__LINE__,__FILE__);  
  xpthread_mutex_unlock(tab->mutabella,__LINE__,__FILE__);
}