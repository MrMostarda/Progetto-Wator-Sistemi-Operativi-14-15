/** \file processo_wator.c
    \author Matteo Anselmi
    Si dichiara che il contenuto di questo file e' in ogni sua parte opera
    originale dell' autore.  */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <mcheck.h>
#include "wator.h"
#include "processo_wator.h"

struct sockaddr_un sa;
int pid;
volatile sig_atomic_t fine = 0;
coda queue;
pthread_mutex_t mtx_coda = PTHREAD_MUTEX_INITIALIZER; /*per accesso alla coda*/
pthread_mutex_t mtx_lastwork = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t mtx_matx = PTHREAD_MUTEX_INITIALIZER; /*per accesso a matx*/
pthread_mutex_t mtx_atlantis = PTHREAD_MUTEX_INITIALIZER; /*per accesso ad atlantis*/
pthread_cond_t cond_empty = PTHREAD_COND_INITIALIZER; /*per la condizione di coda vuota*/
pthread_cond_t *cond_matx; /**/
int flag_matrice_divisa = FALSE; /*segnala quando tutta la matrice è stata divisa dal dispatcher*/
pthread_mutex_t mtx_matriceDivisa = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond_matriceDivisa = PTHREAD_COND_INITIALIZER; 
int chronon_succ = FALSE; /*segnala quando è possibile passare al chronon successivo*/
pthread_mutex_t mtx_chrononSucc = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond_chrononSucc = PTHREAD_COND_INITIALIZER; 
int collector_go = FALSE; /*segnala quando il thread collector può iniziare la sua elaborazione*/
pthread_mutex_t mtx_collectorGo = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond_collectorGo = PTHREAD_COND_INITIALIZER; 
int end_work = FALSE;/*segnala ai worker di terminare l'elaborazione*/
int *nsezioni; /*array usate per verificare quando tutte le sottomatrici sono state elaborate dai workers*/
pthread_mutex_t mtx_nsezioni = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond_nsezioni = PTHREAD_COND_INITIALIZER; 
int length_nsezioni; /*dimensione array nsezioni*/
int sez_per_fila; /*numero di sottomatrici in una fila della matrice*/
int *matx; /*array di contatori utilizzato per bloccare l'elaborazione di alcune sottomatrici*/
int partenza = FALSE;
pthread_mutex_t mtx_partenza = PTHREAD_MUTEX_INITIALIZER; /*per accesso a partenza*/
pthread_cond_t cond_partenza = PTHREAD_COND_INITIALIZER; 
int last_work = 0;/*per vedere se tutti i workers hanno terminato l'ultima elaborazione*/
int abortisci = FALSE; /*per terminare in caso di errore*/
wator_t *atlantis; /*la struttura wator della simulazione*/
int **azione;/*usato per evitare che una casella venga aggiornata più volte e per evitare che un animale appena nato faccia un 		movimento*/

/*calcola la somma degli elementi di un array di interi
	/param array: l' array di interi
	/param len: la lunghezza dell' array

	/retval: la somma
*/
int reduce(int array[], int len){
	int i, somma = 0;

	for(i = 0; i < len; i++)
		somma += array[i];
	return somma;
}

/*azzera un array di interi
	/param array: l' array di interi
	/param len: la lunghezza dell' array
*/
void reset(int array[], int len){
	int i;

	for(i = 0; i < len; i++)
		array[i] = 0;
}

/*azzera matrice di interi*/
void azzera(int **array, int row, int col){
	int i,j;

	for(i = 0; i < row; i++)
		for(j = 0; j < col; j++)
			array[i][j] = 0;

}

/*inizializza la coda
	/param cod: la coda
*/
void init_queue(coda *cod){
	cod->top = 0;
	cod->bottom = 0;
}

/*verifica se la coda è vuota
	/param cod: la coda da verificare

	/retval 1: è vuota
	/retval 0: non è vuota
*/
int isEmpty(coda cod){
	return (cod.top == cod.bottom);
}

/*verifica se la coda è piena
	/param cod: la coda da verificare

	/retval 1: è piena
	/retval 0: non è piena
*/
int isFull(coda cod){
	int t, b;
	t = cod.top % M;
	b = (cod.bottom + 1) % M;
	return (t == b);
}

/*inserisce una sottomatrice del pianeta nella coda
	/param cod: la coda nella quale inserire la sottomatrice
	/param pianeta: la sottomatrice
	/param id: numero che identifica la sottomatrice

	/retval 0: tutto ok
	/retval -1: errore
*/
int insert_queue(coda* cod, planet_t *pianeta, int id){
	int i, j;

	cod->sez[cod->bottom].plan = new_planet(pianeta->nrow, pianeta->ncol);
	if(cod->sez[cod->bottom].plan == NULL)
		return -1;
	cod->sez[cod->bottom].id = id; 

	for(i = 0; i < pianeta->nrow; i++)
		for(j = 0; j < pianeta->ncol; j++){
			cod->sez[cod->bottom].plan->w[i][j] = pianeta->w[i][j]; 
			cod->sez[cod->bottom].plan->btime[i][j] = pianeta->btime[i][j];
			cod->sez[cod->bottom].plan->dtime[i][j] = pianeta->dtime[i][j];
		}
	cod->bottom = (cod->bottom + 1) % M;
	return 0;
}

/*estrae un elemento dalla coda
	/param cod: la coda nella quale inserire la sottomatrice
	
	/retval sezione*: elemento della coda restituito(id = -1 in caso di errore)
*/
sezione* get_queue(coda* cod){
	int i, j;
	sezione *sector;

	sector = (sezione*)malloc(sizeof(sezione));
	sector->plan = new_planet(cod->sez[cod->top].plan->nrow, cod->sez[cod->top].plan->ncol);
	if(sector->plan == NULL){
		sector->id = -1;
		return sector;
	}
	
	sector->id = cod->sez[cod->top].id;
	
	for(i = 0; i < sector->plan->nrow; i++)
		for(j = 0; j < sector->plan->ncol; j++){
			sector->plan->w[i][j] = cod->sez[cod->top].plan->w[i][j];
			sector->plan->btime[i][j] = cod->sez[cod->top].plan->btime[i][j];
			sector->plan->dtime[i][j] = cod->sez[cod->top].plan->dtime[i][j];
		}
	free_planet(cod->sez[cod->top].plan);
	cod->top = (cod->top + 1) % M;
	return sector;
}

