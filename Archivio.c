#include "xerrori.h"
#include "hashtable.h"
#include <sys/syscall.h>

#define Num_elem 1000000            //dimensione tabella hash
#define PC_buffer_len 10            //lunghezza bugger prod/cons
#define Max_sequence_length 2048    //massima lunghezza sequenza inviata attraverso una pipe

pid_t gettid(void)
{
    #ifdef __linux__
      // il tid è specifico di linux
    pid_t tid = (pid_t)syscall(SYS_gettid);
    return tid;
  #else
      // per altri OS restituisco -1
      return -1;
  #endif
}

//strutture di supporto

typedef struct {
  char **buffer;              //buffer di stringhe prod/cons
  int *ppindex;               //primo indice disponibile per produttore
  sem_t *sem_free_slots;      //sem per attesa produttore
  sem_t *sem_data_items;      //sem per attesa consumatori
  int aux;                    //numero ausiliari
  tabella_hash *dati_tab;
} dati_capo;    

typedef struct {
  char **buffer;                    //buffer di stringhe prod/cons
  int *pcindex;                     //primo indice disponibile
  sem_t *sem_free_slots;            //semaforo per attesa produttore nel buffer
  sem_t *sem_data_items;            //semaforo per attesa consumatori nel buffer
  pthread_mutex_t *mutex;           //mutex che gestisce conflitti tra consumatori nel buffer
  pthread_mutex_t *mutexlog;        //mutex per scrivere nel file di log
  FILE *outfile;                    //puntatore al file di output per i lettori
  tabella_hash *dati_tab;           //puntatore alla struttura dati relativa alla tabella hash 
} dati_consumatori;                 //thread ausiliari


//struttura dati da passare come argomento al thread gestore per gestire i capi e la tabella hash
typedef struct{

  pthread_t *capo_lettore;          
  dati_capo *dati_capo_lettore;
  pthread_t *capo_scrittore;
  dati_capo *dati_capo_scrittore;
  tabella_hash *dati_tab;

} dati_gestore;

//corpo del capo lettore
void *consumer_lett_body(void *arg){

  dati_consumatori *a = (dati_consumatori*)arg;
  char *s;
  short t; 


  do {
    //i semafori gestiscono conflitti fra produttori e consumatori
    xsem_wait(a->sem_data_items,__LINE__, __FILE__);
    //il buffer ha più elementi e possono entrare più consumatori che modificano cindex
    //mutex che gestisce conflitti tra consumatori 
    xpthread_mutex_lock(a->mutex,__LINE__, __FILE__);
    //prelevo dal buffer la prossima stringa
    s = (a->buffer[((*(a->pcindex))++) % PC_buffer_len]);
    if(s==NULL) {
      //se sono arrivato in fondo (e leggo il valore di terminazione)
      xpthread_mutex_unlock(a->mutex,__LINE__, __FILE__);
      xsem_post(a->sem_free_slots,__LINE__, __FILE__);
      break;
    }
    xpthread_mutex_unlock(a->mutex,__LINE__, __FILE__);
    xsem_post(a->sem_free_slots,__LINE__, __FILE__);
    readtable_lock(a->dati_tab);
    //entro nella tabella e chiamo la funzioe
    t = conta(s);
    readtable_unlock(a->dati_tab);
    //acquisisco la lcok per scrivere sul file
    xpthread_mutex_lock(a->mutexlog, __LINE__, __FILE__);
    fprintf(a->outfile,"stringa: %s, valore di conta: %d\n", s, t);
    xpthread_mutex_unlock(a->mutexlog, __LINE__, __FILE__);
    free(s);
  } while(true);

  //pthread_exit(NULL); 
  return (void *) 0;

}

