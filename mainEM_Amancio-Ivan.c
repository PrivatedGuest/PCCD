#include <sys/types.h>// este e os 2 seguintes para o tema de mensaxes
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h> //para os prints
#include <stdlib.h>
#include <unistd.h> //para os fork
#include <pthread.h> //para os threads
#include <semaphore.h> //para os semaforos
#include <string.h> //para os strcopy

typedef struct mensaxe {
  long mtype; //para filtrar o mensaxe
  int prio;
	int clk;
  int id_nodo;
} mensaxe;

//prototipos das funcións
int sendMsg();
mensaxe receiveMsg();
void *procesoReceptor();
void crearVector();
void *fillo(void *args);
void menu();

int N = 3; //Numero de nodos, poñoo en int para poder modificalo por parametros

#define REPLY -1
#define REQUEST 1

//Identificadores
int mi_id = 1;
int mi_prio = 0;

//Colas de mensaxes
int id_cola = 0;
int id_cola_ack = 0;

//Varibles que nos fan falla
int quero = 0;
int sc = 0;
int stop = 0;

//Semaforos de proteccion destas variables
sem_t sem_prot_quero;
sem_t sem_prot_sc;
sem_t sem_prot_stop;

//Array de nodos
int *id_nodos;
int *id_nodos_pend;

int num_pend = 0;//nn e un array pero e sobre os nodos pendientes

//Semaforos de paso
sem_t sem_paso_fillo;
sem_t sem_paso_pai;
sem_t sem_paso_lectores;

sem_t sem_paso_simu;

//Semáforos de paso por tipo de proceso
sem_t sem_anulacions;
sem_t sem_pagos;
sem_t sem_reservas;
sem_t sem_lectores;
//Contadores
int num_fillos = 0;
int num_fillos_pend = 0;
int num_fillos_atend = 0;
int primeiro_paso = 1;
//Contadores de procesos
int n_anulacions = 0;
int n_pagos = 0;
int n_reservas = 0;
int n_lectores = 0;
//Proteción de contadores
sem_t sem_n_anulacions;
sem_t sem_n_pagos;
sem_t sem_n_reservas;
sem_t sem_n_lectores;

//Petición de entrada / Aviso de saida
sem_t sem_entrada;
sem_t sem_salida;

int nodo_prio = 0;//este fainos falla para deterinar a donde cedemos a em

//"RELOJ GLOBAL"
int clk = 0;
int mi_clk = 0;