/*funzione che si occupa di spedire il pianeta a visualizer*/
void send_to_visualizer(char flag){
	int fd_skt, i, j, err;
	char cella;
	
	/*creo il socket*/
	if((fd_skt = socket(DOMAIN, TYPE, 0)) == -1){
		perror("errore in creazione socket nel client");
		kill(pid, SIGTERM);
		pthread_exit((void*)EXIT_FAILURE);
	}

	/*mi connetto al socket creato dal server*/
	while(connect(fd_skt,(struct sockaddr *)&sa,sizeof(sa)) == -1){
		/*se la connect fallisce perche il server non ha ancora fatto la accept attendo 1 secondo e riprovo 
		 *	connettermi*/		
		if(errno == ENOENT){
			sleep(1);
		}
		else{
			perror("errore in connect nel client");
			close(fd_skt);
			kill(getpid(), SIGTERM);
			abortisci = TRUE;		
			if((err = pthread_cond_signal(&cond_chrononSucc)) != 0){
				errno = err;
				perror("signal");
				Pthread_mutex_unlock(&mtx_chrononSucc);
				pthread_exit((int*)&errno);
			}	
			
			pthread_exit((void*)EXIT_FAILURE);
		}
	}

	while(write(fd_skt, &flag, sizeof(char)) == -1){
		if(errno == EINTR){
			continue;
		}
		perror("errore scrittura");
		close(fd_skt);
		kill(pid, SIGTERM);
		pthread_exit((void*)EXIT_FAILURE);
	}
	if(flag == 'O'){
		fprintf(stderr, "invio matrice...\n");
		/*invio al visualizer le caselle del pianeta*/
		/*le caselle sono prima convertite in char così inviamo solo 1 byte invece di 4(dimensione di int)*/
		Pthread_mutex_lock(&mtx_atlantis);
		for(i = 0; i < (atlantis->plan->nrow); i++)
			for(j = 0; j < (atlantis->plan->ncol); j++){
				cella = cell_to_char (atlantis->plan->w[i][j]);
				if(write(fd_skt, &cella, sizeof(char)) == -1){
					/*se la scrittura fallisce per un segnale inviato mentre so eseguendo la S.C 
					devo riavviare la write*/				
					if(errno == EINTR){
						j--;
					}
					else{
						perror("errore scrittura");
						close(fd_skt);
						kill(pid, SIGTERM);
						Pthread_mutex_unlock(&mtx_atlantis);
						pthread_exit((void*)EXIT_FAILURE);
					}
				}
			}
		Pthread_mutex_unlock(&mtx_atlantis);
		fprintf(stderr, "matrice inviata\n");
	}
	close(fd_skt);
}

/*trasforma le coordinate di una sottomatrice in coordinate della matrice completa/
	/param i,j: le coordinate della sottomatrice
	/param s: la sottomatrice
	/param k,l: le coordiante finali nella matrice completa
*/
void subMatrixToMatrix(int i, int j, int id, int *k, int *l){
	int riga, colonna;


	/*mi posiziono sulla casella della matrice completa dalla quale inizia la sottomatrice*/		
	riga = (id / sez_per_fila) * (K - 2);
	if(riga == 0)	
		riga = (atlantis->plan)->nrow;
	colonna = (id % sez_per_fila) * (N - 2);
	if(colonna == 0)	
		colonna = (atlantis->plan)->ncol;

	*k = ((riga + i) - 1) % (atlantis->plan)->nrow;
	*l = ((colonna + j) - 1) % (atlantis->plan)->ncol;
}