//corpo dei thread lettori consumatori
void *capo_lett_body(void *arg){

  dati_capo *a = (dati_capo*)arg;
  FILE* outfile;
  int r = a->aux;
  int cl_index = 0;
  pthread_mutex_t mux_let_c;
  xpthread_mutex_init(&mux_let_c,NULL,__LINE__,__FILE__);
  pthread_mutex_t mux_log;
  xpthread_mutex_init(&mux_log,NULL,__LINE__,__FILE__);
  pthread_t lettori[r]; 
  dati_consumatori dati_let[r];
  outfile = fopen("lettori.log", "w+");
  if(outfile == NULL) xtermina("errore apertura outfile", __LINE__, __FILE__);


  //inizializzazione threads lettori
  for(int i = 0; i < r; i++){
    dati_let[i].buffer = a->buffer;
    dati_let[i].pcindex = &cl_index;
    dati_let[i].sem_free_slots = a->sem_free_slots;
    dati_let[i].sem_data_items = a->sem_data_items;
    dati_let[i].mutex = &mux_let_c;
    dati_let[i].mutexlog = &mux_log;
    dati_let[i].outfile = outfile;
    dati_let[i].dati_tab = a->dati_tab;
    xpthread_create(&lettori[i], NULL, consumer_lett_body, dati_let+i, __LINE__, __FILE__);
  }
   
    //apro la FIFO capolet in sola lettura
    int fd = open("capolet", O_RDONLY);
    if(fd<0) xtermina("Errore apertura named pipe lettore", __LINE__, __FILE__);

    short dim;
    char *tmp;
    char *max;
    ssize_t e;

    //leggo dalla FIFO
    while(true){
      e  = read(fd,&dim,sizeof(dim));
      while(e==-1){
        e  = read(fd,&dim,sizeof(dim));
      };
      if(e==0) break;
      assert(e < Max_sequence_length);
      max = calloc((dim+1),sizeof(char));
      e = read(fd, max, dim*sizeof(char));
      char *p = strtok_r(max,".,:; \n\r\t", &tmp);

      //tokenizzo per inserire le stringhe nel buffer
      while(p!=NULL){
        xsem_wait((a->sem_free_slots),__LINE__, __FILE__);
        a->buffer[((*(a->ppindex))++) % PC_buffer_len] = strdup(p);
        xsem_post((a->sem_data_items),__LINE__, __FILE__);
        p = strtok_r(NULL,".,:; \n\r\t", &tmp);
      }
      free(max);
    }

  //terminazione threads consumatori
  //NULL lo scrivo r volte per ogni lettore consumatore
  for(int i=0;i<r;i++) {
    xsem_wait(a->sem_free_slots, __LINE__, __FILE__);
    a->buffer[(*(a->ppindex))++ % PC_buffer_len] = NULL;
    xsem_post(a->sem_data_items,__LINE__, __FILE__);
  }

  // join dei thread e calcolo risulato
  for(int i=0;i<r;i++) {
    xpthread_join(lettori[i],NULL,__LINE__, __FILE__);
  }

  //chiudo estremità lettura della FIFO
  xclose(fd, __LINE__, __FILE__); 
  if(fclose(outfile)!=0) xtermina("errore fclose\n", __LINE__, __FILE__);
  xsem_destroy(a->sem_data_items,__LINE__, __FILE__);
  xsem_destroy(a->sem_free_slots,__LINE__, __FILE__);
  xpthread_mutex_destroy(&mux_let_c, __LINE__, __FILE__);
  xpthread_mutex_destroy(&mux_log, __LINE__, __FILE__);

  //non uso pthread_exit(NULL); per non far uscire errore di sistema
  return (void *) 0;
}

//corpo del consumatore scrittore
void *consumer_scritt_body(void *arg){

  dati_consumatori *a = (dati_consumatori*)arg;
  char *s;

  do {
    //i semafori gestiscono conflitti fra produttori e consumatori
    xsem_wait(a->sem_data_items,__LINE__, __FILE__);
    //il buffer ha più elementi e possono entrare più consumatori
    //e modificano cindex
    //mutex che gestisce conflitti tra consumatori 
    xpthread_mutex_lock(a->mutex,__LINE__, __FILE__);
    s = (a->buffer[((*(a->pcindex))++) % PC_buffer_len]);
    setbuf(stdout, NULL);
    fflush(stdout);
    if(s==NULL) {
      xpthread_mutex_unlock(a->mutex,__LINE__, __FILE__);
      xsem_post(a->sem_free_slots,__LINE__, __FILE__);
      break;
    }
    xpthread_mutex_unlock(a->mutex,__LINE__, __FILE__);
    xsem_post(a->sem_free_slots,__LINE__, __FILE__);
    writetable_lock(a->dati_tab);
    setbuf(stdout, NULL);
    //fflush(stdout);
    aggiungi(s, a->dati_tab);
    writetable_unlock(a->dati_tab);
    free(s);
  } while(true);

  //pthread_exit(NULL);
  return (void *) 0;
 
}