//------------------------------- INICIO DO MAIN ------------------------------------
//meto por parametros id nºnodos prio nºfillos
int main (int argc, char const *argv[]){
  if (argc == 3){
    mi_id = atoi(argv[1]);
    N = atoi(argv[2]);
  }else{
    printf("Indroduzca: ID Nº_NODOS .\n");
    exit(0);
  }
  int id_nodos_aux[N-1];
  id_nodos = id_nodos_aux;
  int id_nodos_pend_aux[N-1];
  id_nodos_pend = id_nodos_pend_aux;

  pthread_t fioReceptor;
  pthread_t fioFillo[1000];

  id_cola = 		msgget(ftok("/tmp" , 123 ), 0777 | IPC_CREAT);
	id_cola_ack = msgget(ftok("/tmp" , 1234), 0777 | IPC_CREAT);

  printf("ID da cola de peticions: %i\n", id_cola);
	printf("ID da cola de acks: %i\n", id_cola_ack);

  //semaforos de paso
  sem_init(&sem_paso_fillo,0,0);
  sem_init(&sem_paso_pai,0,0);
  sem_init(&sem_paso_lectores,0,0);

	//Semáforos de paso por tipo de proceso
	sem_init(&sem_anulacions,0,0);
	sem_init(&sem_pagos,0,0);
	sem_init(&sem_reservas,0,0);
	sem_init(&sem_lectores,0,0);

	sem_init(&sem_entrada,0,0);
	sem_init(&sem_salida,0,0);

  //Semaforos para as variables
  sem_init(&sem_prot_quero,0,1);
  sem_init(&sem_prot_sc,0,1);
  sem_init(&sem_prot_stop,0,1);

	sem_init(&sem_n_anulacions,0,1);
	sem_init(&sem_n_pagos,0,1);
	sem_init(&sem_n_reservas,0,1);
	sem_init(&sem_n_lectores,0,1);
  //creamos o vector cos IDc dos veciños
  crearVector();

  //Facemos un thread para o proceso receptor
  pthread_create(&fioReceptor,NULL,procesoReceptor,"");


  menu();


  while (1){

    if(n_anulacions > 0) {
       mi_prio = 1;
      //sem_post(&sem_anulacions);
    } else if(n_pagos > 0) {
       mi_prio = 2;
      //sem_post(&sem_pagos);
    } else if(n_reservas > 0) {
        mi_prio = 3;
      //sem_post(&sem_reservas);
    } else if (n_lectores > 0){
        mi_prio = 4;
    }

    //Se nn e a primeira vez, pulsamos unha tecla para crear o fillo
    //if (!quero && !primeiro_paso) {

      /* printf("Esperando os novos fillos. \n");
      getchar(); // solo vale para volver a intentar ejecutar outra vez todos os procesos fillo que xa fixemos antes

      for(int i = 0; i < num_fillos; i++){
	pthread_create(&fioFillo[i], NULL, fillo, "");
      }*/

      //--------------------------------------_MENUUU---------------------------------------------
      //menu();

      // }else {
      //primeiro_paso = 0;
      //}
		sem_wait(&sem_entrada);
    sem_wait(&sem_prot_quero);
    quero = 1;
    sem_post(&sem_prot_quero);

		mi_clk = clk + 1;
    for (int i = 0; i < N-1; i++){
      sendMsg(REQUEST, id_nodos[i]);
    }
    for (int i = 0; i < N-1; i++){
      receiveMsg(id_cola_ack);
    }

    if(mi_prio == 4) {
      for (int i = 0; i < num_pend; i++) {
          sendMsg(REPLY, id_nodos_pend[i]);
          num_pend = 0;
      }
    }

    sem_wait(&sem_prot_sc);
    sc = 1;
    sem_post(&sem_prot_sc);

    //--------------------------------- INICIO DA SC -----------------------------

    printf("O nodo %i gana a 'exclusion mutua' entre nodos\n", mi_id);
		printf("Buscado prioritario entre %i, %i, %i\n", n_anulacions, n_pagos, n_reservas);
				 if(n_anulacions > 0) {
						printf("Avisado anulacions\n");
					 sem_post(&sem_anulacions);
				 } else if(n_pagos > 0) {
						printf("Avisado pagos\n");
					 sem_post(&sem_pagos);
				 } else if(n_reservas > 0) {
						 printf("Avisado reservas\n");
					 sem_post(&sem_reservas);
				 }else if (n_lectores > 0){
           printf("LECTORES\n");
           sem_post(&sem_lectores);
         }

		printf("Aviso para continuar\n");
		sem_wait(&sem_salida);

    /*num_fillos_pend = num_fillos - num_fillos_atend;

    for (int i = 0; i < num_fillos_pend; i++){
      sem_post(&sem_paso_fillo);
      num_fillos_atend ++;

      //para escritores
      if (mi_prio != 4){
	sem_wait(&sem_paso_pai);
      }

      sem_wait(&sem_prot_stop);
      if(stop == 1){
	sem_post(&sem_prot_stop);
	printf("Hai unha petición mais prioritara e deixo de dar paso aos procesos de prio menor.\n");
	sendMsg(REPLY, nodo_prio);
	break;
      }

      sem_post(&sem_prot_stop);
    }

    //para lectores
    if(mi_prio == 4){
      for (int i = 0; i < num_fillos; i++){
	printf("Esperando o lector %i\n" , i);
	sem_wait(&sem_paso_lectores);
	stop = 0;
      }
    }

    if (stop == 1){
      primeiro_paso = 1;
      stop = 0;
      continue;
    }
		*/
    // ---------------------------------SALIMOS DA SC -----------------------------------

    sem_wait(&sem_prot_sc);
    sc = 0;
    sem_post(&sem_prot_sc);

    sem_wait(&sem_prot_quero);
    quero = 0;
    sem_post(&sem_prot_quero);

    printf("Numero de nodos pendentes: %i\n",num_pend);
    for(int i = 0; i < num_pend; i++){
      sendMsg(REPLY, id_nodos_pend[i]);
    }

    num_pend = 0;
    num_fillos_atend = 0;

  }//--------cerra while--------


}

// -------------------------------- PROCESO RECEPTOR --------------------------------

void *procesoReceptor(){
  while (1){
    int id_nodo_orixe, prio_orixe, clk_orixe;

    mensaxe msg = receiveMsg(id_cola);
		//printf("1\n");
    id_nodo_orixe = msg.id_nodo;
    prio_orixe = msg.prio;
		clk_orixe = msg.clk;

			if(clk_orixe > clk) clk = clk_orixe;
			//printf("2\n");
			if (quero == 0 || /*(prio_orixe < mi_prio && sc != 1) ||*/ clk_orixe < mi_clk ||
					 /*(prio_orixe == mi_prio &&*/ (id_nodo_orixe < mi_id && sc != 1) ){
      sendMsg(REPLY, id_nodo_orixe);
    	} else{
      printf("Añadido a pendentes\n");
      if(prio_orixe < mi_prio && sc == 1){
				sem_wait(&sem_prot_stop);
				stop = 1;
				sem_post(&sem_prot_stop);
				printf("Recibida peticion con mais prioridade dende %i\n", id_nodo_orixe);
				nodo_prio = id_nodo_orixe;

				if (mi_prio == 4){
	 				printf("Lector añadido a pendentes\n");
	  			id_nodos_pend[num_pend++] = id_nodo_orixe;
				}
      }else{
				id_nodos_pend[num_pend++] = id_nodo_orixe;
      }
   }

  }//--------cerra while ------------
  pthread_exit(NULL);
}


