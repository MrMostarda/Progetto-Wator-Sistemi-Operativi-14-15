/** \wator.c
       \author Matteo Anselmi
     Si dichiara che il contenuto di questo file e' in ogni sua parte opera
     originale dell' autore.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "wator.h"

/** trasforma una cella in un carattere
   \param a cella da trasformare

   \retval 'W' se a contiene WATER
   \retval 'S' se a contiene SHARK
   \retval 'F' se a contiene FISH
   \retval '?' in tutti gli altri casi
 */
char cell_to_char (cell_t a){
	char tmp;
	switch(a){
		case WATER: 	tmp = 'W';
				break;
		case SHARK:	tmp = 'S';
				break;
		case FISH:	tmp = 'F';
				break;
		default:	tmp = '?';
				break;
	}
	return tmp;
}

/** trasforma un carattere in una cella
   \param c carattere da trasformare

   \retval WATER se c=='W'
   \retval SHARK se c=='S'
   \retval FISH se c=='F'
   \retval -1 in tutti gli altri casi
 */
int char_to_cell (char c){
	int tmp;
	switch(c){
		case 'W': 	tmp = WATER;
				break;
		case 'S':	tmp = SHARK;
				break;
		case 'F':	tmp = FISH;
				break;
		default:	tmp = -1;
				break;
	}
	return tmp;
}

/** crea un nuovo pianeta vuoto (tutte le celle contengono WATER) utilizzando 
    la rappresentazione con un vettore di puntatori a righe
    \param nrow numero righe
    \param numero colonne

    \retval NULL se si sono verificati problemi nell'allocazione
    \retval p puntatore alla matrice allocata altrimenti
*/

/*utilizzo una variabile flag che mi segnala gli errori nell'allocazione della memoria
	\flag = 0: tutto bene;
	\flag = 1: errore nell'allocamento della memoria

	se il flag è diverso da 0 devo ripulire lo heap dalla memoria già allocata
*/
planet_t * new_planet (unsigned int nrow, unsigned int ncol){
	planet_t *p;
	int i = 0, j, flag = 0, row = nrow, col = ncol;

	/*deve esserci almeno una riga e una colonna*/
	if(row <= 0 || col <= 0)
		return NULL;

	p =(planet_t*)malloc(sizeof (planet_t));
	if(p == NULL)
		return NULL;
	
	p->nrow = nrow;
	p->ncol = ncol;

	/*creo i vettori di puntatori*/
	p->w = (cell_t **)malloc(p->nrow * sizeof(cell_t*));
	if(p->w == NULL)
		flag = 1;
	p->btime = (int **)malloc(p->nrow * sizeof(int*));
	if(p->btime == NULL)
		flag = 1;
	p->dtime = (int **)malloc(p->nrow * sizeof(int*));
	if(p->dtime == NULL)
		flag = 1;

	/*creo le righe*/
	for(i = 0; (i < p->nrow) && (flag == 0); i++){
		p->w[i] = (cell_t *)malloc(p->ncol * sizeof(cell_t));
		if(p->w[i] == NULL)
			flag = 1;
		p->btime[i] = (int *)malloc(p->ncol * sizeof(int));
		if(p->btime[i] == NULL)
			flag = 1;
		p->dtime[i] = (int *)malloc(p->ncol * sizeof(int));	
		if(p->dtime[i] == NULL)
			flag = 1;
	}

	/*inizializzo il pianeta*/
	for(i = 0; (i < p->nrow) && (flag == 0); i++){
		for(j = 0; j < p->ncol; j++){
			p->w[i][j] = WATER;
			p->btime[i][j] = 0;
			p->dtime[i][j] = 0;
		}
	}

	/*se c'è stato un errore nell'allocazione della memoria ripulisco lo heap dalle strutture già allocate*/
	if(flag != 0){
		p->nrow = i;
		free_planet(p);
		return NULL;
	}
	return p;
}

/** dealloca un pianeta (e tutta la matrice ...)
    \param p pianeta da deallocare

*/
void free_planet (planet_t* p){
	int i;
	if(p != NULL){
		/*dealloco le righe*/
		for(i = 0; i < p->nrow; i++){
			if(p->w != NULL && p->w[i] != NULL)
				free(p->w[i]);
			if(p->btime != NULL && p->btime[i] != NULL)
				free(p->btime[i]);
			if(p->dtime != NULL && p->dtime[i] != NULL)
				free(p->dtime[i]);
		}
		/*dealloco i vettori di puntatori*/
		if(p->w != NULL)
			free(p->w);
		if(p->btime != NULL)
			free(p->btime);
		if(p->dtime != NULL)
			free(p->dtime);

		/*dealloco la struttura*/
		free(p);
	}
}

/** stampa il pianeta su file secondo il formato di fig 2 delle specifiche, es

3
5
W F S W W
F S W W S
W W W W W

dove 3 e' il numero di righe (seguito da newline \n)
5 e' il numero di colonne (seguito da newline \n)
e i caratteri W/F/S indicano il contenuto (WATER/FISH/SHARK) separati da un carattere blank (' '). Ogni riga terminata da newline \n

    \param f file su cui stampare il pianeta (viene sovrascritto se esiste)
    \param p puntatore al pianeta da stampare

    \retval 0 se tutto e' andato bene
    \retval -1 se si e' verificato un errore (in questo caso errno e' settata opportunamente)

*/


int print_planet (FILE* f, planet_t* p){
	int i, j;
	if(p == NULL){
		errno = EINVAL;
		return -1;
	}
	
	if(f == NULL){
		errno = EINVAL;
		return -1;
	}
	
	/*stampo sul file il numero di riga e il numero di colonna*/
	fprintf(f, "%d\n", p->nrow);
	fprintf(f, "%d\n", p->ncol);
	
	/*stampo i caratteri*/
	for(i = 0; i < p->nrow; i++){
		for(j = 0; j < (p->ncol)-1; j++){
			fprintf(f, "%c ", cell_to_char(p->w[i][j]));
		}
		fprintf(f, "%c\n", cell_to_char(p->w[i][j]));
	}

	return 0;
}

/** inizializza il pianeta leggendo da file la configurazione iniziale

    \param f file da dove caricare il pianeta (deve essere gia' stato aperto in lettura)

    \retval p puntatore al nuovo pianeta (allocato dentro la funzione)
    \retval NULL se si e' verificato un errore (setta errno) 
            errno = ERANGE se il file e' mal formattato
 */