/*elaborazione di una sottomatrice
	/param s: la sezione da elaborare

	/retval 1: tutto ok
	/retval 0: errore
*/
int elabora(int id, wator_t *pw){
	int i, j, nrow, ncol, err, x, k, l, imat, jmat, esito;
	

	nrow = pw->plan->nrow;
	ncol = pw->plan->ncol;

	

	/*inizio l'applicazione delle regole*/
	for(i = 1; i < (nrow-1); i++){
		for(j = 1; j < (ncol-1); j++){
			/*mi trovo sul bordo oppure in un punto che interagisce sul bordo*/
		      if((i == 1) || (i == nrow-2) || (i == 2) || (i == nrow-3) || (j == 1) || (j == ncol-2) || (j == 2) || (j == ncol-3)){
				Pthread_mutex_lock(&mtx_matx);
				while(matx[id] > 0){
					if((err = pthread_cond_wait(&cond_matx[id], &mtx_matx)) != 0){
						errno = err;
						perror("wait condition");
						return 0;
					}
				}
				x = id;
				/*blocco la sottomatrice nella posizione soprastante*/
				if( (i == 1) || (i == 2) ){
					if(id < sez_per_fila)
						x += length_nsezioni;
					if( ((x-sez_per_fila) % length_nsezioni) != id)
						matx[(x-sez_per_fila) % length_nsezioni]++;
						
				}
				x = id;
				/*blocco la matrice sottostante*/
				if( (i == nrow-2) || (i == nrow-3) ){
					if( ((x+sez_per_fila) % length_nsezioni) != id)
						matx[(x+sez_per_fila) % length_nsezioni]++;
				}
				x = id;
				/*blocco la matrice a sinistra*/
				if( (j == 1) || (j == 2) ){
					if((id % sez_per_fila) == 0)
						x += sez_per_fila;
					if( ((x-1) % length_nsezioni) != id)
						matx[(x-1) % length_nsezioni]++;
						
				}
				x = id;
				/*blocco la matrice a destra*/
				if( (j == ncol-2) || (j == ncol-3) ){
					if((id % sez_per_fila) == (sez_per_fila-1))
						x -= sez_per_fila;
					if( ((x+1) % length_nsezioni) != id)
						matx[(x+1) % length_nsezioni]++;
				}
				/*se sono su un angolo devo bloccare anche la sottomatrice posta diagonalmente*/
				x = id;
				/*diagonale alto a sx*/
				if( (i == 1) && (j == 1) ){
					if(id < sez_per_fila)
						x += length_nsezioni;
					x = (x-sez_per_fila) % length_nsezioni;

					if((x % sez_per_fila) == 0)
						x += sez_per_fila;
					x = (x-1) % length_nsezioni;

					if( x != id )
						matx[x % length_nsezioni]++;
				}
				x = id;
				/*diagonale alto a dx*/
				if( (i == 1) && (j == ncol-2) ){
					if(id < sez_per_fila)
						x += length_nsezioni;
					x = (x-sez_per_fila) % length_nsezioni;

					if((x % sez_per_fila) == (sez_per_fila-1))
						x -= sez_per_fila;
					x = (x+1) % length_nsezioni;
					
					if( x != id )
						matx[x % length_nsezioni]++;
				}
				x = id;
				/*diagonale basso a sx*/
				if( (i == nrow-2) && (j == 1) ){
					x = (x+sez_per_fila) % length_nsezioni;

					if((x % sez_per_fila) == 0)
						x += sez_per_fila;
					x = (x-1) % length_nsezioni;
				
					if( x != id )
						matx[x % length_nsezioni]++;
				} 
				x = id;
				/*diagonale basso a dx*/
				if( (i == nrow-2) && (j == ncol-2) ){
					x = (x+sez_per_fila) % length_nsezioni;

					if((x % sez_per_fila) == (sez_per_fila-1))
						x -= sez_per_fila;
					x = (x+1) % length_nsezioni;

					if( x != id )
						matx[x % length_nsezioni]++;
				}
				Pthread_mutex_unlock(&mtx_matx);
			}
		/*************************************************************************************************************/
		
		/*copio le caselle adiacenti che potrebbero essere state modificate da altre sottomatrici*/
			 if(i == 1){
				Pthread_mutex_lock(&mtx_atlantis);
				/*sinistra*/
				subMatrixToMatrix(i, j-1, id, &imat, &jmat);
				(pw->plan)->w[i][j-1] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i][j-1] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i][j-1] = (atlantis->plan)->dtime[imat][jmat];
				/*casella stessa*/
				subMatrixToMatrix(i, j, id, &imat, &jmat);
				(pw->plan)->w[i][j] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i][j] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i][j] = (atlantis->plan)->dtime[imat][jmat];
				/*destra*/
				subMatrixToMatrix(i, j+1, id, &imat, &jmat);
				(pw->plan)->w[i][j+1] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i][j+1] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i][j+1] = (atlantis->plan)->dtime[imat][jmat];
				/*sopra*/
				subMatrixToMatrix(i-1, j, id, &imat, &jmat);
				(pw->plan)->w[i-1][j] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i-1][j] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i-1][j] = (atlantis->plan)->dtime[imat][jmat];
				Pthread_mutex_unlock(&mtx_atlantis);
			
			}

			if(j == 1){
				Pthread_mutex_lock(&mtx_atlantis);
				/*sotto*/
				subMatrixToMatrix(i+1, j, id, &imat, &jmat);
				(pw->plan)->w[i+1][j] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i+1][j] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i+1][j] = (atlantis->plan)->dtime[imat][jmat];
				Pthread_mutex_unlock(&mtx_atlantis);
				if(i != 1){
					Pthread_mutex_lock(&mtx_atlantis);
					/*sinistra*/
					subMatrixToMatrix(i, j-1, id, &imat, &jmat);
					(pw->plan)->w[i][j-1] = (atlantis->plan)->w[imat][jmat];
					(pw->plan)->btime[i][j-1] = (atlantis->plan)->btime[imat][jmat];
					(pw->plan)->dtime[i][j-1] = (atlantis->plan)->dtime[imat][jmat];
					/*casella stessa*/
					subMatrixToMatrix(i, j, id, &imat, &jmat);
					(pw->plan)->w[i][j] = (atlantis->plan)->w[imat][jmat];
					(pw->plan)->btime[i][j] = (atlantis->plan)->btime[imat][jmat];
					(pw->plan)->dtime[i][j] = (atlantis->plan)->dtime[imat][jmat];
					/*sopra*/
					subMatrixToMatrix(i-1, j, id, &imat, &jmat);
					(pw->plan)->w[i-1][j] = (atlantis->plan)->w[imat][jmat];
					(pw->plan)->btime[i-1][j] = (atlantis->plan)->btime[imat][jmat];
					(pw->plan)->dtime[i-1][j] = (atlantis->plan)->dtime[imat][jmat];
					Pthread_mutex_unlock(&mtx_atlantis);
				}
			}

			if(i == nrow-2 && i > 1){
				Pthread_mutex_lock(&mtx_atlantis);
				/*sinistra*/
				subMatrixToMatrix(i, j-1, id, &imat, &jmat);
				(pw->plan)->w[i][j-1] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i][j-1] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i][j-1] = (atlantis->plan)->dtime[imat][jmat];
				/*casella stessa*/
				subMatrixToMatrix(i, j, id, &imat, &jmat);
				(pw->plan)->w[i][j] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i][j] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i][j] = (atlantis->plan)->dtime[imat][jmat];
				/*destra*/
				subMatrixToMatrix(i, j+1, id, &imat, &jmat);
				(pw->plan)->w[i][j+1] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i][j+1] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i][j+1] = (atlantis->plan)->dtime[imat][jmat];
				/*sotto*/
				subMatrixToMatrix(i+1, j, id, &imat, &jmat);
				(pw->plan)->w[i+1][j] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i+1][j] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i+1][j] = (atlantis->plan)->dtime[imat][jmat];
				Pthread_mutex_unlock(&mtx_atlantis);
			}

			if(j == ncol-2 && j > 1){
				Pthread_mutex_lock(&mtx_atlantis);
				/*sopra*/
				subMatrixToMatrix(i-1, j, id, &imat, &jmat);
				(pw->plan)->w[i-1][j] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i-1][j] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i-1][j] = (atlantis->plan)->dtime[imat][jmat];
				Pthread_mutex_unlock(&mtx_atlantis);
				if(i != nrow-1){
					Pthread_mutex_lock(&mtx_atlantis);
					/*destra*/
					subMatrixToMatrix(i, j+1, id, &imat, &jmat);
					(pw->plan)->w[i][j+1] = (atlantis->plan)->w[imat][jmat];
					(pw->plan)->btime[i][j+1] = (atlantis->plan)->btime[imat][jmat];
					(pw->plan)->dtime[i][j+1] = (atlantis->plan)->dtime[imat][jmat];
					/*casella stessa*/
					subMatrixToMatrix(i, j, id, &imat, &jmat);
					(pw->plan)->w[i][j] = (atlantis->plan)->w[imat][jmat];
					(pw->plan)->btime[i][j] = (atlantis->plan)->btime[imat][jmat];
					(pw->plan)->dtime[i][j] = (atlantis->plan)->dtime[imat][jmat];
					/*sotto*/
					subMatrixToMatrix(i+1, j, id, &imat, &jmat);
					(pw->plan)->w[i+1][j] = (atlantis->plan)->w[imat][jmat];
					(pw->plan)->btime[i+1][j] = (atlantis->plan)->btime[imat][jmat];
					(pw->plan)->dtime[i+1][j] = (atlantis->plan)->dtime[imat][jmat];
					Pthread_mutex_unlock(&mtx_atlantis);
				}
			}

			if( (i == 2) && (j > 1) && (j < ncol-2) ){
				Pthread_mutex_lock(&mtx_atlantis);
				/*sopra*/
				subMatrixToMatrix(i-1, j, id, &imat, &jmat);
				(pw->plan)->w[i-1][j] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i-1][j] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i-1][j] = (atlantis->plan)->dtime[imat][jmat];
				Pthread_mutex_unlock(&mtx_atlantis);
			}

			if( (i == nrow-3) && (j > 1) && (j < ncol-2) ){
				Pthread_mutex_lock(&mtx_atlantis);
				/*sotto*/
				subMatrixToMatrix(i+1, j, id, &imat, &jmat);
				(pw->plan)->w[i+1][j] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i+1][j] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i+1][j] = (atlantis->plan)->dtime[imat][jmat];
				Pthread_mutex_unlock(&mtx_atlantis);
			}

			if( (j == 2) && (i > 1) && (i < nrow-2) ){
				Pthread_mutex_lock(&mtx_atlantis);
				/*sinistra*/
				subMatrixToMatrix(i, j-1, id, &imat, &jmat);
				(pw->plan)->w[i][j-1] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i][j-1] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i][j-1] = (atlantis->plan)->dtime[imat][jmat];
				Pthread_mutex_unlock(&mtx_atlantis);
			}

			if( (j == ncol-3) && (i > 1) && (i < nrow-2) ){
				Pthread_mutex_lock(&mtx_atlantis);
				/*destra*/
				subMatrixToMatrix(i, j+1, id, &imat, &jmat);
				(pw->plan)->w[i][j+1] = (atlantis->plan)->w[imat][jmat];
				(pw->plan)->btime[i][j+1] = (atlantis->plan)->btime[imat][jmat];
				(pw->plan)->dtime[i][j+1] = (atlantis->plan)->dtime[imat][jmat];
				Pthread_mutex_unlock(&mtx_atlantis);
			}


				/*è il momento di applicare le regole*/
				esito = -10;
				/*riproduzione*/		
				k = i;
				l = j;
				subMatrixToMatrix(i, j, id, &imat, &jmat);
				switch((pw->plan)->w[i][j]){
					case SHARK:	if(!azione[imat][jmat]){
								esito = shark_rule2(pw, i, j, &k, &l);
							}
							break;
					case FISH:	if(!azione[imat][jmat])
								esito = fish_rule4(pw, i, j, &k, &l);
							break;
					case WATER:	break;
				}
				if(esito == -1){
					perror("applicazione regola\n");
					return 0;
				}
				/*si è riprodotto*/
				if(k != i || l != j){
					subMatrixToMatrix(k, l, id, &imat, &jmat);
					azione[imat][jmat] = 1;
					Pthread_mutex_lock(&mtx_atlantis);
					(atlantis->plan)->w[imat][jmat] = (pw->plan)->w[k][l];
					(atlantis->plan)->btime[imat][jmat] = (pw->plan)->btime[k][l];
					(atlantis->plan)->dtime[imat][jmat] = (pw->plan)->dtime[k][l];
					Pthread_mutex_unlock(&mtx_atlantis);
				}

				/*applico le regole di spostamento*/
				k = i;
				l = j;
				subMatrixToMatrix(i, j, id, &imat, &jmat);
				switch((pw->plan)->w[i][j]){
					case SHARK:	if(!azione[imat][jmat])
								esito = shark_rule1(pw, i, j, &k, &l);
							break;
					case FISH:	if(!azione[imat][jmat])
								esito = fish_rule3(pw, i, j, &k, &l);
							break;
					case WATER:  	break;
				}
				if(esito == -1){
					perror("applicazione regola\n");
					return 0;
				}
				/*si è mosso*/
				if(k != i || l != j){

					subMatrixToMatrix(k, l, id, &imat, &jmat);
					azione[imat][jmat] = 1;
					Pthread_mutex_lock(&mtx_atlantis);
					(atlantis->plan)->w[imat][jmat] = (pw->plan)->w[k][l];
					(atlantis->plan)->btime[imat][jmat] = (pw->plan)->btime[k][l];
					(atlantis->plan)->dtime[imat][jmat] = (pw->plan)->dtime[k][l];
					Pthread_mutex_unlock(&mtx_atlantis);
				}
				subMatrixToMatrix(i, j, id, &imat, &jmat);
				Pthread_mutex_lock(&mtx_atlantis);
				(atlantis->plan)->w[imat][jmat] = (pw->plan)->w[i][j];
				(atlantis->plan)->btime[imat][jmat] = (pw->plan)->btime[i][j];
				(atlantis->plan)->dtime[imat][jmat] = (pw->plan)->dtime[i][j];
				Pthread_mutex_unlock(&mtx_atlantis);
				azione[imat][jmat] = 1;

			/*************************************************************************************************************/
			/*ho finito di lavorare sulla cella; decremento i contatori*/
			if((i == 1) || (i == nrow-2) || (i == 2) || (i == nrow-3) || (j == 1) || (j == ncol-2) || (j == 2) || (j == ncol-3)){
					Pthread_mutex_lock(&mtx_matx);
					x = id;
					/*decremento la sottomatrice nella posizione soprastante*/
					if( (i == 1) || (i == 2) ){
						if(id < sez_per_fila)
							x += length_nsezioni;
						if( ((x-sez_per_fila) % length_nsezioni) != id)
							matx[(x-sez_per_fila) % length_nsezioni]--;
						if( matx[(x-sez_per_fila) % length_nsezioni] == 0 )
							if((err = pthread_cond_signal(&cond_matx[(x-sez_per_fila) % length_nsezioni])) != 0){
								errno = err;
								perror("signal condition");
								return 0;
							}	
						
					}
					x = id;
					/*decremento la matrice sottostante*/
					if( (i == nrow-2) || (i == nrow-3) ){
						if( ((x+sez_per_fila) % length_nsezioni) != id)
							matx[(x+sez_per_fila) % length_nsezioni]--;

						if( matx[(x+sez_per_fila) % length_nsezioni] == 0 )
							if((err = pthread_cond_signal(&cond_matx[(x+sez_per_fila) % length_nsezioni])) != 0){
								errno = err;
								perror("signal condition");
								return 0;
							}
					}
					x = id;
					/*decremento la matrice a sinistra*/
					if( (j == 1) || (j == 2) ){
						if((id % sez_per_fila) == 0)
							x += sez_per_fila;
						if( ((x-1) % length_nsezioni) != id)
							matx[(x-1) % length_nsezioni]--;

						if( matx[(x-1) % length_nsezioni] == 0 )
							if((err = pthread_cond_signal(&cond_matx[(x-1) % length_nsezioni])) != 0){
								errno = err;
								perror("signal condition");
								return 0;
							}
						
					}
					x = id;
					/*decremento la matrice a destra*/
					if( (j == ncol-2) || (j == ncol-3) ){
						if((id % sez_per_fila) == (sez_per_fila-1))
							x -= sez_per_fila;
						if( ((x+1) % length_nsezioni) != id)
							matx[(x+1) % length_nsezioni]--;

						if( matx[(x+1) % length_nsezioni] == 0 )
							if((err = pthread_cond_signal(&cond_matx[(x+1) % length_nsezioni])) != 0){
								errno = err;
								perror("signal condition");
								return 0;
							}
					}
					/*se sono su un angolo devo sbloccare anche la sottomatrice posta diagonalmente*/
					x = id;
					/*diagonale alto a sx*/
					if( (i == 1) && (j == 1) ){
						if(id < sez_per_fila)
							x += length_nsezioni;
						x = (x-sez_per_fila) % length_nsezioni;

						if((x % sez_per_fila) == 0)
							x += sez_per_fila;
						x = (x-1) % length_nsezioni;

						if( x != id )
							matx[x % length_nsezioni]--;

						if( matx[x % length_nsezioni] == 0 )
							if((err = pthread_cond_signal(&cond_matx[x % length_nsezioni])) != 0){
								errno = err;
								perror("signal condition");
								return 0;
							}
					}
					x = id;
					/*diagonale alto a dx*/
					if( (i == 1) && (j == ncol-2) ){
						if(id < sez_per_fila)
							x += length_nsezioni;
						x = (x-sez_per_fila) % length_nsezioni;

						if((x % sez_per_fila) == (sez_per_fila-1))
							x -= sez_per_fila;
						x = (x+1) % length_nsezioni;
					
						if( x != id )
							matx[x % length_nsezioni]--;

						if( matx[x % length_nsezioni] == 0 )
							if((err = pthread_cond_signal(&cond_matx[x % length_nsezioni])) != 0){
								errno = err;
								perror("signal condition");
								return 0;
							}
					}
					x = id;
					/*diagonale basso a sx*/
					if( (i == nrow-2) && (j == 1) ){
						x = (x+sez_per_fila) % length_nsezioni;

						if((x % sez_per_fila) == 0)
							x += sez_per_fila;
						x = (x-1) % length_nsezioni;
				
						if( x != id )
							matx[x % length_nsezioni]--;

						if( matx[x % length_nsezioni] == 0 )
							if((err = pthread_cond_signal(&cond_matx[x % length_nsezioni])) != 0){
								errno = err;
								perror("signal condition");
								return 0;
							}
					} 
					x = id;
					/*diagonale basso a dx*/
					if( (i == nrow-2) && (j == ncol-2) ){
						x = (x+sez_per_fila) % length_nsezioni;

						if((x % sez_per_fila) == (sez_per_fila-1))
							x -= sez_per_fila;
						x = (x+1) % length_nsezioni;

						if( x != id )
							matx[x % length_nsezioni]--;

						if( matx[x % length_nsezioni] == 0 )
							if((err = pthread_cond_signal(&cond_matx[x % length_nsezioni])) != 0){
								errno = err;
								perror("signal condition");
								return 0;
							}
					}
					Pthread_mutex_unlock(&mtx_matx);
								
			      }
		}
	}
	return 1;
}

