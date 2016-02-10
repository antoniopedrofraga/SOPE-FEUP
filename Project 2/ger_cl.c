// PROGRAMA ger_cl

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> // Para constantes O_*
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_SIZE 300
#define BUF_SIZE 5
#define MAX_NUM_DESK 300
#define MAX_INT 32767 //Valor maximo de um inteiro

//Macros para tabelas
#define DESK_NUM 0  	//numero do balcao
#define TIME 1			//tempo de abertura
#define DURATION 2		//duracao de abertura do balcao
#define PID 3			//identificacao do processo do balcao
#define IN_RECEPTION 4	//clientes a serem atendidos
#define RECEPTIONED 5	//clientes ja atendidos
#define AVERAGE_TIME 6	//tempo medio de atendimento

time_t refresh;
char log_file_name[MAX_SIZE];
int log_file;

typedef struct
{
	//Variaveis de condicao e mutex
	pthread_mutex_t table_lock;
	int time_sum;
	int sum; //total numero de balcoes ja executados
	int running; //numero de balcoes em execucao
	time_t mytime;
	int table[MAX_NUM_DESK][7];

} Shared_memory;

char * chopN(char *str, size_t n) //cortar os primeiros n chars de str
{
    if(n == 0 || str == 0)
    	return NULL;
    size_t len = strlen(str);
    if (n > len)
        return NULL;
    memmove(str, str+n, len - n + 1);
    return str;
}

void checkResult(char*string, int err) //Verificar resultado ou erro
{
	if(err != 0)
	{
		printf("Error %d on %s\n", err, string);
		exit(EXIT_FAILURE);
	}
	return;
} 

void send_fifo_name(Shared_memory * shm, char * fifoc_name, int best_desk, char * shm_name) //funcao para enviar o nom do fifo privado criado pelo cliente
{
	time(&refresh);
	char time_now[20];
	strftime(time_now, 20, "%Y-%m-%d %H:%M:%S", localtime(&refresh));
	char message[MAX_SIZE];
	sprintf(message, "%s\t| Client | %d\t\t| pede_atendimento\t| fc_%d\n", time_now, best_desk + 1, getpid());
	log_file = open(log_file_name, O_WRONLY | O_APPEND);
	if (log_file < 0)
	{
		printf("Nome da mem: %s\n", log_file_name);
		printf("Error opening log file!\n");
		exit(EXIT_FAILURE);
	}
	write(log_file, message, strlen(message));
	close(log_file);

	//Passar o identificador do melhor balcao para string

	char fifob_name[MAX_SIZE];      //Gerar o nome do fifo
	strcpy(fifob_name,"/tmp/fb_");
	char str_pid_b[MAX_SIZE];
	sprintf(str_pid_b, "%d", shm->table[best_desk][PID]);
	strcat(fifob_name, str_pid_b);

	//Lidar com o fifo do balcao (enviar o nome do fifo privado)

	int fd_b, messagelen;
	char msg[100];

	do{
		fd_b = open(fifob_name, O_WRONLY);
		if (fd_b==-1) sleep(1);
	}while (fd_b==-1);
			
	sprintf(msg,"%s", fifoc_name);
	messagelen = strlen(msg) + 1;
	log_file = open(log_file_name, O_WRONLY | O_APPEND);
	if (log_file < 0)
	{
		printf("Nome da mem: %s\n", log_file_name);
		printf("Error opening log file!\n");
		exit(EXIT_FAILURE);
	}
	write(fd_b, msg, messagelen);
	close(log_file);
			
	//---------------
	close(fd_b);
}

int readline(int fd, char *str) //ler informacao do fifo
{
	int n;

	do{
		n = read(fd,str,1);
	}while(n>0 && *str++ != '\0');

	return (n>0);
} 

int getBestDesk(Shared_memory * shm)  // funcao que retorna o numero do balcao com menos clientes
{		
	int min = MAX_INT, desk_num = 0, return_desk;
	for( ; desk_num < shm->sum; desk_num++)
	{
		if((min > shm->table[desk_num][IN_RECEPTION]) && (-1 == shm->table[desk_num][DURATION])){
			min = shm->table[desk_num][IN_RECEPTION];
			return_desk = desk_num;
		}
	}
	return return_desk;
}

int toInt(char a[]) //funcao que converte string em inteiro
{
	int pos, sign, offset, n = 0;

  	if (a[0] == '-') //numeros negativos
  	{  
  		sign = -1;
  	}

  	if (sign == -1)  //definir posicao inicial para converter
  		offset = 1;
  	else
  		offset = 0; 

  	for (pos = offset; a[pos] != '\0'; pos++) //remover o char '\0'
  		n = n * 10 + a[pos] - '0';

  	if (sign == -1)
  		n = -n;

  	return n;
}