planet_t* load_planet (FILE* f){
	int nrow, ncol, i, j;
	char tmp, c;
	planet_t *p;

	if(f == NULL){
		errno = EINVAL;
		return NULL;
	}
	/*il numero di righe deve essere seguito da newline*/
	if((fscanf(f, "%d%c", &nrow,&tmp) != 2) || (tmp != '\n')){
		errno = ERANGE;
		return NULL;
	}
	/*il numero di colonne deve essere seguito da newline*/
	if((fscanf(f, "%d%c", &ncol,&tmp) != 2) || (tmp != '\n')){
		errno = ERANGE;
		return NULL;
	}

	p = new_planet(nrow, ncol);	

	if(p == NULL){
		errno = ENOMEM;
		return NULL;
	}
	
	for(i = 0; i < nrow; i++){
		for(j = 0; j < (ncol-1); j++){
			/*tra un carattere e l'altro ci deve essere uno spazio*/
			if((fscanf(f, "%c%c", &c,&tmp) != 2) || tmp != ' '){
				errno = ERANGE;				
				free_planet(p);
				return NULL;
			}
			/*il carattere deve essere W/F/S*/
			if((p->w[i][j] = char_to_cell(c)) == -1){
				errno = ERANGE;
				free_planet(p);
				return NULL;
			}			
		}
		/*alla fine di una riga deve esserci un newline*/
		if((fscanf(f, "%c%c", &c,&tmp) != 2) || tmp != '\n'){
			errno = ERANGE;
			free_planet(p);
			return NULL;
		}
		if((p->w[i][j] = char_to_cell(c)) == -1){
			errno = ERANGE;
			free_planet(p);
			return NULL;
		}			
		
		
	}
	/*il file deve essere terminato*/
	if(fgetc(f) != EOF){
		errno = ERANGE;
		free_planet(p);
		return NULL;
	}
	return p;
}

/** crea una nuova istanza della simulazione in base ai contenuti 
    del file di configurazione "wator.conf"

    \param fileplan nome del file da cui caricare il pianeta

    \retval p puntatore alla nuova struttura che descrive 
              i parametri di simulazione
    \retval NULL se si e' verificato un errore (setta errno)
*/
wator_t* new_wator (char* fileplan){
	wator_t *wat = NULL;
	FILE *f;
	int sd, sb, fb, num, i;
	char c1, c2, spazio, newline;
	
	f = fopen(CONFIGURATION_FILE, "r");
	if(f == NULL){
		errno = EINVAL;
		return NULL;
	}

	wat = (wator_t *)malloc(sizeof(wator_t));
	if(wat == NULL){
		errno = ENOMEM;
		return NULL;
	}

	/*controllo che il file wator.conf sia ben formattato*/
	for(i = 0; i < 3; i++){	
		if((fscanf(f, "%c%c%c%d%c", &c1, &c2, &spazio, &num, &newline) != 5) || (spazio != ' ') || (newline != '\n')){
			errno = ERANGE;
			free(wat);
			return NULL;
		}
		switch(c1){
			case 'f':  	if(c2 != 'b'){
						errno = ERANGE;
						free(wat);
						return NULL;
					}
					else
						fb = num;
					break;			

			case 's':	switch(c2){
						case 'd':	sd = num;
								break;
						
						case 'b':	sb = num;
								break;

						default:	errno = ERANGE;
								free(wat);
								return NULL;	
					}
					break;

			default:	errno = ERANGE;
					free(wat);
					return NULL;	
		}
	}
	
	if(fgetc(f) != EOF){
		errno = ERANGE;
		free(wat);
		return NULL;
	}
	fclose(f);

	wat->sd = sd;
	wat->sb = sb;
	wat->fb = fb;
	wat->nwork = 0;
	wat->chronon = 0;
	
	f = fopen(fileplan, "r");
	if(f == NULL){
		errno = EINVAL;
		free(wat);
		return NULL;
	}
	/*carico il pianeta dal file passato per parametro*/
	wat->plan = load_planet(f);
	fclose(f);

	if(wat->plan == NULL){
		free(wat);
		return NULL;
	}

	/*imposto il numero di pesci*/
	if((wat->nf = fish_count(wat->plan)) < 0){
		free_planet(wat->plan);
		free(wat);
		return NULL;
	}
	/*imposto il numero di squali*/
	if((wat->ns = shark_count(wat->plan)) < 0){
		free_planet(wat->plan);
		free(wat);
		return NULL;
	}

	return wat;
}


/** restituisce il numero di pesci nel pianeta
    \param p puntatore al pianeta
    
    \retval n (>=0) numero di pesci presenti
    \retval -1 se si e' verificato un errore (setta errno )
 */
int fish_count (planet_t* p){
	int nf = 0, i, j;

	if(p == NULL){
		errno = EINVAL;
		nf = -1;
	}
	else{
		/*conto il numero di pesci*/
		for(i = 0; i < p->nrow; i++)
			for(j = 0; j < p->ncol; j++)
				if(p->w[i][j] == FISH)
					nf++;
	}
	return nf;
}

/** restituisce il numero di squali nel pianeta
    \param p puntatore al pianeta
    
    \retval n (>=0) numero di squali presenti
    \retval -1 se si e' verificato un errore (setta errno )
 */
int shark_count (planet_t* p){
	int ns = 0, i, j;

	if(p == NULL){
		errno = EINVAL;
		ns = -1;
	}
	else{
		/*conto il numero di squali*/
		for(i = 0; i < p->nrow; i++)
			for(j = 0; j < p->ncol; j++)
				if(p->w[i][j] == SHARK)
					ns++;
	}
	return ns;
}

/** libera la memoria della struttura wator (e di tutte le sottostrutture)
    \param pw puntatore struttura da deallocare

 */
void free_wator(wator_t* pw){
	if(pw != NULL){
		free_planet(pw->plan);
		free(pw);
	}
}


/** Regola 1: gli squali mangiano e si spostano 
  \param pw puntatore alla struttura di simulazione
  \param (x,y) coordinate iniziali dello squalo
  \param (*k,*l) coordinate finali dello squalo (modificate in uscita)

  La funzione controlla i 4 vicini 
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle contiene un pesce, lo squalo mangia il pesce e 
  si sposta nella cella precedentemente occupata dal pesce. Se nessuna 
  delle celle adiacenti contiene un pesce, lo squalo si sposta
  in una delle celle adiacenti vuote. Se ci sono piu' celle vuote o piu' pesci 
  la scelta e' casuale.
  Se tutte le celle adiacenti sono occupate da squali 
  non possiamo ne mangiare ne spostarci lo squalo rimane fermo.

  NOTA: la situazione del pianeta viene 
        modificata dalla funzione con il nuovo stato

  \retval STOP se siamo rimasti fermi
  \retval EAT se lo squalo ha mangiato il pesce
  \retval MOVE se lo squalo si e' spostato solamente
  \retval -1 se si e' verificato un errore (setta errno)
*/