/*funzione di lock su un mutex*/
void Pthread_mutex_lock(pthread_mutex_t *mtx){
	int err;
	if((err = pthread_mutex_lock(mtx)) != 0){
		errno = err;
		perror("lock");
		pthread_exit((int*)&errno);
	}
}

/*funzione di unlock su un mutex*/
void Pthread_mutex_unlock(pthread_mutex_t *mtx){
	int err;
	if((err = pthread_mutex_unlock(mtx)) != 0){
		errno = err;
		perror("unlock");
		pthread_exit((int*)&errno);
	}
}

/*quando arriva il segnale SIGINT imposto il flag fine ad 1(così i thread capiscono che devono terminare) e inoltro il segnale a visualizer*/
void gestoreSIGINT(){
	fine = 1;
	
}

/*quando arriva il segnale SIGTERM imposto il flag fine ad 1(così i thread capiscono che devono terminare) e inoltro il segnale a visualizer*/
void gestoreSIGTERM(){
	fine = 1;
	
}

/*all arrivo del segnale SIGUSR1 memorizzo lo stato attuale del pianeta su file*/
void gestoreSIGUSR1(){
	FILE *f;
	
	fprintf(stderr, "apertura wator.check\n");
	if((f = fopen("./wator.check", "w")) == NULL){
		perror("errore apertura wator.check");
		return;
	}
	if(print_planet(f, atlantis->plan) == -1){
		perror("in print_planet wator.check");
	}
	else
		printf("scrittura su file effettuata\n");	
	fclose(f);
}

