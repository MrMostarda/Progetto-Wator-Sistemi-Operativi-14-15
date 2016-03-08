/** \file processo_wator.h
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

#define NWORK_DEF 2
#define CHRON_DEF 1
#define M 1000
#define K 12
#define N 12
#define TRUE 1
#define FALSE 0
#define DOMAIN AF_UNIX
#define TYPE SOCK_STREAM
#define SOCK_NAME "./visual.sck"
#define UNIX_PATH_MAX 108
#define FINITO -1
#define DA_FARE 0;


/*la struttura che rappresenta la sottomatrice del pianeta*/
typedef struct _sezione{
	planet_t *plan;
	int id;
}sezione;

/*la coda condivisa che contiene le sottomatrici del pianeta*/
typedef struct _coda{
	sezione sez[M];
	int top, bottom;
}coda;

/*inizializza la coda
	/param cod: la coda
*/
void init_queue(coda *cod);

/*inserisce una sottomatrice del pianeta nella coda
	/param cod: la coda nella quale inserire la sottomatrice
	/param pianeta: la sottomatrice
	/param id: numero che identifica la sottomatrice

	/retval 0: tutto ok
	/retval -1: errore
*/
int insert_queue(coda* cod, planet_t *pianeta, int id);

/*estrae un elemento dalla coda
	/param cod: la coda nella quale inserire la sottomatrice
	
	/retval sezione*: elemento della coda restituito(id = -1 in caso di errore)
*/
sezione* get_queue(coda* cod);

/*verifica se la coda è vuota
	/param cod: la coda da verificare

	/retval 1: è vuota
	/retval 0: non è vuota
*/
int isEmpty(coda cod);

/*verifica se la coda è piena
	/param cod: la coda da verificare

	/retval 1: è piena
	/retval 0: non è piena
*/
int isFull(coda cod);

/*implementa il meccanismo di lock*/
void Pthread_mutex_lock(pthread_mutex_t *mtx);

/*implementa il meccanismo di unlock*/
void Pthread_mutex_unlock(pthread_mutex_t *mtx);

/*gestore del segnale SIGINT*/
void gestoreSIGINT();

/*gestore del segnale SIGTERM*/
void gestoreSIGTERM();

/*gestore del segnale SIGUSR1*/
void gestoreSIGUSR1();

/*funzione che parte alla creazione del thread dispatcher*/
void* dispatcher(void* arg);

/*funzione che parte alla creazione del thread collector*/
void* collector(void* arg);

/*funzione che parte alla creazione del thread worker*/
void* worker(void* arg);

/*funzione che parte alla creazione del thread handler ed ha il compito di gestire i segnali*/
void* handler(void* arg);

/*invio la matrice a visualizer attraverso il socket/
	/param flag: se flag = 'O' (ok) segnalo al visualizer che è in arrivo la matrice
		     se flag = 'T' (termina) segnalo al visualizer di terminare l'elaborazione
*/
void send_to_visualizer(char flag);

/*calcola la somma degli elementi di un array di interi
	/param array: l' array di interi
	/param len: la lunghezza dell' array

	/retval: la somma
*/
int reduce(int array[], int len);

/*azzera un array di interi
	/param array: l' array di interi
	/param len: la lunghezza dell' array
*/
void reset(int array[], int len);

/*azzera matrice di interi*/
void azzera(int **array, int row, int col);

/*trasforma le coordinate di una sottomatrice in coordinate della matrice completa/
	/param i,j: le coordinate della sottomatrice
	/param id: id sottomatrice
	/param k,l: le coordiante finali nella matrice completa
*/
void subMatrixToMatrix(int i, int j, int id, int *k, int *l);


