int shark_rule1 (wator_t* pw, int x, int y, int *k, int* l){
	int *adj = NULL;
	cell_t su, giu, dx, sx;
	int nfadj, nwadj, nsadj, nrow, ncol, i = 0, cont = 0, j = 0, stato;

	/*mi dice effettivamente quante sono le celle adiacenti sopra e sotto(nel caso in cui le righe sono < 3 la riga sopra e quella sotto sono la stessa)*/
	int ncellsg = 2;

	/*mi dice effettivamente quante sono le celle adiacenti a destra e a sinistra(nel caso in cui le colonne sono < 3 la colonna dx e quella sx sono la stessa)*/
	int ncellds = 2;
 	
	if(pw == NULL){
		errno = EINVAL;
		return -1;	
	}

	nrow = (pw->plan)->nrow;
	ncol = (pw->plan)->ncol;
	/*i parametri passati in ingresso devono essere giusti*/
	if((x >= nrow) || (x < 0) || (y >= ncol) || (y < 0) || ((pw->plan)->w[x][y] != SHARK) ){
		errno = EINVAL;
		return -1;
	} 	
	/*matrice 1x1*/
	if(((pw->plan)->nrow == 1) && ((pw->plan)->ncol == 1))
		return STOP;
	
	/*il risultato dell'operatore % sui numeri negativi non è come a matematica discreta, es: -1 mod 20 != 19, in questo modo evito di avere numeri negativi */
	if(x == 0)
		x = nrow;

	if(y == 0)
		y = ncol;

	/*le celle sopra e sotto sono le stesse*/
	if( (x-1) % nrow == (x+1) % nrow )
		ncellsg--;


	/*le celle a destra e a sinistra sono le stesse*/
	if( (y-1) % ncol == (y+1) % ncol )
		ncellds--;

	/*non ci sono celle sopra o sotto quella corrente*/
	if( (x-1) % nrow ==  x % nrow )
		ncellsg--;

	/*non ci sono celle a destra o a sinistra di quella corrente*/
	if( (y-1) % ncol == y % ncol )
		ncellds--;


	adj = (int *)malloc((ncellsg + ncellds)*sizeof(int));
	if(adj == NULL){
		free(adj);
		errno = ENOMEM;
		return -1;
	}

	su = (pw->plan)->w[(x-1) % nrow][y % ncol];
	giu = (pw->plan)->w[(x+1) % nrow][y % ncol];
	dx = (pw->plan)->w[x % nrow][(y+1) % ncol];
	sx = (pw->plan)->w[x % nrow][(y-1) % ncol];

	

	/*conto il numero di squali adiacenti allo squalo dato*/
	if( (nsadj = count_adj(nrow, ncol, x, y, su, giu, dx, sx, adj, SHARK)) < 0){
		free(adj);
		return -1;
	}

	

	/*se ho solo squali adiacenti resto fermo*/
	if(nsadj == (ncellsg + ncellds)){
			*k = x % nrow;
			*l = y % ncol;
			stato = STOP;
	}

	/*conto il numero di pesci accanto allo squalo*/
	if( (nfadj = count_adj(nrow, ncol, x, y, su, giu, dx, sx, adj, FISH)) < 0){
		free(adj);
		return -1;
	}

	/* ho almeno un pesce adiacente*/
	if(nfadj >= 1){
			/*calcolo un numero casuale compreso tra 1 e il numero di pesci*/
			j = 1 + genera()%nfadj;
			/*scelgo il pesce in base al numero casuale calcolato precedentemente*/
			while(cont != j){
				cont += adj[i];
				i++;			
			}
			i--;
			pw->nf--;
			stato = EAT;
	}

	/*conto il numero di acqua adiacenti allo squalo dato*/
	if( (nwadj = count_adj(nrow, ncol, x, y, su, giu, dx, sx, adj, WATER)) < 0){
		free(adj);
		return -1;
	}

	/*non ho pesci adiacenti ma ho almeno un water*/
	if(nfadj == 0  && nwadj >= 1){
		/*calcolo un numero casuale compreso tra 1 e il numero di acqua*/
		j = 1 + genera()%nwadj;
		/*scelgo una cella acqua in base al numero casuale calcolato precedentemente*/
		while(cont != j){
			cont += adj[i];
			i++;			
		}
		i--;
		stato = MOVE;
	}

	free(adj);
	
	if(stato != STOP){
	/*** effettuo gli spostamenti ***/	

		/* se ncellsg == 0 e ncellds == 1 allora adjf[0] indica la cella destra( =sinistra)*/
		if(ncellsg == 0 && ncellds == 1){		
				*k = x % nrow;
				*l = (y+1) % ncol;		
		}

		/* se ncellsg == 0 e ncellds == 2 allora adjf[0] indica la cella destra adjf[1] indica la cella a sinistra*/
		if(ncellsg == 0 && ncellds == 2){
			switch(i){
				case 0: *k = x % nrow;
					*l = (y+1) % ncol;
					break;
			
				case 1:	*k = x % nrow;
					*l = (y-1) % ncol;
					break;
			}
		}
	
		/* se ncellsg == 1 e ncellds == 0 allora adjf[0] indica la cella sopra( =sotto)*/
		if(ncellsg == 1 && ncellds == 0){	
			*k = (x-1) % nrow;
			*l = y % ncol;
		}
		
		
		/* se ncellsg == 2 e ncellds == 0 allora adjf[0] indica la cella sopra adjf[1] indica la cella sotto*/
		if(ncellsg == 2 && ncellds == 0){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;
			
				case 1: *k = (x+1) % nrow;
					*l = y % ncol;
					break;
			}
		}

		/* se ncellsg == 1 e ncellds == 1 allora adjf[0] indica la cella sopra( =sotto) e adjf[1] indica la cella destra( =sinistra)*/
		if(ncellsg == 1 && ncellds == 1 && stato){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;
			
				case 1: *k = x % nrow;
					*l = (y+1) % ncol;
					break;
			}
		}

	
		/* se ncellsg == 2 e ncellds == 1 allora adjf[0] indica la cella sopra adjf[1] indica la cella sotto adjf[2] indica la cella
		   a destra*/
		if(ncellsg == 2 && ncellds == 1 && stato){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;

				case 1:	*k = (x+1) % nrow;
					*l = y % ncol;
					break;

				case 2: *k = x % nrow;
					*l = (y+1) % ncol;
					break;
			}
		}


		/* se ncellsg == 1 e ncellds == 2 allora adjf[0] indica la cella sopra adjf[1] indica la cella destra adjf[2] indica la   			cella a sinisra*/
		if(ncellsg == 1 && ncellds == 2){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;

				case 1: *k = x % nrow;
					*l = (y+1) % ncol;
					break;

				case 2: *k = x % nrow;
					*l = (y-1) % ncol;
					break;
			}
		}

	
		/* se ncellsg == 2 e ncellds == 2 allora adjf[0] indica la cella sopra adjf[1] indica la cella sotto adjf[2] indica la cella
		   a destra e adjf[3] indica la cella a sinistra*/ 
		if(ncellsg == 2 && ncellds == 2 && stato){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;
				
				case 1: *k = (x+1) % nrow;
					*l = y % ncol;
					break;

				case 2: *k = x % nrow;
					*l = (y+1) % ncol;
					break;

				case 3: *k = x % nrow;
					*l = (y-1) % ncol;
					break;
			}
		}
		/*nella casella in cui mi sposto ci devo mettere uno squalo*/
		(pw->plan)->w[*k][*l] = SHARK;
		/*la casella dalla quale mi sono spostato deve diventare acqua*/
		(pw->plan)->w[x % nrow][y % ncol] = WATER;
		/*aggiorno btime e dtime*/
		(pw->plan)->btime[*k][*l] = (pw->plan)->btime[x % nrow][y %ncol];
		(pw->plan)->btime[x % nrow][y %ncol] = 0;
		(pw->plan)->dtime[*k][*l] = (pw->plan)->dtime[x % nrow][y %ncol];
		(pw->plan)->dtime[x % nrow][y %ncol] = 0;
		/*se lo squalo ha mangiato azzera il suo dtime*/		
		if(stato == EAT)
			(pw->plan)->dtime[*k][*l] = 0;
	}
	
	return stato;
}








