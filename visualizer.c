/** \file visualizer.c
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
#include "processo_wator.h"
#include "wator.h"

volatile sig_atomic_t fine = 0;

/*utilizzata solo per sbloccare la accept*/
static void gestoreSIGUSR2 (int signum){
	write(2, "ciao dal segnale SIGUSR2\n", 26);
}

/*quando arriva il segnale SIGINT imposto il flag fine ad 1(così i thread capiscono che devono terminare)*/
void gestoreSIGINT(){
	fprintf(stderr, "SIGINT ARRIVATO\n");
	fine = 1;
	kill(getppid(), SIGINT);
}

/*quando arriva il segnale SIGTERM imposto il flag fine ad 1(così i thread capiscono che devono terminare)*/
void gestoreSIGTERM(){
	fprintf(stderr, "SIGTERM ARRIVATO\n");
	fine = 1;
	kill(getppid(), SIGTERM);
	
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
		perror("sigaddset SIGTERM");
		pthread_exit((void*)EXIT_FAILURE);
	}

	if(sigaddset(&set, SIGUSR2) == -1){
		perror("sigaddset SIGUSR2");
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

		switch(sig){
			case SIGINT : 	gestoreSIGINT();
					break;
			
			case SIGTERM: 	gestoreSIGTERM();
					break;
		}
		
	}
	pthread_exit(0);
		
}

/*il processo visualizer riceve come argomenti il dumpfile, il numero di righe del pianeta e il numero di colonne*/
int main(int argc, char *argv[]){
	int fd_skt, nrow, ncol, fd_conn, status, err, j, k;
	planet_t *plan;
	struct sockaddr_un sa;
	pthread_t tid;
	char cella, flag = 'O';
	struct sigaction s;
	FILE *f;
	sigset_t set;
	
	/*se il numero di argomenti è errato fallisco e dico al processo wator di terminare*/
	if(argc != 4){
		fprintf(stderr, "numero argomenti di visualizer errato\n");
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}

	/*il segnale SIGUSR2 è utilizzato per sbloccare la accept*/
	memset( &s, 0, sizeof(s) );
	s.sa_handler = gestoreSIGUSR2;  
	if(sigaction(SIGUSR2,&s,NULL) == -1){
		perror("sigaction");
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}

	/*maschero i segnali fino alla creazione del thread handler che ha il compito di gestirli*/
	if(sigaddset(&set, SIGINT) == -1){
		perror("sigaddset SIGINT");
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}

	if(sigaddset(&set, SIGTERM) == -1){
		perror("sigaddset SIGTERM");
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}

	if(sigaddset(&set, SIGUSR1) == -1){
		perror("sigaddset SIGUSR1");
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}


	if((err = pthread_sigmask(SIG_SETMASK,&set,NULL)) != 0){
		fprintf(stderr, "errore mascheramento %d\n", err);
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}

	/*creo l handler*/
	if((err = pthread_create(&tid, NULL, &handler, NULL)) != 0){
			fprintf(stderr, "errore nella creazione del handler\n");
			kill(getppid(), 2);
			exit(EXIT_FAILURE);
	}

	/*inizializzo la struttura per la socket*/
	sa.sun_family = DOMAIN;
	snprintf(sa.sun_path, UNIX_PATH_MAX, SOCK_NAME);


	/*creo il socket*/
	if((fd_skt = socket(DOMAIN, TYPE, 0)) == -1){
		perror("errore in creazione socket nel server");
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}

	/*associo un nome al socket*/
	if(bind(fd_skt,(struct sockaddr *)&sa,sizeof(sa)) == -1){
		perror("errore in bind nel server");
		close(fd_skt);
		unlink(SOCK_NAME);
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}	

	/*sono predisposto ad accettare connessioni*/
	if(listen(fd_skt, SOMAXCONN) == -1){
		perror("errore in listen nel server");
		close(fd_skt);
		unlink(SOCK_NAME);
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}

	/*inizializzo il pianeta*/
	nrow = atoi(argv[2]);
	ncol = atoi(argv[3]);
	fprintf(stderr, "inizializzo il pianeta\n");
	if((plan = new_planet(nrow, ncol)) == NULL){
		fprintf(stderr,"errore in new planet nel visualizer\n");
		close(fd_skt);
		unlink(SOCK_NAME);
		kill(getppid(), 2);
		exit(EXIT_FAILURE);
	}

	/*leggo la socket fino a che non arriva un segnale di terminazione*/
	while(flag == 'O'){
		if((fd_conn = accept(fd_skt, NULL, 0)) == -1){
			if(errno == EINTR){
				printf("sblocco accept\n");
				continue;
			}
			perror("errore in accept nel visualizer");
			close(fd_conn);
			close(fd_skt);
			unlink(SOCK_NAME);
			free_planet(plan);
			kill(getppid(), 2);
			exit(EXIT_FAILURE);
		}
		fprintf(stderr, "connessione accettata\n");
	
		if(read(fd_conn, &flag, sizeof(char)) != sizeof(flag)){
			perror("errore lettura");
			close(fd_skt);
			kill(getppid(), 2);
			free_planet(plan);
			unlink(SOCK_NAME);
			exit(EXIT_FAILURE);
		}

		if(flag == 'O'){
			/*leggo le caselle del pianeta e le inserisco nella matrice dopo averle riconvertite in cell_t*/
			for(k = 0; k < nrow; k++)
				for(j = 0; j < ncol; j++){
					if(read(fd_conn, &cella, sizeof(char)) != sizeof(cella)){
						if(errno == EINTR){
							j--;
							continue;
						}
						else{
							perror("errore lettura");
							close(fd_skt);
							kill(getppid(), 2);
							free_planet(plan);
							unlink(SOCK_NAME);
							exit(EXIT_FAILURE);
						}
					}
					plan->w[k][j] = char_to_cell(cella);
				}
			sleep(1);

			if(strcmp(argv[1], "stdout") != 0){
				if((f = fopen(argv[1], "w")) == NULL){
					perror("apertura file in visualizer");
					close(fd_conn);
					continue;
				} 
			
				if(print_planet(f, plan) == -1){
					perror("in print_planet visualizer");
				}
				fclose(f);
			}
			else
				if(print_planet(stdout, plan) == -1){
					perror("in print_planet visualizer");
				}
		}
		close(fd_conn);
		
	}
	fprintf(stderr, "attesa handler\n");
	/*attendo terminazione handler*/
	if((err = pthread_join(tid, (void *)&status)) != 0){
		fprintf(stderr, "errore nella join\n");
		free_planet(plan);
		unlink(SOCK_NAME);
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "terminazione gentile visualizer\n");
	free_planet(plan);
	close(fd_skt);
	unlink(SOCK_NAME);
	kill(getppid(), SIGTERM);
	return 0;
}