Shared_memory * getShm(char * shm_name) //funcao para obter memoria partilhada criada em balcao atraves do nome
{
	int shmfd;
	Shared_memory * shm;
	if((shmfd = shm_open(shm_name, O_RDWR, 0660)) < 0) //Verificar se existe uma memoria partilhada com este nome 
	{
		printf("Shared Memory with name %s does not exist\n", shm_name); //Nao existe uma memoria partilhada com este nome e por isso o programa nao deve continuar
		exit(EXIT_FAILURE);
	}


	if(ftruncate(shmfd, sizeof(Shared_memory)) < 0) //Especificar o tamanho da regiao de memoria partilhada
	{
		perror("Failure in ftruncate()");
		return NULL;
	} 

	shm = mmap(0, sizeof(Shared_memory), PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0); //juntar a regiao a memoria virtual
	
	if(shm == MAP_FAILED) //Em caso do mmap falhar
	{
		perror("Failure in mmap()");
		return NULL;
	}

	return shm;

}


int main(int num, char * path[])
{
	int i, num_of_clients;
	if (num != 3) //O programa "ger_cl" espera 3 argumentos, caso contrario nao deve continuar a ser executado
	{
		printf("You are not using the right number of arguments\nUse it like: ger_cl < Shr_Mem Name > < Number of Clients >\n"); 
		exit(EXIT_FAILURE);
	}


	char * shm_name = malloc(sizeof(char) * MAX_SIZE); //guardar o nome da memoria partilhada
	strcpy(shm_name, path[1]);

	//gerar o nome do log com o nome da memoria partilhada
	strcpy(log_file_name, shm_name);
	strcat(log_file_name, ".log");
	if(shm_name[0] == '/')
	chopN(log_file_name, 1);

	

	num_of_clients = toInt(path[2]); //guardar o numero de clientes num inteiro

	Shared_memory * shm;

	if((shm = getShm(shm_name)) == NULL) //Verificacao de erros para a obtencao da memoria partilhada criada em balcao
	{
		printf("Error getting shared memory\n");
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < num_of_clients; i++){

		pid_t pid = fork();
		
		if( pid < 0){ //Erro ao ciar um processo filho
			perror("Error with fork()\n");
			exit(EXIT_FAILURE);
		}else if ( pid == 0){ //Handler do processo filho

			
			//Recebe o melhor balcao e aumenta o numero de clientes nele
			int best_desk = getBestDesk(shm);
			shm->table[best_desk][IN_RECEPTION]++;

			//FIFO privado do cliente

			char fifoc_name[MAX_SIZE];      //Gerar o nome do fifo
			strcpy(fifoc_name,"/tmp/fc_");
			char str_pid_c[MAX_SIZE];
			sprintf(str_pid_c, "%d", getpid());
			strcat(fifoc_name, str_pid_c);


			mkfifo(fifoc_name, 0660); //Criar o fifo com o nome gerado anteriormente

			send_fifo_name(shm, fifoc_name, best_desk, shm_name); //Envia o nome do fifo privado ao balcao

			
			//Receber a mensagem final
			//-------------------------
			int fd_c = open(fifoc_name, O_RDONLY);
			putchar('\n');
			char str[100];

			while(readline(fd_c,str))
			{ 
				if(strcmp(str,"fim_atendimento") != 0)
				{
					exit(EXIT_FAILURE);
				}else if(strcmp(str,"fim_atendimento") == 0){
					time(&refresh);
					char time_now[MAX_SIZE];
					strftime(time_now, 20, "%Y-%m-%d %H:%M:%S", localtime(&refresh));
					char message[MAX_SIZE];
					sprintf(message, "%s\t| Client | %d\t\t| fim_atendimento\t| fc_%d\n",time_now, best_desk + 1, getpid());
					log_file = open(log_file_name, O_WRONLY | O_APPEND);
					if (log_file < 0)
					{
						printf("Nome da mem: %s\n", log_file_name);
						printf("Error opening log file!\n");
						exit(EXIT_FAILURE);
					}
					write(log_file, message, strlen(message));
					close(log_file);
				}else{
					printf("Nao recebeu notificacao\n");
					exit(EXIT_FAILURE);
				}
			}

			close(fd_c);
			//--------------------------
			close(log_file);
			exit(EXIT_SUCCESS); //Terminar para nao correr as linhas abaixo e para nao continuar a correr o ciclo
		}
	}

	for(i = 0; i < num_of_clients; i++) wait(NULL); //wait() em paralelo
	
	free(shm_name);

	return 0;
}