/** calcola il numero di pesci/squali/acqua adiacenti ad una posizione data
   \param nrow numero righe matrice che rappresenta il pianeta
   \param ncol numero colonne matrice che rappresenta il pianeta
   \param x,y posizione corrente
   \param su, giu, dx, sx rispettivamente le celle sopra, sotto, a destra e a sinistra della posizione data
   \param adj array che memorizza la posizione dei pesci/squali/acqua adiacenti (se adj[i] = 1 : ho una cella del tipo dato)
   \param type il tipo delle celle che voglio sapere

   \retval n(>=0) numero di pesci adiacenti
   \retval -1 in caso di errore (setta errno)
 */ 		
int count_adj(int nrow, int ncol, int x, int y, cell_t su, cell_t giu, cell_t dx, cell_t sx, int* adj, cell_t type){
	int count = 0;

	if((x > nrow) || (x < 0) || (y > ncol) || (y < 0)){
		errno = EINVAL;
		return -1;
	} 

	/*le celle sono disposte in questo modo
			  *
			(x,y) *	
	*/
	if( ((x-1) % nrow == (x+1) % nrow ) && ((y-1) % ncol == (y+1) % ncol) ){
		if(su == type){
			count++;
			adj[0] = 1;
		}
		else
			adj[0] = 0;
		
		if(dx == type){
			count++;
			adj[1] = 1;
		}
		else
			adj[1] = 0;
	}

	/*le cell sono disposte in questo modo 
			    *
			* (x,y) *			
	*/
	if( ((x-1) % nrow ==  (x+1) % nrow ) && ((y-1) % ncol != (y+1) % ncol) ){
		if(su == type){
			count++;
			adj[0] = 1;
		}	
		else
			adj[0] = 0;
	
		if(dx == type){
			count++;
			adj[1] = 1;
		}
		else
			adj[1] = 0;

		if(sx == type){
			count++;
			adj[2] = 1;		
		}
		else
			adj[2] = 0;
	}

	/*le celle sono disposte in questo modo
			  *
			(x,y) *
			  *
	*/
	if( ((x-1) % nrow != (x+1) % nrow ) && ((y-1) % ncol == (y+1) % ncol) ){
		if(su == type){
			count++;
			adj[0] = 1;
		}
		else
			adj[0] = 0;
		
		if(giu == type){
			count++;
			adj[1] = 1;
		}
		else
			adj[1] = 0;

		if(sx == type){
			count++;
			adj[2] = 1;		
		}
		else
			adj[2] = 0;

	}

	/*le celle sono disposte in questo modo
			   *
		       * (x,y) *
			   *
	*/
	if( ((x-1) % nrow != (x+1) % nrow ) && ((y-1) % ncol != (y+1) % ncol) ){
		if(su == type){
			count++;
			adj[0] = 1;
		}
		else
			adj[0] = 0;
				
		if(giu == type){
			count++;
			adj[1] = 1;
		}
		else
			adj[1] = 0;

		if(dx == type){
			count++;
			adj[2] = 1;		
		}
		else
			adj[2] = 0;

		if(sx == type){
			count++;
			adj[3] = 1;		
		}
		else
			adj[3] = 0;
	}
	
	/*le celle sono disposte in questo modo
		       * (x,y) *
	*/
	if( ((x-1) % nrow == (x+1) % nrow ) && ((x-1) % nrow  ==  x % nrow ) && ((y-1) % ncol != (y+1) % ncol) ){
		if(dx == type){
			count++;
			adj[0] = 1;		
		}
		else
			adj[0] = 0;

		if(sx == type){
			count++;
			adj[1] = 1;		
		}
		else
			adj[1] = 0;

	}

	/*le celle sono disposte in questo modo
		        (x,y) *
	*/
	if( ((x-1) % nrow == (x+1) % nrow ) && ((x-1) % nrow == x % nrow ) && ((y-1) % ncol == (y+1) % ncol) ){
		if(dx == type){
			count++;
			adj[0] = 1;		
		}
		else
			adj[0] = 0;
	}

	/*le celle sono disposte in questo modo
  			  *		       
			(x,y)
  			  * 
	*/
	if( ((x-1) % nrow != (x+1) % nrow) && ((y-1) % ncol == (y+1) % ncol ) && ((y-1) % ncol == y % ncol) ){
		if(su == type){
			count++;
			adj[0] = 1;
		}
		else
			adj[0] = 0;
		
		if(giu == type){
			count++;
			adj[1] = 1;
		}
		else
			adj[1] = 0;
	}

	/*le celle sono disposte in questo modo
  			  *		       
			(x,y)
	*/
	if( ((x-1) % nrow == (x+1) % nrow ) && ((y-1) % ncol == (y+1) % ncol ) && ((y-1) % ncol == y % ncol) ){
		if(su == type){
			count++;
			adj[0] = 1;
		}
		else
			adj[0] = 0;		

	}

	return count;
}