// -------------------------------- sendMsg ----------------------------------------------

int sendMsg(int tipo, int id_destino,int id_ticket){
  mensaxe msg;
  msg.id_nodo = mi_id;
  msg.mtype = id_destino;
	msg.clk = id_ticket;

  if (tipo == REPLY){
    msg.prio = REPLY;
    return msgsnd(id_cola_ack, (struct msgbuf *)&msg, sizeof(msg.prio) +
																											sizeof (msg.id_nodo)+
																										  sizeof(msg.mtype)+
																											sizeof(msg.clk) ,0);
  }else {
    printf("MI_CLK: %i\n",mi_clk);
    printf("MI PRIO: %i\n",mi_prio);
    msg.prio = mi_prio;
    msg.clk= mi_clk;
    printf("Enviado dende %i: prio: %i, mtype %li, clk: %i\n", msg.id_nodo, msg.prio , msg.mtype, msg.clk );
    return msgsnd(id_cola, (struct msgbuf *)&msg, sizeof(msg.prio)+
																								  sizeof(msg.id_nodo)+
																								  sizeof(msg.mtype)+
																									sizeof(msg.clk),0);
  }
}



//--------------------------------- receiveMsg --------------------------------------------

mensaxe receiveMsg(int id_cola) {
  mensaxe msg;
  int res = msgrcv (id_cola, (struct msgbuf *)&msg, sizeof(msg), mi_id, 0);
  printf("Recibo dende: %i e a cola e %i\n", msg.id_nodo, id_cola);
	printf("Recibo: type: %li,prio: %i,clk: %i,id_nodo: %i \n", msg.mtype, msg.prio, msg.clk, msg.id_nodo);
  return msg;
}

//------------------------------- crearVector ---------------------------------------------

void crearVector() {
  for(int i = 0, j = 0; i < N; i++){
    if(i + 1 != mi_id){
      id_nodos[j++] = (i + 1);
    }
  }
}

// ------------------------------- fillo ------------------------------------------------

void *fillo (void *args){

  char proceso[50];
  int *prio;
  prio = (int *)args;
	sem_t* sem_contador;
	sem_t* sem_proceso;
	int *contador;
  switch(*prio){
  case 1: strcpy(proceso, "anulacions");
		sem_contador = &sem_n_anulacions;
		sem_proceso = &sem_anulacions;
		contador = &n_anulacions;
		break;
  case 2: strcpy(proceso, "pagos");
		sem_contador = &sem_n_pagos;
		sem_proceso = &sem_pagos;
		contador = &n_pagos;
		break;
  case 3: strcpy(proceso, "pre-reservas");
		sem_contador = &sem_n_reservas;
		sem_proceso = &sem_reservas;
		contador = &n_reservas;
		break;
  case 4: strcpy(proceso, "gradas");
		sem_contador = &sem_n_lectores;
		sem_proceso = &sem_lectores;
		contador = &n_lectores;
		break;
	case 5: strcpy(proceso, "eventos");
		*prio= 4;
		sem_contador = &sem_n_lectores;
		sem_proceso = &sem_lectores;
		contador = &n_lectores;
		break;
  default : strcpy(proceso, "proceso desconocido"); break;
  }
	printf("Creando...\n");
	sem_wait(sem_contador);
	*contador = *contador + 1;
	sem_post(sem_contador);
	sem_post(&sem_entrada);
	printf("Aviso de entrada\n");
  sem_wait(sem_proceso);
  	printf("Proceso de %s na SC\n", proceso);
		// paramos dentro da sc ata que lle damos enter
  	//menu();
		sem_wait(sem_contador);
		*contador = *contador - 1;
		sem_post(sem_contador);
 	 	printf("Proceso de %s salindo da SC\n", proceso);
  sem_post(&sem_salida);

  if (mi_prio == 4){
    sem_post(&sem_paso_lectores);
  }
  pthread_exit(NULL);
}

//----------------------------------------MENU--------------------------------------

void menu(){
  //sem_t sem_paso_simu;
  char mander[30];
  int prio_novos= 5;
  int numero_fillos_novos = 0;
  while(1){
  printf("ENTER para continuar ou numero e prio de novos procesos\n");
  fgets(mander,30,stdin);
  fflush(stdin);
  if (mander[0] == '\n'){
    sem_post(&sem_paso_simu);
    break;
  }else{
    prio_novos =  atoi( &mander[2]);
    numero_fillos_novos = atoi(&mander[0]);
    num_fillos =+ numero_fillos_novos;
    pthread_t fioFillo[1000];
    printf("Creamos %i procesos con prio %i \n", numero_fillos_novos, prio_novos);

    //Creacion de fillos
    for(int i = 0; i < numero_fillos_novos; i++){
      pthread_create(&fioFillo[i],NULL, fillo, (void*) &prio_novos);
    }
    //sleep(2);
  }
  }
  return;
}