//corpo del capo scrittore
void *capo_scritt_body(void *arg){

  dati_capo *a = (dati_capo*)arg;
  int w = a->aux;
  int cr_index = 0;
  pthread_mutex_t mux_scritt_c;
  xpthread_mutex_init(&mux_scritt_c,NULL,__LINE__,__FILE__);
  pthread_t scrittori[w]; 
  dati_consumatori dati_sc[w];

  //inizializzazione threads scrittori
  for(int i = 0; i < w; i++){
    dati_sc[i].buffer = a->buffer;
    dati_sc[i].pcindex = &cr_index;
    dati_sc[i].sem_free_slots = a->sem_free_slots;
    dati_sc[i].sem_data_items = a->sem_data_items;
    dati_sc[i].mutex = &mux_scritt_c;
    //dati_sc[i].mutexlog = &mux_log;
    dati_sc[i].dati_tab = a->dati_tab;
    xpthread_create(&scrittori[i], NULL, consumer_scritt_body, dati_sc+i, __LINE__, __FILE__);
  }

  //apro la FIFO caposc in sola lettura
	int fd = open("caposc", O_RDONLY);
  if(fd<0) xtermina("Errore apertura named pipe scrittore", __LINE__, __FILE__);

  short dim;
	char *tmp;
  char *max;
	ssize_t e;

  //leggo dalla FIFO
  while(true){
    e  = read(fd,&dim,sizeof(dim));
    while(e==-1){
      e  = read(fd,&dim,sizeof(dim));
    };
    fflush(stdout);

    if(e==0) break;
    assert(e < Max_sequence_length);
    max = calloc((dim+1),sizeof(char));
    e = read(fd, max, dim*sizeof(char));

    char *p = strtok_r(max,".,:; \n\r\t", &tmp);

    //tokenizzo per inserire le stringhe nel buffer
    while(p!=NULL){
      
      fflush(stdout);
      xsem_wait((a->sem_free_slots),__LINE__, __FILE__);
      a->buffer[((*(a->ppindex))++) % PC_buffer_len] = strdup(p);
                fflush(stdout);
      setbuf(stdout, NULL);
      xsem_post((a->sem_data_items),__LINE__, __FILE__);
      p = strtok_r(NULL,".,:; \n\r\t", &tmp);
    }
    free(max);
  }
   //terminazione threads consumatori
  //NULL lo scrivo r volte per ogni consumatore (ogni consumatore elimina quando legge)
  for(int i=0;i<w;i++) {
    //aspetto se non ci sono slot liberi
    xsem_wait(a->sem_free_slots, __LINE__, __FILE__);
    a->buffer[(*(a->ppindex))++ % PC_buffer_len] = NULL;
    //comunico che c'è un nuovo valore 
    xsem_post(a->sem_data_items,__LINE__, __FILE__);
  }

  // join dei thread e calcolo risulato
  for(int i=0;i<w;i++) {
    xpthread_join(scrittori[i],NULL,__LINE__, __FILE__);
  }

  //chiudo estremità lettura della FIFO
  xclose(fd,__LINE__, __FILE__); //chiude estremità lettura
  xsem_destroy(a->sem_data_items,__LINE__, __FILE__);
  xsem_destroy(a->sem_free_slots,__LINE__, __FILE__);
  xpthread_mutex_destroy(&mux_scritt_c, __LINE__, __FILE__);
  //pthread_exit(NULL);
  return (void *) 0;

}

//corpo del thread gestore
void *gestione(void* arg){

  // recupera argomento passato al thread
  dati_gestore *d = (dati_gestore*) arg;
  
  //starto il capo dei lettori
  xpthread_create(d->capo_lettore, NULL, capo_lett_body, d->dati_capo_lettore, __LINE__, __FILE__);
  //starto il capo degli scrittori
  xpthread_create(d->capo_scrittore, NULL, capo_scritt_body, d->dati_capo_scrittore, __LINE__, __FILE__);

  //la maschera del gestore non blocca nessun segnale
  sigset_t mask;
  sigfillset(&mask);
  int s;
  siginfo_t sinfo; //struttura con info: pid chi ha mandato segnale, numero segnale...

  while(true) {
    int e = sigwaitinfo(&mask,&sinfo);
    if(e==-1) perror("Errore sigwaitinfo");
    s = sinfo.si_signo;
    //printf("Thread gestore %d svegliato dal segnale %d da %d\n",gettid(),s,sinfo.si_pid);
    //printf("Mi è stato inviato il valore %d\n",sinfo.si_value.sival_int); //si_value è una union
    if(s ==  SIGINT){ //SIGINT
      fprintf(stderr, "stringhe contenute nella tabella %d\n", *((d->dati_tab)->dati_aggiunti));

    }else if(s == SIGTERM){ //SIGTERM

      //aspetto la terminazione dei thread capi
      xpthread_join(*(d->capo_lettore), NULL, __LINE__, __FILE__);
      xpthread_join(*(d->capo_scrittore), NULL, __LINE__, __FILE__);
      printf("stringhe contenute nella tabella: %d\n", *((d->dati_tab)->dati_aggiunti));
      pthread_exit(NULL);
    }     
  }
  return NULL;
}


