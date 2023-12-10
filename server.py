#!/usr/bin/env python3
import os, sys, struct, socket, threading, subprocess, datetime, time, signal, logging, argparse, concurrent.futures, pytz
from threading import Lock

Description ="""Server che gestisce più client nell'accesso ad un archivio"""

# host e porta di default
HOST = "127.0.0.1" 
PORT = 59364
MAX = 2048

timezone = pytz.timezone('Europe/Rome')
now = datetime.datetime.now(tz = timezone)
current_time = datetime.datetime.now()

#configurazione del file di log del server
logging.basicConfig(filename='server.log', 
                    filemode='w',
                    level=logging.DEBUG, datefmt='%d/%m/%y %H:%M:%S',
                    #data:                lvl logging      msg
                    format='%(asctime)s - %(levelname)s - %(message)s')

#nomi delle FIFO
Pipe_let = "capolet"
Pipe_sc = "caposc"

def main(t, r, w, v):

  print(f"Server lanciato con {t} threads, {r} lettori e {w} scrittori nell'archivio")

  #creo le FIFO
  os.mkfifo(Pipe_let)
  os.mkfifo(Pipe_sc)

  #error_file.txt è il file dove andrà output di stderr
  with open('error_file.txt', 'w') as f:

    #esecuzione in background del processo Archivio
    #start_new_session=True per non far influenzare Archivio dai segnali di server.py (processo padre)
    if(v == True):
      p = subprocess.Popen(["valgrind","--leak-check=full", "--track-origins=yes",
      "--show-leak-kinds=all", 
      "--log-file=valgrind-%p.log", "-s",
      "./archivio.out", str(r), str(w)], stderr=f, start_new_session=True)
    else:   
      p = subprocess.Popen(["./archivio.out", str(r), str(w)], stderr=f, start_new_session=True)

    #apro le FIFO
    fd_l = os.open(Pipe_let, os.O_WRONLY)
    fd_s = os.open(Pipe_sc, os.O_WRONLY)

    pid = os.getpid()

    print("il pid dell'archivio è: ", p.pid)

    print("Server: archivio in esecuzione")

    #apro il server socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      try: 
        #setto i parametri del socket 
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)   
        s.bind((HOST, PORT))
        #socket in ascolto
        s.listen()
        #lock per scrivere in modo safe nella FIFO (più thread scrivono sulla solita FIFO)
        lock = threading.Lock()
        with concurrent.futures.ThreadPoolExecutor(max_workers=t) as executor:
          while True:
            print("Server: In attesa di un client...")
            #accetto la connessione
            conn, addr = s.accept()
            #e passo al thread la funzione di gestione e i parametri
            executor.submit(gestisci_connessione, conn, addr, fd_l, fd_s, p, lock)
      #se intercetto una SIGINT la gestisco
      except KeyboardInterrupt:
        pass
      print('Server: Terminazione Server')
      #chiudo le FIFO, il socket e mando SIGTERM al processo Archivio
      os.close(fd_l)
      os.close(fd_s)
      s.shutdown(socket.SHUT_RDWR)
      p.send_signal(signal.SIGTERM)
    os.unlink(Pipe_let)
    os.unlink(Pipe_sc)
        # è brutto
        #os.kill(p.pid, signal.SIGTERM)


# gestisci una singola connessione con un client
def gestisci_connessione(conn, addr, fd_l, fd_s, p, lock): 
  with conn:
    print(f"Server: {threading.current_thread().name} contattato da {addr}")
    #ricevo carattere che descrive il tipo di conessione
    data = recv_all(conn,1) 
    tipo = struct.unpack("<c", data)[0].decode()
    if tipo == 'A':
      #ricevo la lunghezza dell'input e la riga del file
      data = recv_all(conn,2)
      l = struct.unpack("<h",data)[0]
      seq = recv_all(conn, l)
      logging.debug(f"Connessione con {addr} di tipo {tipo}, {l+3} bytes inviati")
      #preparo i dati per inviarli sulla FIFO
      bd = struct.pack("<h", l)
      #acquisisco la lock per scrivere in modo safe sulla FIFO
      lock.acquire()
      os.write(fd_l, bd)
      os.write(fd_l, seq)
      lock.release()
      print(f"{threading.current_thread().name} finito con {addr}")
    elif tipo == 'B':
      #b = numero byte inviati, i = numero della riga
      b = 1
      i = 1
      while(True):   
        #ricevo la lunghezza dell'input e la riga del file (se lunghezza != 0)
        data = recv_all(conn,2)
        l = struct.unpack("<h",data)[0]
        #se il client invia lunghezza = 0, ha finito
        if(l==0):
          break
        seq = recv_all(conn, l)
        logging.debug(f"Connessione con {addr} di tipo {tipo},{i} riga, {l+3} bytes inviati") 
        b = b+l+3
        i = i+1
        #preparo i dati per inviarli sulla FIFO
        bd = struct.pack("<h", l)
        #acquisisco la lock per scrivere in modo safe sulla FIFO
        lock.acquire()
        os.write(fd_s, bd)
        os.write(fd_s, seq)
        lock.release()
      logging.debug(f"Connessione terminata con {addr} di tipo {tipo}, {b} bytes inviati totali")
      print(f"{threading.current_thread().name} finito con {addr}")
    else:
      print("tipo di connessione non supportato.")
    
#funzione recv_all come da lezione
def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 2048))
    if len(chunk) == 0:
      raise RuntimeError("socket connection broken")
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks

#gestione dei parametri (anche opzionali) passati da linea di comando
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=Description, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('t', help='numero max threads', type = int)  
    parser.add_argument('-r', help='numero lettori', type = int, default=3)  
    parser.add_argument('-w', help='numero scrittori', type = int, default=3) 
    parser.add_argument('-v', '--valgrind', help='uso di valgrind', action='store_true', default=False) 
    args = parser.parse_args()
    assert args.t > 0, "il numero di thread del server deve essere positivo"
    assert args.r > 0, "Il numero di thread lettori dell'archivio deve essere positivo"
    assert args.w > 0, "Il numero di thread scrittori dell'archivio deve essere positivo"
    
    if args.valgrind:
        main(args.t, args.r, args.w, True)
    else:
        main(args.t, args.r, args.w, False)
      