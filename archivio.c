#include "xerrori.h"
#include "hashtable.h"
#include <sys/syscall.h>

volatile sig_atomic_t dati_aggiunti_global = 0;


// ridefinizione funzione gettid() (non è sempre disponibile) 
#include <sys/syscall.h>   /* For SYS_xxx definitions */
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


#define Num_elem 1000000            //dimensione tabella hash
#define PC_buffer_len 10            //lunghezza bugger prod/cons
#define Max_sequence_length 2048    //massima lunghezza sequenza inviata attraverso una pipe

//strutture dati relativa ai capi (lettore / scrittore)
typedef struct {
  char **buffer;                    //buffer di stringhe prod/cons
  int *ppindex;                     //primo indice disponibile per produttore
  sem_t *sem_free_slots;            //semaforo per attesa produttore
  sem_t *sem_data_items;            //semaforo per attesa consumatori
  int aux;                          //numero dei thread ausiliari
  tabella_hash *dati_tab;           //puntatore alla struttura dati relativa alla tabella hash 
} dati_capo;                        

//struttura dati relativa ai consumatori (lettori / scrittori)
typedef struct {
  char **buffer;                    //buffer di stringhe prod/cons
  int *pcindex;                     //primo indice disponibile per consumatori
  sem_t *sem_free_slots;            //semaforo per attesa produttore nel buffer
  sem_t *sem_data_items;            //semaforo per attesa consumatori nel buffer
  pthread_mutex_t *mutex;           //mutex che gestisce conflitti tra consumatori nel buffer
  pthread_mutex_t *mutexlog;        //mutex per scrivere nel file di log (lettori)
  FILE *outfile;                    //puntatore al file di log per i lettori
  tabella_hash *dati_tab;           //puntatore alla struttura dati relativa alla tabella hash 
} dati_consumatori;                 


//struttura dati da passare come argomento al thread gestore per gestire i capi e la tabella hash
typedef struct{

  pthread_t *capo_lettore;          //ID del thread capo_lettore per fare la join
  pthread_t *capo_scrittore;        //ID del thread capo_scrittore per fare la join
  tabella_hash *dati_tab;           //puntatore alla struttura dati relativa alla tabella hash

} dati_gestore;

//corpo del lettore consumatore
void *consumer_lett_body(void *arg){

  dati_consumatori *a = (dati_consumatori*)arg;
  char *s;
  short t; 

  do {
    //aspetto finchè non c'è un dato
    xsem_wait(a->sem_data_items,__LINE__, __FILE__);
    //il buffer ha più elementi e possono entrare più consumatori che modificano pcindex
    //mutex che gestisce conflitti tra consumatori 
    xpthread_mutex_lock(a->mutex,__LINE__, __FILE__);
    //prelevo dal buffer la prossima stringa
    s = (a->buffer[((*(a->pcindex))++) % PC_buffer_len]);
    xpthread_mutex_unlock(a->mutex,__LINE__, __FILE__);
    //segnalo che c'è uno slot libero
    xsem_post(a->sem_free_slots,__LINE__, __FILE__);
    //se leggo NULL (valore dummy), esco
    if(s == NULL) break;
    //entro nella tabella e chiamo la funzione
    readtable_access(a->dati_tab);
    t = conta(s);
    readtable_exit(a->dati_tab);
    //acquisisco la lock per scrivere sul file
    xpthread_mutex_lock(a->mutexlog, __LINE__, __FILE__);
    fprintf(a->outfile,"stringa: %s, valore di conta: %d\n", s, t);
    xpthread_mutex_unlock(a->mutexlog, __LINE__, __FILE__);
    free(s);
  } while(true);

  //pthread_exit(NULL); tolto per evitare errori di sistema su valgrind
  return (void *) 0;

}