/** Regola 2: gli squali si riproducono e muoiono
  \param pw puntatore alla struttura wator
  \param (x,y) coordinate dello squalo
  \param (*k,*l) coordinate dell'eventuale squalo figlio (

  La funzione calcola nascite e morti in base agli indicatori 
  btime(x,y) e dtime(x,y).

  == btime : nascite ===
  Se btime(x,y) e' minore di  pw->sb viene incrementato.
  Se btime(x,y) e' uguale a pw->sb si tenta di generare un nuovo squalo.
  Si considerano i 4 vicini 
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle e' vuota lo squalo figlio viene generato e la occupa, se le celle sono tutte occupate da pesci o squali la generazione non avviene. 
  In entrambi i casi btime(x,y) viene azzerato.

  == dtime : morte dello squalo  ===
  Se dtime(x,y) e' minore di pw->sd viene incrementato.
  Se dtime(x,y) e' uguale a pw->sd lo squalo muore e la sua posizione viene 
  occupata da acqua.

  NOTA: la situazione del pianeta viene 
        modificata dalla funzione con il nuovo stato


  \retval DIED se lo squalo e' morto
  \retval ALIVE se lo squalo e' vivo
  \retval -1 se si e' verificato un errore (setta errno)
*/
int shark_rule2 (wator_t* pw, int x, int y, int *k, int* l){
	int *adj = NULL;
	cell_t su, giu, dx, sx;
	int nwadj, nrow, ncol, i = 0, cont = 0, j = 0, stato = 0;

	/*mi dice effettivamente quante sono le celle adiacenti sopra e sotto*/
	int ncellsg = 2;

	/*mi dice effettivamente quante sono le celle adiacenti a destra e a sinistra*/
	int ncellds = 2;
 	
	if(pw == NULL){
		errno = EINVAL;
		return -1;	
	}

	nrow = (pw->plan)->nrow;
	ncol = (pw->plan)->ncol;
	/*i parametri passati in ingresso devono essere giusti*/
	if((x > nrow) || (x < 0) || (y > ncol) || (y < 0) || ((pw->plan)->w[x][y] != SHARK) ){
		errno = EINVAL;
		return -1;
	} 	

	/*non è giunta la fine dello squalo*/
	if((pw->plan)->dtime[x][y] < pw->sd){
		stato = ALIVE;
		(pw->plan)->dtime[x][y]++;
	}

	/*lo squalo muore*/
	else{
		pw->ns--;
		stato = DEAD;
		(pw->plan)->dtime[x][y] = 0;
		(pw->plan)->w[x][y] = WATER;
	}
	/*non è l'ora di riprodursi*/
	if((pw->plan)->btime[x][y] < pw->sb)
		(pw->plan)->btime[x][y]++;

	/*lo squalo si riproduce*/
	else{	
		(pw->plan)->btime[x][y] = 0;
		/*il risultato dell'operatore % sui numeri negativi non è come a matematica discreta, es: -1 mod 20 != 19, in questo modo evito di avere numeri negativi  */
		if(x == 0)
			x = nrow;

		if(y == 0)
			y = ncol;
		/*le celle sopra e sotto sono le stesse*/
		if( (x-1) % nrow == (x+1) % nrow )
			ncellsg--;


		/*le celle a destra e a sinistra sono le stesse*/
		if( (y-1) % ncol == (y+1) % ncol )
			ncellds--;

		/*non ci sono celle sopra o sotto quella corrente*/
		if( (x-1) % nrow ==  x % nrow )
			ncellsg--;

		/*non ci sono celle a destra o a sinistra di quella corrente*/
		if( (y-1) % ncol == y % ncol )
			ncellds--;
	
		su = (pw->plan)->w[(x-1) % nrow][y % ncol];
		giu = (pw->plan)->w[(x+1) % nrow][y % ncol];
		dx = (pw->plan)->w[x % nrow][(y+1) % ncol];
		sx = (pw->plan)->w[x % nrow][(y-1) % ncol];

		adj = (int *)malloc((ncellsg + ncellds)*sizeof(int));
		if(adj == NULL){
			free(adj);
			errno = ENOMEM;
			return -1;
		}
		
		/*conto il numero di acqua adiacenti allo squalo dato*/
		if( (nwadj = count_adj(nrow, ncol, x, y, su, giu, dx, sx, adj, WATER)) < 0){
			free(adj);
			return -1;
		}

		/*ho almeno un water*/
		if(nwadj >= 1){
			pw->ns++;
			/*numero casuale compreso tra 1 e il numero di acqua adiacente*/
			j = 1 + genera()%nwadj;
			/*scelgo la casella giusta in base al numero casuale calcolato precedentemente*/
			while(cont != j){
				cont += adj[i];
				i++;			
			}
			i--;

			/*** effettuo la riproduzione ***/	

			/* se ncellsg == 0 e ncellds == 1 allora adj[0] indica la cella destra( =sinistra)*/
			if(ncellsg == 0 && ncellds == 1){		
				*k = x % nrow;
				*l = (y+1) % ncol;		
			}

			/* se ncellsg == 0 e ncellds == 2 allora adj[0] indica la cella destra adj[1] indica la cella a sinistra*/
			if(ncellsg == 0 && ncellds == 2){
				switch(i){
					case 0: *k = x % nrow;
						*l = (y+1) % ncol;
						break;
			
					case 1: *k = x % nrow;
						*l = (y-1) % ncol;
						break;
				}
			}
	
			/* se ncellsg == 1 e ncellds == 0 allora adj[0] indica la cella sopra( =sotto)*/
			if(ncellsg == 1 && ncellds == 0){	
				*k = (x-1) % nrow;
				*l = y % ncol;
			}
		
		
			/* se ncellsg == 2 e ncellds == 0 allora adj[0] indica la cella sopra adj[1] indica la cella sotto*/
			if(ncellsg == 2 && ncellds == 0){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;
			
					case 1: *k = (x+1) % nrow;
						*l = y % ncol;
						break;
				}
			}

/* se ncellsg == 1 e ncellds == 1 allora adj[0] indica la cella sopra( =sotto) e adj[1] indica la cella destra  ( =sinistra)*/
			if(ncellsg == 1 && ncellds == 1 ){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;
			
					case 1: *k = x % nrow;
						*l = (y+1) % ncol;
						break;
				}
			}

	
/* se ncellsg == 2 e ncellds == 1 allora adj[0] indica la cella sopra adj[1] indica la cella sotto adj[2] indica la cella
			   a destra*/
			if(ncellsg == 2 && ncellds == 1){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;

					case 1: *k = (x+1) % nrow;
						*l = y % ncol;
						break;

					case 2: *k = x % nrow;
						*l = (y+1) % ncol;
						break;
				}
			}


