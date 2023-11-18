#!/usr/bin/env python3
import os, sys, struct, socket, threading, subprocess, datetime, time, signal, logging, argparse, concurrent.futures, pytz

Description ="""Server che gestisce più client nell'accesso ad un archivio"""

# host e porta di default
HOST = "127.0.0.1" 
PORT = 59364
MAX = 2048

#
timezone = pytz.timezone('Europe/Rome')
now = datetime.datetime.now(tz = timezone)
current_time = datetime.datetime.now()
#print(current_time.strftime("%H:%M:%S"))


#configurazione del file di log del server
logging.basicConfig(filename='server.log', 
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

    if(v == True):
      p = subprocess.Popen(["valgrind","--leak-check=full", "--track-origins=yes",
      "--show-leak-kinds=all", 
      "--log-file=valgrind-%p.log", 
      "./Archivio", str(r), str(w)], stderr=f, start_new_session=True)
    else:   
      p = subprocess.Popen(["./Archivio", str(r), str(w)], stderr=f, start_new_session=True)

    fd_l = open(Pipe_let, "wb")
    fd_s = open(Pipe_sc, "wb")

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
        with concurrent.futures.ThreadPoolExecutor(max_workers=t) as executor:
          while True:
            print("Server: In attesa di un client...")
            #accetto la connessione
            conn, addr = s.accept()
            #e passo al thread la funzione di gestione e i parametri
            executor.submit(gestisci_connessione, conn, addr, fd_l, fd_s, p)
      #se intercetto una SIGINT la gestisco
      except KeyboardInterrupt:
        print('Server: Terminazione Server')
        #chiudo il socket, le FIFO e mando SIGTERM al processo Archivio
        s.shutdown(socket.SHUT_RDWR)
        os.unlink(Pipe_let)
        os.unlink(Pipe_sc)
        os.kill(p.pid, signal.SIGTERM)


# gestisci una singola connessione con un client
def gestisci_connessione(conn, addr, fd_l, fd_s, p): 
  with conn:
    print(f"Server: {threading.current_thread().name} contattato da {addr}")
    b=1
    # attendo carattere: tipo di conessione
    data = recv_all(conn,1) 
    tipo = struct.unpack("<c", data)[0].decode()
    if tipo == 'A':
      #print("Tipo A")
      #invio il carattere inutile
      #conn.sendall(b'x')
      #ricevo la lunghezza dell'input
      data = recv_all(conn,2)
      l = struct.unpack("<h",data)[0]
      b=b+(2+l)
      seq = recv_all(conn, l).decode()
      logging.debug(f"Connessione con {addr} di tipo {tipo}, {b} bytes inviati")
      bd = struct.pack("<h", l)
      fd_l.write(bd)
      fd_l.flush()
      fd_l.write(seq.encode())
      fd_l.flush()
      print(f"{threading.current_thread().name} finito con {addr}")
    elif tipo == 'B':
      #print("Tipo B")
      while(True):    
        #conn.sendall(b'x')
        data = recv_all(conn,2)
        l = struct.unpack("<h",data)[0]
        if(l==0):
          break
        b=b+(3+l)
        seq = recv_all(conn, l).decode()
        bd = struct.pack("<h", l)
        fd_s.write(bd)
        fd_s.flush()
        fd_s.write(seq.encode())
        fd_s.flush()
      logging.debug(f"Connessione con {addr} di tipo {tipo}, {b} bytes inviati") 
      print(f"{threading.current_thread().name} finito con {addr}")
    else:
      print("tipo di connessione non supportato")
    

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
      