/*il thread dispatcher ha il compito di suddividere la matrice del pianeta in sottomatrici*/
void* dispatcher(void* arg){
	planet_t *p = NULL;
	unsigned int nrow, ncol, k, n;
	int i = 0, j = 0, row_counter, col_counter, id, esito, i_checkpoint = 0, j_checkpoint = 0, err, contatore = 0;

	nrow = atlantis->plan->nrow;
	ncol = atlantis->plan->ncol;

	/*preparo la matrice azione*/
	azione = (int**)malloc(nrow*sizeof(int*));
	if(azione == NULL){
		fprintf(stderr, "errore allocazione 'azione'\n");
		abortisci = TRUE;
		kill(pid, SIGINT);
		pthread_exit((void*)EXIT_FAILURE);
	}
	
	for(i = 0; i < nrow; i++){
		azione[i] = (int*)malloc(ncol*sizeof(int));
		if(azione[i] == NULL){
			fprintf(stderr, "errore allocazione 'azione'\n");
			libera(azione, i);
			abortisci = TRUE;
			kill(pid, SIGINT);
			pthread_exit((void*)EXIT_FAILURE);
		}
	}
	
	i = 0;
	Pthread_mutex_lock(&mtx_partenza);
		while(!partenza && !fine){
			if((err = pthread_cond_wait(&cond_partenza, &mtx_partenza)) != 0){
				errno = err;
				perror("wait condition");
				abortisci = TRUE;
				kill(pid, SIGINT);
				libera(azione, nrow);
				Pthread_mutex_unlock(&mtx_partenza);
				pthread_exit((int*)&errno);
			}
		}
	Pthread_mutex_unlock(&mtx_partenza);
	while(!fine && !abortisci){
		id = 0;
		flag_matrice_divisa = FALSE;
		while(i_checkpoint < nrow){
			chronon_succ = FALSE;
			azzera(azione, nrow, ncol);
			/*preparo la sottomatrice*/
			if(nrow-(i_checkpoint%nrow) >= K-2)
				k = K;
			else
				k = (nrow-(i_checkpoint%nrow))+2;
	 		if(ncol-(j_checkpoint%ncol) >= N-2)
				n = N;
			else
				n = (ncol-(j_checkpoint%ncol))+2;
			
			p = new_planet(k, n);
			if(p == NULL){
				abortisci = TRUE;
				kill(pid, SIGINT);
				libera(azione, nrow);
				pthread_exit((void*)EXIT_FAILURE);
			}

			i = i_checkpoint;
			j = j_checkpoint;
			for(row_counter = 0; row_counter < k; row_counter++){
				if(i == 0)
					i = nrow;
				for(col_counter = 0; col_counter < n; col_counter++){
					if(j == 0)
						j = ncol;
					Pthread_mutex_lock(&mtx_atlantis);
					p->w[row_counter][col_counter] = (atlantis->plan)->w[i-1][j-1];
					p->btime[row_counter][col_counter] = (atlantis->plan)->btime[i-1][j-1];
					p->dtime[row_counter][col_counter] = (atlantis->plan)->dtime[i-1][j-1];
					Pthread_mutex_unlock(&mtx_atlantis);
					j = (j+1)%ncol;
				}
				j = j_checkpoint;
				i = (i+1)%nrow;
			}
			/*sottomatrice preparata*/
			j_checkpoint += n-2;
			contatore++;
			if(j_checkpoint >= ncol){
				sez_per_fila = contatore;
				i_checkpoint += k-2;
				j_checkpoint = 0;
				contatore = 0;
			}
			/*inserisco nella coda*/
			Pthread_mutex_lock(&mtx_coda);
			esito = insert_queue(&queue, p, id);
			if(esito == -1){
				fprintf(stderr, "errore insert_queue\n");
				Pthread_mutex_unlock(&mtx_coda);
				abortisci = TRUE;
				kill(pid, SIGINT);
				libera(azione, nrow);
				free_planet(p);
				pthread_exit((void*)EXIT_FAILURE);
			}
			Pthread_mutex_unlock(&mtx_coda);
			id++;
			free_planet(p);
			
		}
		length_nsezioni = id;
		nsezioni = calloc(sizeof(int), id);
		if(nsezioni == NULL){
			fprintf(stderr, "errore allocazione 'nsezioni'\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			libera(azione, nrow);
			pthread_exit((void*)EXIT_FAILURE);
		}
		matx = calloc(sizeof(int), id);
		if(matx == NULL){
			fprintf(stderr, "errore allocazione 'matx'\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			libera(azione, nrow);
			free(nsezioni);
			pthread_exit((void*)EXIT_FAILURE);
		}
		cond_matx = (pthread_cond_t*)malloc(id*sizeof(pthread_cond_t));
		if(cond_matx == NULL){
			fprintf(stderr, "errore allocazione 'cond_matx'\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			libera(azione, nrow);
			free(nsezioni);
			free(matx);
			pthread_exit((void*)EXIT_FAILURE);
		}
		for(i = 0; i < id; i++){
			if((err = pthread_cond_init(&cond_matx[i], NULL)) != 0){
				fprintf(stderr, "errore nella cond_init del thread\n");
				abortisci = TRUE;
				kill(pid, SIGINT);
				libera(azione, nrow);
				free(nsezioni);
				free(matx);
				free(cond_matx);
				pthread_exit((void*)EXIT_FAILURE);;
			}
		}
		Pthread_mutex_lock(&mtx_matriceDivisa);
		flag_matrice_divisa = TRUE;
		if((err = pthread_cond_broadcast(&cond_matriceDivisa)) != 0){
			errno = err;
			perror("signal");
			abortisci = TRUE;
			kill(pid, SIGINT);
			libera(azione, nrow);
			free(nsezioni);
			free(matx);
			free(cond_matx);
			Pthread_mutex_unlock(&mtx_matriceDivisa);
			pthread_exit((int*)&errno);
		}
		Pthread_mutex_unlock(&mtx_matriceDivisa);

		Pthread_mutex_lock(&mtx_collectorGo);
		collector_go = TRUE;
		if((err = pthread_cond_signal(&cond_collectorGo)) != 0){
			errno = err;
			perror("signal");
			abortisci = TRUE;
			kill(pid, SIGINT);
			libera(azione, nrow);
			free(nsezioni);
			free(matx);
			free(cond_matx);
			Pthread_mutex_unlock(&mtx_collectorGo);
			pthread_exit((int*)&errno);
		}
		Pthread_mutex_unlock(&mtx_collectorGo);
		
		/*risveglio i worker interrotti sulla condition variable*/
		Pthread_mutex_lock(&mtx_coda);
		if((err = pthread_cond_broadcast(&cond_empty)) != 0){
			errno = err;
			perror("broadcast");
			abortisci = TRUE;
			kill(pid, SIGINT);
			libera(azione, nrow);
			free(nsezioni);
			free(matx);
			free(cond_matx);
			Pthread_mutex_unlock(&mtx_coda);
			pthread_exit((int*)&errno);
		}
		Pthread_mutex_unlock(&mtx_coda);
		/*attendo fino a che non è finita l'elaborazione relativa al chronon corrente*/
		Pthread_mutex_lock(&mtx_chrononSucc);		
		while(!chronon_succ && !abortisci){
				if((err = pthread_cond_wait(&cond_chrononSucc, &mtx_chrononSucc)) != 0){
				errno = err;
				perror("wait condition");
				abortisci = TRUE;
				kill(pid, SIGINT);
				libera(azione, nrow);
				free(nsezioni);
				free(matx);
				free(cond_matx);
				Pthread_mutex_unlock(&mtx_chrononSucc);
				pthread_exit((int*)&errno);
			}
		}
		Pthread_mutex_unlock(&mtx_chrononSucc);
		i_checkpoint = 0;
		j_checkpoint = 0;
		free(matx);
		free(nsezioni);
		free(cond_matx);
				
	}
	Pthread_mutex_lock(&mtx_matriceDivisa);
	end_work = TRUE;
	if((err = pthread_cond_broadcast(&cond_matriceDivisa)) != 0){
			errno = err;
			perror("signal");
			abortisci = TRUE;
			kill(pid, SIGINT);
			libera(azione, nrow);
			Pthread_mutex_unlock(&mtx_matriceDivisa);
			pthread_exit((int*)&errno);
	}
	Pthread_mutex_unlock(&mtx_matriceDivisa);

	fprintf(stderr, "end_work = true\n");
	Pthread_mutex_lock(&mtx_coda);
	if((err = pthread_cond_broadcast(&cond_empty)) != 0){
		errno = err;
		perror("broadcast");
		abortisci = TRUE;
		kill(pid, SIGINT);
		libera(azione, nrow);
		Pthread_mutex_unlock(&mtx_coda);
		pthread_exit((int*)&errno);
	}
	Pthread_mutex_unlock(&mtx_coda);

	Pthread_mutex_lock(&mtx_collectorGo);
	if((err = pthread_cond_signal(&cond_collectorGo)) != 0){
		errno = err;
		perror("signal");
		abortisci = TRUE;
		kill(pid, SIGINT);
		Pthread_mutex_unlock(&mtx_collectorGo);
		pthread_exit((int*)&errno);
	}
	Pthread_mutex_unlock(&mtx_collectorGo);
	pthread_exit(0);
}

/*thread che si occupa di sincronizzare il lavoro svolto dai thread dispatcher e worker*/
void* collector(void* arg){
	int chronon = (int) arg, nwork, err;
	/*stampo la config iniziale*/
	Pthread_mutex_lock(&mtx_atlantis);
	nwork = atlantis->nwork;
	Pthread_mutex_unlock(&mtx_atlantis);
	fprintf(stderr, "chron : %d\n", atlantis->chronon);
	send_to_visualizer('O');
	Pthread_mutex_lock(&mtx_partenza);
	partenza = TRUE;
	if((err = pthread_cond_signal(&cond_partenza)) != 0){
		errno = err;
		perror("signal");
		abortisci = TRUE;
		kill(pid, SIGINT);
		Pthread_mutex_unlock(&mtx_partenza);
		send_to_visualizer('T');
		kill(pid, SIGTERM);
		Pthread_mutex_lock(&mtx_chrononSucc);		
		chronon_succ = TRUE;
		if((err = pthread_cond_signal(&cond_chrononSucc)) != 0){
			errno = err;
			perror("signal");
			abortisci = TRUE;
			kill(pid, SIGINT);
			Pthread_mutex_unlock(&mtx_chrononSucc);
			pthread_exit((int*)&errno);
		}	
		Pthread_mutex_unlock(&mtx_chrononSucc);	
		pthread_exit((int*)&errno);
	}
	Pthread_mutex_unlock(&mtx_partenza);
	while((last_work < nwork) && !abortisci){
		Pthread_mutex_lock(&mtx_collectorGo);
		while(!collector_go && !abortisci && !end_work){
				if((err = pthread_cond_wait(&cond_collectorGo, &mtx_collectorGo)) != 0){
				errno = err;
				perror("wait condition");
				abortisci = TRUE;
				kill(pid, SIGINT);
				Pthread_mutex_unlock(&mtx_collectorGo);
				send_to_visualizer('T');
				kill(pid, SIGTERM);
				chronon_succ = TRUE;
				pthread_exit((int*)&errno);
			}
		}
		collector_go = FALSE;
		Pthread_mutex_unlock(&mtx_collectorGo);

		if(last_work >= nwork || abortisci)
			break;

		Pthread_mutex_lock(&mtx_nsezioni);
		while(!abortisci && !end_work && (reduce(nsezioni, length_nsezioni) != length_nsezioni)){
			if((err = pthread_cond_wait(&cond_nsezioni, &mtx_nsezioni)) != 0){
				errno = err;
				perror("wait condition");
				abortisci = TRUE;
				kill(pid, SIGINT);
				Pthread_mutex_unlock(&mtx_nsezioni);
				send_to_visualizer('T');
				kill(pid, SIGTERM);	
				chronon_succ = TRUE;
				if((err = pthread_cond_signal(&cond_chrononSucc)) != 0){
					errno = err;
					perror("signal");
					abortisci = TRUE;
					kill(pid, SIGINT);
					pthread_exit((int*)&errno);
				}	
				Pthread_mutex_unlock(&mtx_chrononSucc);	
				pthread_exit((int*)&errno);
			}
		}
		if(last_work >= nwork || abortisci){
			Pthread_mutex_unlock(&mtx_nsezioni);
			break;
		}
		reset(nsezioni, length_nsezioni);
		Pthread_mutex_unlock(&mtx_nsezioni);	

			

		Pthread_mutex_lock(&mtx_atlantis);
		atlantis->chronon++;
		atlantis->nf = fish_count(atlantis->plan);
		atlantis->ns = shark_count(atlantis->plan);
		Pthread_mutex_unlock(&mtx_atlantis);
		/*ogni x chronon*/
		if(((atlantis->chronon) % chronon) == 0){
			fprintf(stderr, "chron : %d\n", atlantis->chronon);
			send_to_visualizer('O');
		}
		Pthread_mutex_lock(&mtx_chrononSucc);		
		chronon_succ = TRUE;
		if((err = pthread_cond_signal(&cond_chrononSucc)) != 0){
			errno = err;
			perror("signal");
			abortisci = TRUE;
			kill(pid, SIGINT);
			Pthread_mutex_unlock(&mtx_chrononSucc);
			pthread_exit((int*)&errno);
		}	
		Pthread_mutex_unlock(&mtx_chrononSucc);		
	}
	send_to_visualizer('T');
	kill(pid, SIGTERM);
	Pthread_mutex_lock(&mtx_chrononSucc);		
	chronon_succ = TRUE;
	if((err = pthread_cond_signal(&cond_chrononSucc)) != 0){
		errno = err;
		perror("signal");
		abortisci = TRUE;
		kill(pid, SIGINT);
		Pthread_mutex_unlock(&mtx_chrononSucc);
		pthread_exit((int*)&errno);
	}	
	Pthread_mutex_unlock(&mtx_chrononSucc);	
	pthread_exit(0);

}

/*i thread worker hanno il compito di lavorare su delle sottomatrici del pianeta*/
void* worker(void* arg){
	int wid = (int) arg, err, err_print, i, j;
	FILE *f;
	char nomefile[101];
	sezione *s = NULL;
	wator_t *pw;

	snprintf(nomefile, 100, "wator_worker_%d", wid);

	while(!end_work && !abortisci){
		/*i worker appena vengono creati creano a loro volta un file*/
		if((f = fopen(nomefile, "w")) == NULL){
			fprintf(stderr, "errore in apertura file worker\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			pthread_exit((void*)EXIT_FAILURE);	
		}

		Pthread_mutex_lock(&mtx_matriceDivisa);
		while(!flag_matrice_divisa && !end_work){
			if((err = pthread_cond_wait(&cond_matriceDivisa, &mtx_matriceDivisa)) != 0){
				errno = err;
				perror("wait condition");
				abortisci = TRUE;
				Pthread_mutex_unlock(&mtx_matriceDivisa);
				pthread_exit((int*)&errno);
			}
		}
		Pthread_mutex_unlock(&mtx_matriceDivisa);

		Pthread_mutex_lock(&mtx_coda);
		while( (isEmpty(queue)) && !end_work && !abortisci){
			if((err = pthread_cond_wait(&cond_empty, &mtx_coda)) != 0){
				errno = err;
				perror("wait condition");
				abortisci = TRUE;
				Pthread_mutex_unlock(&mtx_coda);
				pthread_exit((int*)&errno);
			}
		}
		if(end_work || abortisci){
			Pthread_mutex_unlock(&mtx_coda);
			break;
		}
		s = get_queue(&queue);
		Pthread_mutex_unlock(&mtx_coda);
			
		if(s->id == -1){
			fprintf(stderr, "errore nella get_queue in un worker\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			pthread_exit((void*)EXIT_FAILURE);	
		}
		
		/*funzione di elaborazione della sottomatrice*/
		err_print = print_planet(f, s->plan);
		if(err_print == -1){
			perror("print_planet in worker\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			pthread_exit((void*)EXIT_FAILURE);
		}
		if(fclose(f) != 0){
			fprintf(stderr, "errore in chiusura file\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			pthread_exit((void*)EXIT_FAILURE);
		}
		pw = new_wator(nomefile);
		if(pw == NULL){
			perror("new_wator\n");
			fprintf(stderr, "pw è nullo\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			pthread_exit((void*)EXIT_FAILURE);
		}
		for(i = 0; i < (s->plan)->nrow; i++){
			for(j = 0; j < (s->plan)->ncol; j++){
				(pw->plan)->btime[i][j] = (s->plan)->btime[i][j];
				(pw->plan)->dtime[i][j] = (s->plan)->dtime[i][j];
			}
		}
		
		if(!elabora(s->id, pw)){
			fprintf(stderr, "elaborazione fallita\n");
			abortisci = TRUE;
			kill(pid, SIGINT);
			free_wator(pw);
			pthread_exit((void*)EXIT_FAILURE);
		}
		
		free_wator(pw);

		Pthread_mutex_lock(&mtx_nsezioni);
		nsezioni[s->id] = 1;
		if(reduce(nsezioni, length_nsezioni) == length_nsezioni){
			if((err = pthread_cond_signal(&cond_nsezioni)) != 0){
			errno = err;
			perror("signal");
			abortisci = TRUE;
			kill(pid, SIGINT);
			Pthread_mutex_unlock(&mtx_nsezioni);
			pthread_exit((int*)&errno);
			}
		}
		Pthread_mutex_unlock(&mtx_nsezioni);

		Pthread_mutex_lock(&mtx_coda);
		while( (isEmpty(queue)) && !end_work && !abortisci){
			if((err = pthread_cond_wait(&cond_empty, &mtx_coda)) != 0){
				errno = err;
				perror("wait condition");
				abortisci = TRUE;
				Pthread_mutex_unlock(&mtx_coda);
				pthread_exit((int*)&errno);
			}
		}
		Pthread_mutex_unlock(&mtx_coda);
	}
	Pthread_mutex_lock(&mtx_lastwork);
	last_work++;
	Pthread_mutex_unlock(&mtx_lastwork);

	Pthread_mutex_lock(&mtx_nsezioni);
	if((err = pthread_cond_signal(&cond_nsezioni)) != 0){
			errno = err;
			perror("signal");
			abortisci = TRUE;
			kill(pid, SIGINT);
			Pthread_mutex_unlock(&mtx_nsezioni);
			pthread_exit((int*)&errno);
	}
	Pthread_mutex_unlock(&mtx_nsezioni);
	pthread_exit(0);
}

/*thread dedito alla gestione dei segnali*/
void* handler(void* arg){
	sigset_t set;
	int err, sig;

	/*imposto la maschera per gestire correttamente i segnali in arrivo*/
	if(sigaddset(&set, SIGINT) == -1){
		perror("sigaddset SIGINT");
		pthread_exit((void*)EXIT_FAILURE);
	}

	if(sigaddset(&set, SIGTERM) == -1){
		perror("sigaddset SIGTERM");
		pthread_exit((void*)EXIT_FAILURE);
	}

	if(sigaddset(&set, SIGUSR1) == -1){
		perror("sigaddset SIGUSR1");
		pthread_exit((void*)EXIT_FAILURE);
	}

	if((err = pthread_sigmask(SIG_SETMASK,&set,NULL)) != 0){
		fprintf(stderr, "errore mascheramento %d\n", err);
		pthread_exit((void*)EXIT_FAILURE);
	}


	/*ciclo fino alla terminazione del processo che deve avvenire all'arrivo dei segnali SIGINT o SIGTERM*/
	while(!fine){
		if((err = sigwait(&set, &sig)) != 0){
			fprintf(stderr, "errore sigwait %d\n", err);
			pthread_exit((void*)EXIT_FAILURE);
		}
		/*gestisco i segnali opportunamente*/
		switch(sig){
			case SIGINT : 	gestoreSIGINT();
					break;
			
			case SIGTERM: 	gestoreSIGTERM();
					break;

			case SIGUSR1:	fprintf(stderr, "segnale sigusr1 arrivato\n");
					gestoreSIGUSR1();
					break;
		}
		
	}
	pthread_exit(0);
		
}

int main(int argc, char *argv[]){
	extern char *optarg;
	extern int optind, opterr, optopt;
	int opt, nwork = NWORK_DEF, chronon = CHRON_DEF, err, status, i;
	int  n_flag = 0, v_flag = 0, f_flag = 0; /*utilizzati per controllare che non vengano passati comandi doppi*/
	char dumpfile[101], file[101], arg_nrow[101], arg_ncol[101];
	sigset_t set;
	pthread_t tid;
	pthread_t dispid;
	pthread_t collectid;
	pthread_t *wid;
	

	mtrace();
	if(argc < 2 ){
		fprintf(stderr, "numero argomenti errato\n");
		exit(EXIT_FAILURE);
	}

	memset(dumpfile, 0, sizeof(dumpfile));
	memset(file, 0, sizeof(file));

	/*parsing argomenti*/
	while((opt = getopt(argc, argv, "n:v:f:")) != -1){
		switch(opt){
			case 'n':	if(n_flag >= 1){
						fprintf(stderr, "argomenti doppi\n");
						exit(EXIT_FAILURE);
					} 
					else{
						nwork = atoi(optarg);
						n_flag++;
					}
					break;

			case 'v':	if(v_flag >= 1){
						fprintf(stderr, "argomenti doppi\n");
						exit(EXIT_FAILURE);
					} 
					else{
						chronon = atoi(optarg);
						v_flag++;
					}
					break;

			case 'f':	if(f_flag >= 1){
						fprintf(stderr, "argomenti doppi\n");
						exit(EXIT_FAILURE);
					} 
					else{
						if(strncpy(dumpfile, optarg, 100) == NULL){
							fprintf(stderr, "dumpfile errato\n");
							exit(EXIT_FAILURE);
						}
						f_flag++;
					}
					break;
		
			default:	fprintf(stderr, "argomenti errati\n");
					exit(EXIT_FAILURE);
					break;
		}
	}

	/*manca il file da cui caricare il pianeta*/
	if((optind +1) != argc){
		fprintf(stderr, "argomenti errati deve esserci un file di configurazione\n");
		exit(EXIT_FAILURE);
	}
	if(strncpy(file, argv[optind], 100) == NULL){
		fprintf(stderr, "dumpfile errato\n");
		exit(EXIT_FAILURE);
	}

	/*non è presente il dumpfile*/
	if(!f_flag)
		if(strncpy(dumpfile, "stdout", 100) == NULL){
			fprintf(stderr, "dumpfile errato\n");
			exit(EXIT_FAILURE);
		}

	fprintf(stderr,"nwork : %d   chronon :  %d      dumpfile :  %s      file :   %s\n",  nwork, chronon, dumpfile, file);
	
	/*inizializzo la struttura wator*/
	if((atlantis = new_wator(file)) == NULL){
		perror("in new_wator");
		exit(EXIT_FAILURE);
	} 

	/*imposto il numero di worker*/
	if((atlantis = set_nwork(atlantis, nwork)) == NULL){
		perror("in set_nwork");
		free_wator(atlantis);
		exit(EXIT_FAILURE);
	}

	if(snprintf(arg_nrow, 100, "%d", (atlantis->plan)->nrow) < 0){
		fprintf(stderr, "snprintf\n");
		free_wator(atlantis);
		exit(EXIT_FAILURE);
	}	
	if(snprintf(arg_ncol, 100, "%d", (atlantis->plan)->ncol) < 0){
		fprintf(stderr, "snprintf\n");
		free_wator(atlantis);
		exit(EXIT_FAILURE);
	}	

	/*maschero i segnali fino a che non viene creato il thread addetto alla gestione dei segnali*/
	if(sigfillset(&set) == -1){
		perror("sigfillset");
		free_wator(atlantis);
		exit(EXIT_FAILURE);
	}
	
	if((err = pthread_sigmask(SIG_SETMASK,&set,NULL)) != 0){
		fprintf(stderr, "errore mascheramento %d\n", err);
		free_wator(atlantis);
		exit(EXIT_FAILURE);
	}
	
	/*inizializzo la struttura per la socket*/
	sa.sun_family = DOMAIN;
	snprintf(sa.sun_path, UNIX_PATH_MAX, SOCK_NAME);

	/*creo il figlio che ha il compito di mandare in esecuzione visualizer*/
	switch(pid = fork()){
		case -1:	perror("creazione figlio");
				free_wator(atlantis);
				exit(EXIT_FAILURE);
				break;

		case 0:		execlp("./visualizer", "./visualizer", dumpfile, arg_nrow, arg_ncol, (char*)NULL);
				perror("errore exec visualizer");
				free_wator(atlantis);
				exit(EXIT_FAILURE);
				break;
	}
	
	
	init_queue(&queue);
	
	/*creazione del thread addetto alla gestione dei segnali*/
	if((err = pthread_create(&tid, NULL, &handler, NULL)) != 0){
			fprintf(stderr, "errore nella creazione del handler\n");
			kill(pid, SIGINT);
			send_to_visualizer('T');
			Pthread_mutex_lock(&mtx_atlantis);
			free_wator(atlantis);
			Pthread_mutex_unlock(&mtx_atlantis);
			exit(EXIT_FAILURE);
	}
	fprintf(stderr,"pid : %d\n", pid);

	/*creo dispatcher*/
	if((err = pthread_create(&dispid, NULL, &dispatcher, NULL)) != 0){
			fprintf(stderr, "errore nella creazione del dispatcher\n");
			kill(pid, SIGINT);
			send_to_visualizer('T');
			Pthread_mutex_lock(&mtx_atlantis);
			free_wator(atlantis);
			Pthread_mutex_unlock(&mtx_atlantis);
			exit(EXIT_FAILURE);
	}
	/*creo collector*/
	if((err = pthread_create(&collectid, NULL, &collector, chronon)) != 0){
			fprintf(stderr, "errore nella creazione del dispatcher\n");
			kill(pid, SIGINT);
			send_to_visualizer('T');
			Pthread_mutex_lock(&mtx_atlantis);
			free_wator(atlantis);
			Pthread_mutex_unlock(&mtx_atlantis);
			exit(EXIT_FAILURE);
	}

	/*alloco il vettore wid che conterrà l identificatori dei thread worker*/
	Pthread_mutex_lock(&mtx_atlantis);
	wid = (pthread_t*)malloc((atlantis->nwork)*sizeof(pthread_t));
	Pthread_mutex_unlock(&mtx_atlantis);
	if(wid == NULL){
		perror("in malloc wid");
		kill(pid, SIGINT);
		Pthread_mutex_lock(&mtx_atlantis);
		free_wator(atlantis);
		Pthread_mutex_unlock(&mtx_atlantis);
		exit(EXIT_FAILURE);
	}

	/*creo i worker*/
	for(i = 0; i < (atlantis->nwork); i++){
		if((err = pthread_create(&wid[i], NULL, &worker, i)) != 0){
			fprintf(stderr, "errore nella creazione del worker\n");
			kill(pid, SIGINT);
			free(wid);
			Pthread_mutex_lock(&mtx_atlantis);
			free_wator(atlantis);
			Pthread_mutex_unlock(&mtx_atlantis);
			exit(EXIT_FAILURE);
		}
	}

	/*attendo la terminazione del visualizer*/
	pid = waitpid(pid, &status, 0);
	fprintf(stderr, "visualizer terminato\n");

	/*attendo la terminazione di dispatcher*/
	if((err = pthread_join(dispid, (void *)&status)) != 0){
		fprintf(stderr, "errore nella join\n");
		free(wid);
		Pthread_mutex_lock(&mtx_atlantis);
		free_wator(atlantis);
		Pthread_mutex_unlock(&mtx_atlantis);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "dispatcher terminato\n");
	
	/*attendo la terminazione di collector*/
	if((err = pthread_join(collectid, (void *)&status)) != 0){
		fprintf(stderr, "errore nella join\n");
		free(wid);
		Pthread_mutex_lock(&mtx_atlantis);
		free_wator(atlantis);
		Pthread_mutex_unlock(&mtx_atlantis);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "collector terminato\n");

	/*attendo la terminazione dei worker*/
	for(i = 0; i < (atlantis->nwork); i++){
		if((err = pthread_join(wid[i], (void *)&status)) != 0){
			fprintf(stderr, "errore nella creazione del worker\n");
			free(wid);
			Pthread_mutex_lock(&mtx_atlantis);
			free_wator(atlantis);
			Pthread_mutex_unlock(&mtx_atlantis);
			exit(EXIT_FAILURE);
		}
	}
	fprintf(stderr, "workers terminati\n");
	libera(azione, (atlantis->plan)->nrow);
	free(wid);
	/*attendo terminazione handler*/
	if((err = pthread_join(tid, (void *)&status)) != 0){
		fprintf(stderr, "errore nella join\n");
		Pthread_mutex_lock(&mtx_atlantis);
		free_wator(atlantis);
		Pthread_mutex_unlock(&mtx_atlantis);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "terminazione gentile\n");
	free_wator(atlantis);
	muntrace();
	return 0;
}












