/* se ncellsg == 1 e ncellds == 2 allora adj[0] indica la cella sopra adj[1] indica la cella destra adj[2] indica la cella
			   a sinistra*/
			if(ncellsg == 1 && ncellds == 2 ){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;

					case 1: *k = x % nrow;
						*l = (y+1) % ncol;
						break;

					case 2: *k = x % nrow;
						*l = (y-1) % ncol;
						break;
				}
			}

	
/* se ncellsg == 2 e ncellds == 2 allora adj[0] indica la cella sopra adj[1] indica la cella sotto adj[2] indica la cella
			   a destra e adj[3] indica la cella a sinistra*/ 
			if(ncellsg == 2 && ncellds == 2){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;
				
					case 1: *k = (x+1) % nrow;
						*l = y % ncol;
						break;

					case 2:	*k = x % nrow;
						*l = (y+1) % ncol;
						break;

					case 3: *k = x % nrow;
						*l = (y-1) % ncol;
						break;
				}
			}

		}
		/*la nuova cella deve contenere uno squalo*/
		/*non setto btime e dtime perchè una cella water avrà dtime=0 e btime=0, se un essere vivente si sposta infatti
btime e dtime vengono aggiornati correttamente nelle regole di spostamento*/
		(pw->plan)->w[*k][*l] = SHARK;

		free(adj);
	}
	
	/*se uno squalo è morto il btime deve essere azzerato, con questa implementazione se uno squalo si deve riprodurre ma ha raggiunto il momento di morire, prima si riproduce poi muore*/
	if(stato == DEAD)
		(pw->plan)->btime[x % nrow][y % ncol] = 0;
	return stato;
}


/** Regola 3: i pesci si spostano

    \param pw puntatore alla struttura di simulazione
    \param (x,y) coordinate iniziali del pesce
    \param (*k,*l) coordinate finali del pesce

    La funzione controlla i 4 vicini 
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

     un pesce si sposta casualmente in una delle celle adiacenti (libera). 
     Se ci sono piu' celle vuote la scelta e' casuale. 
     Se tutte le celle adiacenti sono occupate rimaniamo fermi.

     NOTA: la situazione del pianeta viene 
        modificata dalla funzione con il nuovo stato

  \retval STOP se siamo rimasti fermi
  \retval MOVE se il pesce si e' spostato
  \retval -1 se si e' verificato un errore (setta errno)
*/
int fish_rule3 (wator_t* pw, int x, int y, int *k, int* l){
	int *adj = NULL;
	cell_t su, giu, dx, sx;
	int nwadj, nrow, ncol, i = 0, cont = 0, j = 0, stato;

	/*mi dice effettivamente quante sono le celle adiacenti sopra e sotto*/
	int ncellsg = 2;

	/*mi dice effettivamente quante sono le celle adiacenti a destra e a sinistra*/
	int ncellds = 2;
 	
	if(pw == NULL){
		errno = EINVAL;
		return -1;	
	}

	nrow = (pw->plan)->nrow;
	ncol = (pw->plan)->ncol;
	/*i parametri passati in ingresso devono essere giusti*/
	if((x > nrow) || (x < 0) || (y > ncol) || (y < 0) || ((pw->plan)->w[x][y] != FISH) ){
		errno = EINVAL;
		return -1;
	} 	
	/*matrice 1x1*/
	if(((pw->plan)->nrow == 1) && ((pw->plan)->ncol == 1))
		return STOP;
	/*il risultato dell'operatore % sui numeri negativi non è come a matematica discreta, es: -1 mod 20 != 19, in questo modo evito di avere numeri negativi  */
	if(x == 0)
		x = nrow;

	if(y == 0)
		y = ncol;
	/*le celle sopra e sotto sono le stesse*/
	if( (x-1) % nrow == (x+1) % nrow )
		ncellsg--;


	/*le celle a destra e a sinistra sono le stesse*/
	if( (y-1) % ncol == (y+1) % ncol )
		ncellds--;

	/*non ci sono celle sopra o sotto quella corrente*/
	if( (x-1) % nrow ==  x % nrow )
		ncellsg--;

	/*non ci sono celle a destra o a sinistra di quella corrente*/
	if( (y-1) % ncol == y % ncol )
		ncellds--;


	adj = (int *)malloc((ncellsg + ncellds)*sizeof(int));
	if(adj == NULL){
		free(adj);
		errno = ENOMEM;
		return -1;
	}

	su = (pw->plan)->w[(x-1) % nrow][y % ncol];
	giu = (pw->plan)->w[(x+1) % nrow][y % ncol];
	dx = (pw->plan)->w[x % nrow][(y+1) % ncol];
	sx = (pw->plan)->w[x % nrow][(y-1) % ncol];


	/*conto il numero di acqua adiacenti al pesce dato*/
	if( (nwadj = count_adj(nrow, ncol, x, y, su, giu, dx, sx, adj, WATER)) < 0){
		free(adj);
		return -1;
	}

	/*ho almeno un water*/
	if(nwadj >= 1){
		/*numero casuale compreso tra 1 e il numero di acqua adiacente*/
		j = 1 + genera()%nwadj;
		/*scelgo la casella giusta in base al numero casuale calcolato precedentemente*/
		while(cont != j){
			cont += adj[i];
			i++;			
		}
		i--;
		stato = MOVE;
	}
	/*non ci sono celle disponibili in cui spostarsi*/
	else 
		stato = STOP;

	free(adj);
	
	if(stato != STOP){
		/*** effettuo gli spostamenti ***/	

		/* se ncellsg == 0 e ncellds == 1 allora adj[0] indica la cella destra( =sinistra)*/
		if(ncellsg == 0 && ncellds == 1){		
				*k = x % nrow;
				*l = (y+1) % ncol;		
		}

		/* se ncellsg == 0 e ncellds == 2 allora adj[0] indica la cella destra adj[1] indica la cella a sinistra*/
		if(ncellsg == 0 && ncellds == 2){
			switch(i){
				case 0: *k = x % nrow;
					*l = (y+1) % ncol;
					break;
			
				case 1: *k = x % nrow;
					*l = (y-1) % ncol;
					break;
			}
		}
	
		/* se ncellsg == 1 e ncellds == 0 allora adj[0] indica la cella sopra( =sotto)*/
		if(ncellsg == 1 && ncellds == 0){	
			*k = (x-1) % nrow;
			*l = y % ncol;
		}
		
		
		/* se ncellsg == 2 e ncellds == 0 allora adj[0] indica la cella sopra adj[1] indica la cella sotto*/
		if(ncellsg == 2 && ncellds == 0){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;
			
				case 1: *k = (x+1) % nrow;
					*l = y % ncol;
					break;
			}
		}

		/* se ncellsg == 1 e ncellds == 1 allora adj[0] indica la cella sopra( =sotto) e adj[1] indica la cella destra( =sinistra)*/
		if(ncellsg == 1 && ncellds == 1){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;
			
				case 1:	*k = x % nrow;
					*l = (y+1) % ncol;
					break;
			}
		}

	
		/* se ncellsg == 2 e ncellds == 1 allora adj[0] indica la cella sopra adj[1] indica la cella sotto adj[2] indica la cella
		   a destra*/
		if(ncellsg == 2 && ncellds == 1){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;

				case 1: *k = (x+1) % nrow;
					*l = y % ncol;
					break;

				case 2: *k = x % nrow;
					*l = (y+1) % ncol;
					break;
			}
		}


		/* se ncellsg == 1 e ncellds == 2 allora adj[0] indica la cella sopra adj[1] indica la cella destra adj[2] indica la cella
		   a sinisra*/
		if(ncellsg == 1 && ncellds == 2){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;

				case 1: *k = x % nrow;
					*l = (y+1) % ncol;
					break;

				case 2: *k = x % nrow;
					*l = (y-1) % ncol;
					break;
			}
		}

	
		/* se ncellsg == 2 e ncellds == 2 allora adjf[0] indica la cella sopra adj[1] indica la cella sotto adj[2] indica la cella
		   a destra e adj[3] indica la cella a sinistra*/ 
		if(ncellsg == 2 && ncellds == 2){
			switch(i){
				case 0: *k = (x-1) % nrow;
					*l = y % ncol;
					break;
				
				case 1: *k = (x+1) % nrow;
					*l = y % ncol;
					break;

				case 2: *k = x % nrow;
					*l = (y+1) % ncol;
					break;

				case 3: *k = x % nrow;
					*l = (y-1) % ncol;
					break;
			}
		}
		/*la casella nuova deve contenere un pesce*/
		(pw->plan)->w[*k][*l] = FISH;
		/*quella vecchia un water*/
		(pw->plan)->w[x % nrow][y % ncol] = WATER;
		/*aggiorno btime e dtime*/
		(pw->plan)->btime[*k][*l] = (pw->plan)->btime[x % nrow][y %ncol];
		(pw->plan)->btime[x % nrow][y %ncol] = 0;
		(pw->plan)->dtime[*k][*l] = (pw->plan)->dtime[x % nrow][y %ncol];
		(pw->plan)->dtime[x % nrow][y %ncol] = 0;
	}
	return stato;
}