int main(int argc, char *argv[]) {

  //leggi da linea di comando

  if(argc != 3){
    printf("Uso: %s intero1 intero2\n", argv[0]);
    exit(1);
  }

  printf("Programma C Archivio in esecuzione\n");
  
  int w = atoi(argv[1]); //numero thread scrittori
  int r = atoi(argv[2]); //numero thread lettori

  assert(w > 0);
  assert(r > 0);

  //creo e inizializzo la tabella hash
  int ht = hcreate(Num_elem);
  if(ht == 0) xtermina("Errore creazione HT", __LINE__, __FILE__);
  tabella_hash dati_tab;
  pthread_cond_t condScrittoriTab, condLettoriTab;
  pthread_mutex_t mutexTab;
  int lett_tab, dati_agg, scritt_attesa;
  bool attesa;
  dati_tab.lettori_tabella=&lett_tab;
  dati_tab.dati_aggiunti=&dati_agg;
  dati_tab.scrittori_tabella_attesa=&scritt_attesa;
  dati_tab.scrittori_tabella = &attesa;
  dati_tab.condLtabella = &condLettoriTab;
  dati_tab.condStabella = &condScrittoriTab;
  dati_tab.mutabella = &mutexTab;
  table_init(&dati_tab);

  //inizializzazione dati lettori
  char *buffer_lettori[PC_buffer_len];
  sem_t lett_sem_free_slots, lett_sem_data_items;
  xsem_init(&lett_sem_free_slots, 0, PC_buffer_len,__LINE__, __FILE__);
  xsem_init(&lett_sem_data_items, 0, 0, __LINE__, __FILE__);

  //inizializzo il capo lettore
  pthread_t capo_lettore;
  int pl_index = 0;
  dati_capo dati_capo_lettore;
  dati_capo_lettore.buffer = buffer_lettori;
  dati_capo_lettore.ppindex = &pl_index;
  dati_capo_lettore.sem_free_slots = &lett_sem_free_slots;
  dati_capo_lettore.sem_data_items = &lett_sem_data_items;
  dati_capo_lettore.aux = r;
  dati_capo_lettore.dati_tab = &dati_tab;

  //inizializzazione dati scrittori
  char *buffer_scrittori[PC_buffer_len];
  sem_t scritt_sem_free_slots, scritt_sem_data_items;
  xsem_init(&scritt_sem_free_slots, 0, PC_buffer_len, __LINE__, __FILE__);
  xsem_init(&scritt_sem_data_items, 0, 0, __LINE__, __FILE__);
  
 //inizializzo il capo scrittore
  pthread_t capo_scrittore;
  int ps_index = 0;
  dati_capo dati_capo_scrittore;
  dati_capo_scrittore.buffer = buffer_scrittori;
  dati_capo_scrittore.ppindex = &ps_index;
  dati_capo_scrittore.sem_free_slots = &scritt_sem_free_slots;
  dati_capo_scrittore.sem_data_items = &scritt_sem_data_items;
  dati_capo_scrittore.aux = w;
  dati_capo_scrittore.dati_tab = &dati_tab;

  //creo e inizializzo thread gestore
  pthread_t gestore;
  dati_gestore dati;

  dati.capo_lettore = &capo_lettore;
  dati.dati_capo_lettore = &dati_capo_lettore;
  dati.capo_scrittore = &capo_scrittore;
  dati.dati_capo_scrittore = &dati_capo_scrittore;
  dati.dati_tab = &dati_tab;
  //blocco tutti i segnali
  sigset_t mask;
  sigfillset(&mask);  // insieme di tutti i segnali
  pthread_sigmask(SIG_BLOCK,&mask,NULL); // blocco tutto 

  //starto thread gestore
  xpthread_create(&gestore, NULL, gestione, &dati, __LINE__, __FILE__);

  //aspetto che il gestore termini
  xpthread_join(gestore, NULL, __LINE__, __FILE__);


  //distruggo mutex e cv della tabella
  hdestroy();
  table_destroy(&dati_tab);
  return 0;
}