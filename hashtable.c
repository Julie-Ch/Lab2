#include "xerrori.h"
#include "hashtable.h"

//funzione che crea una entry per la tabella hash key - value
ENTRY *crea_entry(char *s, int n) {
  ENTRY *e = malloc(sizeof(ENTRY));
  if(e==NULL) xtermina("errore malloc entry 1", __LINE__,__FILE__);
  e->key = strdup(s);  //salva copia di s
  e->data = (int *) malloc(sizeof(int));
  if(e->key==NULL || e->data==NULL)
    termina("errore malloc entry 2");
  *((int *)e->data) = n;
  return e;
}

//funzione che libera la memoria di una entry
void distruggi_entry(ENTRY *e)
{
  free(e->key);
  free(e->data);
  free(e);
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
    assert(strcmp(e->key,r->key)==0); //controllo se la entry esiste davvero
      int *d = (int *)r->data; //con puntatore cambio valore
      *d +=1;
      distruggi_entry(e); // questa non la devo memorizzare
  }
}

//funzione che restituisce valore associato ad una entry nella tabella hash
int conta(char *s){

  //creo una entry per poter usare la funzione hsearch con FIND
  ENTRY *e = crea_entry(s, 1);
  ENTRY *r = hsearch(*e, FIND);
  if(r == NULL) {
    //se non la trovo ritorno 0 e distruggo la entry
    distruggi_entry(e);
    return 0;
    }else{
    //altirmenti ritorno il valore associato e distruggo la entry
    distruggi_entry(e);
    return (*((int *)r->data));
    }
}

//funzione che inizializza una tabella hash
void table_init(tabella_hash *tab){

    *(tab->lettori_tabella) = 0;
    *(tab->dati_aggiunti) = 0;
    xpthread_mutex_init(tab->mutabella,NULL,__LINE__,__FILE__);
    xpthread_cond_init(tab->condStabella,NULL,__LINE__,__FILE__);
}

//funzione che distrugge mutex e cv di una tabella hash
void table_destroy(tabella_hash *tab){
  xpthread_mutex_destroy(tab->mutabella, __LINE__, __FILE__);
  xpthread_cond_destroy(tab->condStabella, __LINE__, __FILE__);
}

void readtable_access(tabella_hash *tab){

  //per modificare il numero di lettori nella tabella acquisisco la lock
  xpthread_mutex_lock(tab->mutabella, __LINE__,__FILE__);
  (*(tab->lettori_tabella))++;
  xpthread_mutex_unlock(tab->mutabella, __LINE__,__FILE__);
}

//funzione che monitora uscita lettori consumatori dalla tabella
void readtable_exit(tabella_hash *tab){
  assert(*(tab->lettori_tabella)>0);  // ci deve essere almeno un reader (me stesso)
  //per modificare il numero di lettori nella tabella acquisisco la lock
  xpthread_mutex_lock(tab->mutabella, __LINE__, __FILE__);
  (*(tab->lettori_tabella))--;                  // cambio di stato
  //se ci sono scrittori in attesa, li sveglio       
  xpthread_cond_broadcast(tab->condStabella,__LINE__, __FILE__);
  xpthread_mutex_unlock(tab->mutabella,__LINE__,__FILE__);
}
  
//funzione che permette ad uno scrittore consumatore di accedere alla tabella
void writetable_lock(tabella_hash *tab){

    //per leggere il numero di lettori nella tabella acquisisco la lock
   xpthread_mutex_lock(tab->mutabella, __LINE__,__FILE__);
    while((*(tab->lettori_tabella)) > 0)
      //attende che tutti i lettori abbiano finito di leggere
      xpthread_cond_wait(tab->condStabella, tab->mutabella,__LINE__,__FILE__);   

}

//funzione che permette ad uno scrittore consumatore di uscire dalla tabella
void writetable_unlock(tabella_hash *tab){
  xpthread_mutex_unlock(tab->mutabella,__LINE__,__FILE__);
}