/** Regola 4: i pesci si riproducono
  \param pw puntatore alla struttura wator
  \param (x,y) coordinate del pesce
  \param (*k,*l) coordinate dell'eventuale pesce figlio 

  La funzione calcola nascite in base a btime(x,y) 

  Se btime(x,y) e' minore di  pw->sb viene incrementato.
  Se btime(x,y) e' uguale a pw->sb si tenta di generare un nuovo pesce.
  Si considerano i 4 vicini 
              (x-1,y)
        (x,y-1) *** (x,y+1)
              (x+1,y)

  Se una di queste celle e' vuota il pesce figlio viene generato e la occupa, se le celle sono tutte occupate da pesci o squali la generazione non avviene. 
  In entrambi i casi btime(x,y) viene azzerato.

  NOTA: la situazione del pianeta viene 
        modificata dalla funzione con il nuovo stato


  \retval  0 se tutto e' andato bene
  \retval -1 se si e' verificato un errore (setta errno)
*/
int fish_rule4 (wator_t* pw, int x, int y, int *k, int* l){
	int *adj = NULL;
	cell_t su, giu, dx, sx;
	int nwadj, nrow, ncol, i = 0, cont = 0, j = 0;

	/*mi dice effettivamente quante sono le celle adiacenti sopra e sotto*/
	int ncellsg = 2;

	/*mi dice effettivamente quante sono le celle adiacenti a destra e a sinistra*/
	int ncellds = 2;
 	
	if(pw == NULL){
		errno = EINVAL;
		return -1;	
	}

	nrow = (pw->plan)->nrow;
	ncol = (pw->plan)->ncol;
	/*i parametri passati in ingresso devono essere giusti*/
	if((x > nrow) || (x < 0) || (y > ncol) || (y < 0) || ((pw->plan)->w[x][y] != FISH) ){
		errno = EINVAL;
		return -1;
	} 	


	/*non è il momento di riprodursi*/
	if((pw->plan)->btime[x][y] < pw->fb)
		(pw->plan)->btime[x][y]++;
	/*è il momento di riprodursi*/
	else{	
		(pw->plan)->btime[x][y] = 0;
		if(x == 0)
			x = nrow;

		if(y == 0)
			y = ncol;
		/*le celle sopra e sotto sono le stesse*/
		if( (x-1) % nrow == (x+1) % nrow )
			ncellsg--;


		/*le celle a destra e a sinistra sono le stesse*/
		if( (y-1) % ncol == (y+1) % ncol )
			ncellds--;

		/*non ci sono celle sopra o sotto quella corrente*/
		if( (x-1) % nrow ==  x % nrow )
			ncellsg--;

		/*non ci sono celle a destra o a sinistra di quella corrente*/
		if( (y-1) % ncol == y % ncol )
			ncellds--;
	
		su = (pw->plan)->w[(x-1) % nrow][y % ncol];
		giu = (pw->plan)->w[(x+1) % nrow][y % ncol];
		dx = (pw->plan)->w[x % nrow][(y+1) % ncol];
		sx = (pw->plan)->w[x % nrow][(y-1) % ncol];

		adj = (int *)malloc((ncellsg + ncellds)*sizeof(int));
		if(adj == NULL){
			free(adj);
			errno = ENOMEM;
			return -1;
		}
		
		/*conto il numero di acqua adiacenti allo squalo dato*/
		if( (nwadj = count_adj(nrow, ncol, x, y, su, giu, dx, sx, adj, WATER)) < 0){
			free(adj);
			return -1;
		}

		/*ho almeno un water*/
		if(nwadj >= 1){
			pw->nf++;
			/*calcolo un numero casuale compreso tra 1 e numero di acqua adiacente*/
			j = 1 + genera()%nwadj;
			/*scelgo la casella giusta in base al numero calcolato precedentemente*/
			while(cont != j){
				cont += adj[i];
				i++;			
			}
			i--;

			/*** effettuo la riproduzione ***/	

			/* se ncellsg == 0 e ncellds == 1 allora adj[0] indica la cella destra( =sinistra)*/
			if(ncellsg == 0 && ncellds == 1){		
				*k = x % nrow;
				*l = (y+1) % ncol;		
			}

			/* se ncellsg == 0 e ncellds == 2 allora adj[0] indica la cella destra adj[1] indica la cella a sinistra*/
			if(ncellsg == 0 && ncellds == 2){
				switch(i){
					case 0: *k = x % nrow;
						*l = (y+1) % ncol;
						break;
			
					case 1: *k = x % nrow;
						*l = (y-1) % ncol;
						break;
				}
			}
	
			/* se ncellsg == 1 e ncellds == 0 allora adj[0] indica la cella sopra( =sotto)*/
			if(ncellsg == 1 && ncellds == 0){	
				*k = (x-1) % nrow;
				*l = y % ncol;
			}
		
		
			/* se ncellsg == 2 e ncellds == 0 allora adj[0] indica la cella sopra adj[1] indica la cella sotto*/
			if(ncellsg == 2 && ncellds == 0){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;
			
					case 1: *k = (x+1) % nrow;
						*l = y % ncol;
						break;
				}
			}

/* se ncellsg == 1 e ncellds == 1 allora adj[0] indica la cella sopra( =sotto) e adj[1] indica la cella destra  ( =sinistra)*/
			if(ncellsg == 1 && ncellds == 1 ){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;
			
					case 1: *k = x % nrow;
						*l = (y+1) % ncol;
						break;
				}
			}

	
/* se ncellsg == 2 e ncellds == 1 allora adjf[0] indica la cella sopra adj[1] indica la cella sotto adj[2] indica la cella
			   a destra*/
			if(ncellsg == 2 && ncellds == 1){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;

					case 1: *k = (x+1) % nrow;
						*l = y % ncol;
						break;

					case 2: *k = x % nrow;
						*l = (y+1) % ncol;
						break;
				}
			}