//corpo dei thread lettori consumatori
void *capo_lett_body(void *arg){

  dati_capo *a = (dati_capo*)arg;
  //file dove scrivere il log
  FILE* outfile;
  //numero consumatori
  int r = a->aux;
  int cl_index = 0;
  //mutex per gestire conflitti tra i consumatori
  pthread_mutex_t mux_let_c;
  xpthread_mutex_init(&mux_let_c,NULL,__LINE__,__FILE__);
  //mutex per scrivere sul file di log
  pthread_mutex_t mux_log;
  xpthread_mutex_init(&mux_log,NULL,__LINE__,__FILE__);
  //array per memorizzare gli ID dei lettori
  pthread_t lettori[r]; 
  //array per memorizzare le strutture dati dei lettori
  dati_consumatori dati_let[r];
  outfile = fopen("lettori.log", "w+");
  if(outfile == NULL) xtermina("errore apertura outfile", __LINE__, __FILE__);


  //inizializzazione struttura dati dei threads lettori
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

    //leggo dalla FIFO un intero e la stringa 
    while(true){
      e = read(fd,&dim,sizeof(dim));
      if(e != sizeof(dim)) break;
      assert(e < Max_sequence_length);
      max = calloc((dim+1),sizeof(char));
      max[dim] = 0;
      e = read(fd, max, dim*sizeof(char));
      if(e != dim*sizeof(char)) xtermina("Errore in lettura\n", __LINE__, __FILE__);
      char *p = strtok_r(max,".,:; \n\r\t", &tmp);

      //tokenizzo per inserire le stringhe nel buffer
      while(p!=NULL){
        //aspetto finchè non c'è uno slot libero
        xsem_wait((a->sem_free_slots),__LINE__, __FILE__);
        a->buffer[((*(a->ppindex))++) % PC_buffer_len] = strdup(p);
        //segnalo la presenza di un elemento nel buffer
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

  //non uso pthread_exit(NULL); per non far uscire errore di sistema su valgrind
  return (void *) 0;
}

//corpo del consumatore scrittore
void *consumer_scritt_body(void *arg){
   

  dati_consumatori *a = (dati_consumatori*)arg;
  char *s;

  do {
    //aspetto finchè non c'è un dato
    xsem_wait(a->sem_data_items,__LINE__, __FILE__);
    //il buffer ha più elementi e possono entrare più consumatori, modificando pcindex
    //mutex che gestisce conflitti tra consumatori 
    xpthread_mutex_lock(a->mutex,__LINE__, __FILE__);
    //prelevo dal buffer la prossima stringa
    s = (a->buffer[((*(a->pcindex))++) % PC_buffer_len]);
    xpthread_mutex_unlock(a->mutex,__LINE__, __FILE__);
    //segnalo che c'è uno slot libero
    xsem_post(a->sem_free_slots,__LINE__, __FILE__);
    if(s==NULL) break;
    //aggiungo il dato alla tabella
    writetable_lock(a->dati_tab);
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
  int cs_index = 0;
  //mutex per gestire i conflitti tra consumatori
  pthread_mutex_t mux_scritt_c;
  xpthread_mutex_init(&mux_scritt_c,NULL,__LINE__,__FILE__);
  //array per memorizzare i PID degli scrittori
  pthread_t scrittori[w]; 
  //array per memorizzare le strutture degli scrittori
  dati_consumatori dati_sc[w];

  //inizializzazione struttura dati dei threads scrittori
  for(int i = 0; i < w; i++){
    dati_sc[i].buffer = a->buffer;
    dati_sc[i].pcindex = &cs_index;
    dati_sc[i].sem_free_slots = a->sem_free_slots;
    dati_sc[i].sem_data_items = a->sem_data_items;
    dati_sc[i].mutex = &mux_scritt_c;
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
    e = read(fd,&dim,sizeof(dim));
    if(e != sizeof(dim)) break;
    assert(e < Max_sequence_length);
    max = calloc((dim+1),sizeof(char));
    max[dim] = 0;
    e = read(fd, max, dim*sizeof(char));
    if(e != dim*sizeof(char)) xtermina("Errore in lettura\n", __LINE__, __FILE__);
    char *p = strtok_r(max,".,:; \n\r\t", &tmp);

    //tokenizzo per inserire le stringhe nel buffer
    while(p!=NULL){ 
      //aspetto finchè non c'è uno slot libero
      xsem_wait((a->sem_free_slots),__LINE__, __FILE__);
      a->buffer[((*(a->ppindex))++) % PC_buffer_len] = strdup(p);
      //segnalo la presenza di un elemento nel buffer
      xsem_post((a->sem_data_items),__LINE__, __FILE__);
      p = strtok_r(NULL,".,:; \n\r\t", &tmp);
    }
    free(max);
  }

  //terminazione threads consumatori
  //NULL lo scrivo w volte per ogni consumatore (ogni consumatore elimina quando legge)
  for(int i=0;i<w;i++) {
    xsem_wait(a->sem_free_slots, __LINE__, __FILE__);
    a->buffer[(*(a->ppindex))++ % PC_buffer_len] = NULL;
    xsem_post(a->sem_data_items,__LINE__, __FILE__);
  }

  // join dei thread e calcolo risulato
  for(int i=0;i<w;i++) {
    xpthread_join(scrittori[i],NULL,__LINE__, __FILE__);
  }

  //chiudo estremità lettura della FIFO
  xclose(fd,__LINE__, __FILE__);
  xsem_destroy(a->sem_data_items,__LINE__, __FILE__);
  xsem_destroy(a->sem_free_slots,__LINE__, __FILE__);
  xpthread_mutex_destroy(&mux_scritt_c, __LINE__, __FILE__);
  //va bene anche return 0
  return (void *) 0;
}

void gestore(int sig) {

  //printf("tid handler: %d\n", gettid());
  int val = dati_aggiunti_global;
  int len = 14;
  char buf[16] = {0};
  buf[15] = '\0'; 
  int e;
  do {
    buf[len--] = '0' + (val % 10); 
    val /= 10;
  } while(val != 0);
  e = write(STDERR_FILENO, buf + len + 1, 14 - len);
  if(e == -1) perror("Errore write");
}

void gestore_term(int sig) {

  //printf("tid handler: %d\n", gettid());
  int val = dati_aggiunti_global;
  int len = 14;
  //inizializzo a 0
  char buf[16] = {0};
  //e metto carattere terminatore
  buf[15] = '\0'; 
  int e;
  do {
      //converto ogni numero in char e recupero le cifres
      buf[len--] = '0' + (val % 10); 
      val /= 10;
  } while(val != 0);
  //ignorando gli 0 iniziali, e rispostandomi con +1 al punto giusto nel buf, scrivo 14-len byte
  e = write(STDOUT_FILENO, buf + len + 1, 14 - len);
  if(e == -1) perror("Errore write");
}


void* gestore_body(void* arg) {

  dati_gestore *d = (dati_gestore*) arg;

  //printf("tid gestore: %d\n", gettid());

  struct sigaction sa;
  int sig;

  sa.sa_handler = &gestore;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
      perror("sigaction");
      return NULL;
  }
  
  sa.sa_handler = &gestore_term;
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
      perror("sigaction");
      return NULL;
  }

  sigset_t mask;
  sigfillset(&mask);
  pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

  while(1) {
    sigwait(&mask, &sig); // Aspetta i segnali
    //acquisisco la lock solo per leggere e per leggere dati consistenti
    xpthread_mutex_lock((d->dati_tab)->mutabella, __LINE__, __FILE__);
    dati_aggiunti_global = *((d->dati_tab)->dati_aggiunti);
    xpthread_mutex_unlock((d->dati_tab)->mutabella, __LINE__, __FILE__);
    
    if(sig == SIGTERM){
      raise(sig);
      break;
    }else if(sig == SIGINT){ 
      raise(sig);
    }else{
      signal(sig, SIG_DFL);
      raise(sig);
    }
  }
  
  // Dopo SIGTERM e fuori dal ciclo di gestione attende la terminazione dei capi
  xpthread_join(*(d->capo_lettore), NULL, __LINE__, __FILE__);
  xpthread_join(*(d->capo_scrittore), NULL, __LINE__, __FILE__);
  
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

  //creo e inizializzo la tabella hash e la struttura dati associata
  int ht = hcreate(Num_elem);
  if(ht == 0) xtermina("Errore creazione HT", __LINE__, __FILE__);
  tabella_hash dati_tab;
  pthread_cond_t condScrittoriTab;
  pthread_mutex_t mutexTab;
  int lett_tab, dati_agg;
  bool writing;
  dati_tab.lettori_tabella=&lett_tab;
  dati_tab.dati_aggiunti=&dati_agg;
  dati_tab.writing = &writing;
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

  sigset_t mask;  
  sigfillset(&mask);
  pthread_sigmask(SIG_SETMASK, &mask, NULL);

  pthread_t gestore;
  //inizializzo la struttura dati del gestore
  dati_gestore dati;
  dati.capo_lettore = &capo_lettore;
  dati.capo_scrittore = &capo_scrittore;
  dati.dati_tab = &dati_tab;

  xpthread_create(&gestore, NULL, gestore_body, &dati,__LINE__,__FILE__);

  //starto il capo dei lettori
  xpthread_create(&capo_lettore, NULL, capo_lett_body, &dati_capo_lettore, __LINE__, __FILE__);
  //starto il capo degli scrittori
  xpthread_create(&capo_scrittore, NULL, capo_scritt_body, &dati_capo_scrittore, __LINE__, __FILE__);

  //aspetto che termini il gestore
  xpthread_join(gestore, NULL, __LINE__, __FILE__);

  //distruggo mutex e cv della tabella
  hdestroy();
  table_destroy(&dati_tab);
  return 0;
}