/* se ncellsg == 1 e ncellds == 2 allora adjf[0] indica la cella sopra adj[1] indica la cella destra adj[2] indica la cella
			   a sinisra*/
			if(ncellsg == 1 && ncellds == 2 ){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;

					case 1: *k = x % nrow;
						*l = (y+1) % ncol;
						break;

					case 2: *k = x % nrow;
						*l = (y-1) % ncol;
						break;
				}
			}

	
/* se ncellsg == 2 e ncellds == 2 allora adj[0] indica la cella sopra adj[1] indica la cella sotto adj[2] indica la cella
			   a destra e adj[3] indica la cella a sinistra*/ 
			if(ncellsg == 2 && ncellds == 2){
				switch(i){
					case 0: *k = (x-1) % nrow;
						*l = y % ncol;
						break;
				
					case 1: *k = (x+1) % nrow;
						*l = y % ncol;
						break;

					case 2: *k = x % nrow;
						*l = (y+1) % ncol;
						break;

					case 3: *k = x % nrow;
						*l = (y-1) % ncol;
						break;
				}
			}

		}
		(pw->plan)->w[*k][*l] = FISH;

		free(adj);
	}
	return 0;
}

/** calcola un chronon aggiornando tutti i valori della simulazione e il pianeta
   \param pw puntatore al pianeta

   \return 0 se tutto e' andato bene
   \return -1 se si e' verificato un errore (setta errno)

 */
int update_wator (wator_t * pw){
	int i,j;
	int k = 0, l = 0;

	/*è una matrice che indica se nel chronon attuale sono finito nella pos (i,j) applicando una regola
	1: applicata 
	0: non applicata*/
	int **azione;

	if(pw == NULL){
		errno = EINVAL;
		return -1;
	}
	
	azione = (int **)malloc((pw->plan)->nrow * sizeof(int*));

	if(azione == NULL){
		errno = ENOMEM;
		return -1;
	}
	/*alloc la matrice azione*/
	for(i = 0; i < (pw->plan)->nrow; i++){
		azione[i] = (int *)calloc((pw->plan)->ncol, sizeof(int));
		/*in caso di errore devo pulire lo heap*/
		if(azione[i] == NULL){
			libera(azione, i);
			errno = ENOMEM;
			return -1;
		}
	}
	/*applico prima le regole di riproduzione*/
	for(i = 0; i < (pw->plan)->nrow; i++){
		for(j = 0; j < (pw->plan)->ncol; j++){
			k = i;
			l = j;
			switch((pw->plan)->w[i][j]){
				case SHARK:	if(!azione[i][j])
							if(shark_rule2(pw, i, j, &k, &l) == -1)
								return -1;
						break;
				case FISH:	if(!azione[i][j])
							if(fish_rule4(pw, i, j, &k, &l) == -1)
								return -1;
						break;
				case WATER:	break;
				}
			/*si è riprodotto*/
			if(k != i || l != j)
				azione[k][l] = 1;
		}
	}

	/*applico le regole di spostamento*/
	for(i = 0; i < (pw->plan)->nrow; i++){
		for(j = 0; j < (pw->plan)->ncol; j++){
			k = i;
			l = j;
			switch((pw->plan)->w[i][j]){
				case SHARK:	if(!azione[i][j])
							if(shark_rule1(pw, i, j, &k, &l) == -1)
								return -1;
						break;
				case FISH:	if(!azione[i][j])
							if(fish_rule3(pw, i, j, &k, &l) == -1)
								return -1;
						break;
				case WATER:  	break;
			}
			/*si è mosso*/
			if(k != i || l != j)
				azione[k][l] = 1;
		}
	}
	libera(azione, (pw->plan)->nrow);
	pw->chronon++;
	return 0;
}


void libera (int** m, int row){
		int i;

	for(i = 0; i < row; i++)
		free(m[i]);
	free(m);
}


/** imposta il numero di worker al valore specificato
    \param pw il wator da modificare
    \param nwork il valore da impostare

    \retval puntatore al nuovo wator modificato	, null se si è verificato errore (setta errno)
*/
wator_t* set_nwork(wator_t *pw, int nwork){
	if(pw == NULL){
		errno = EINVAL;
		return NULL;
	}

	if(nwork < 1){
		errno = EINVAL;
		return NULL;
	}

	pw->nwork = nwork;
	return pw;
}

/*genera un numero casuale*/
int genera(void){
	unsigned int seed;
	seed = (unsigned)time(NULL);
	srand(seed);
	return rand_r(&seed);
}




