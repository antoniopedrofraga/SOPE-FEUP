// PROGRAMA balcao

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // Para constantes O_*
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h> 
#include <string.h>
#include <signal.h> 
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUF_SIZE 5

#define MAX_NUM_DESK 300

#define MAX_SIZE 300

//Macros para tabelas
#define DESK_NUM 0  	//numero do balcao
#define TIME 1			//tempo de abertura
#define DURATION 2		//duracao de abertura do balcao
#define PID 3			//identificacao do processo do balcao
#define IN_RECEPTION 4	//clientes a serem atendidos
#define RECEPTIONED 5	//clientes ja atendidos
#define AVERAGE_TIME 6	//tempo medio de atendimento

int time_sum = 0; //variavel usada para calcular o tempo medio de cada balcao
time_t refresh;
int log_file;
char log_file_name[300];
int alarmflag = 0;
int fd1; //fd usado para abrir

typedef struct //Estrutura usada na memoria partilhada
{
	//Variaveis de condicao e mutex
	pthread_mutex_t table_lock;

	int time_sum;
	int sum; //total numero de balcoes ja criados
	int running; //numero de balcoes abertos / em execucao
	time_t mytime;
	int table[MAX_NUM_DESK][7];
} Shared_memory;

typedef struct //Argumentos a serem passados na criacao da thread principal
{
	char name[MAX_SIZE];
	int mytime;

} ArgsToSend;

typedef struct //Argumentos a serem passados na criacao de thread uma thread de atendimento
{
	Shared_memory * shm;
	char str[MAX_SIZE];
	int desk_number;

} Treatment_args;



void send_final_message(char * fc_name) //funcao para enviar a mensagem final ao ger_cl
{
    mkfifo(fc_name, 0660);
	int fd_c, messagelen;
	char message[] = "fim_atendimento";

	do{
		fd_c = open(fc_name, O_WRONLY);
		if (fd_c==-1) sleep(1);
	}while (fd_c==-1);
	messagelen = strlen(message) + 1;
	write(fd_c, message, messagelen);

	close(fd_c);

}

void alarmhandler(int signo) //Handler usado quando o alarme e ativado
{ 
	char fifo_name[MAX_SIZE];
	strcpy(fifo_name, "/tmp/fb_");
	sprintf(fifo_name, "/tmp/fb_%d", getpid());

	alarmflag = 1;

	if(unlink(fifo_name) < 0){
		perror("FIFO unlink()");
		exit(EXIT_FAILURE);
	} 

	close(fd1);
} 

char * chopN(char *str, size_t n) //Cortar os n primeiro chars de uma string
{
    if(n == 0 || str == 0)
    	return NULL;
    size_t len = strlen(str);
    if (n > len)
        return NULL;
    memmove(str, str+n, len - n + 1);
    return str;
}


void print_on_log(int desk_number, char * str){ //funcao para imprimir no log qualquer mesnagem (str)

	time(&refresh);
	char time_now[MAX_SIZE];
	strftime(time_now, 20, "%Y-%m-%d %H:%M:%S", localtime(&refresh));
	char msg[MAX_SIZE];
	strcpy(msg, str);
	if(strcmp(str, "cria_linh_mempart") != 0) strcat(msg, "\t");

	char message[MAX_SIZE];
	sprintf(message, "%s\t| Balcao | %d\t\t| %s\t| fb_%d\n",time_now, desk_number, msg, getpid());

	log_file = open(log_file_name, O_WRONLY | O_APPEND);
	if (log_file < 0)
	{
		perror("open()");
		exit(EXIT_FAILURE);
	}
	write(log_file, message, strlen(message));
	close(log_file);
}

void checkResult(char*string, int err)  //Verificar resultado
{
	if(err != 0)
	{
		printf("Error %d on %s\n", err, string);
		exit(EXIT_FAILURE);
	}
	return;
} 


void *treatment_thr(void * t_args) //Thread de atendimento
{
	int waiting_time = 10, res;
	char fc_name[MAX_SIZE];
	strcpy(fc_name, ((Treatment_args * )t_args)->str);
	
	res = pthread_mutex_lock(&((Treatment_args * )t_args)->shm->table_lock); //Vai ler da memoria, por isso nao convem que algum dos processos estejam a alterar memoria
	checkResult("pthread_mutex_lock()\n", res); 
	
	if(((Treatment_args * )t_args)->shm->table[(((Treatment_args * )t_args)->desk_number) - 1][IN_RECEPTION] < 10)
	{
	 	waiting_time = ((Treatment_args * )t_args)->shm->table[(((Treatment_args * )t_args)->desk_number) - 1][IN_RECEPTION] + 1;
	}
	res = pthread_mutex_unlock(&((Treatment_args * )t_args)->shm->table_lock); //Terminou a leitura da memoria
	checkResult("pthread_mutex_unlock()\n", res);

	time_sum += waiting_time;

	sleep(waiting_time);

	//Imprimir no log

	time(&refresh);
	char time_to_print[20];
	strftime(time_to_print, 20, "%Y-%m-%d %H:%M:%S", localtime(&refresh));

	char str[MAX_SIZE];
	strcpy(str,fc_name);

	log_file  =  open(log_file_name, O_WRONLY | O_APPEND);
	if (log_file < 0)
	{
		printf("Nome da mem: %s\n", log_file_name);
		printf("Error opening log file!\n");
		exit(EXIT_FAILURE);
	}

	char message[MAX_SIZE];
	sprintf(message, "%s\t| Balcao | %d\t\t| fim_atend_cl\t\t| %s\n", time_to_print, ((Treatment_args * )t_args)->desk_number, chopN(str, 5));
	write(log_file, message, strlen(message));
	close(log_file);

	send_final_message(fc_name);
	
	res = pthread_mutex_lock(&((Treatment_args * )t_args)->shm->table_lock); //Vai escrever em memoria, convem bloquear a memoria
	checkResult("pthread_mutex_lock()\n", res); 

	
	((Treatment_args * )t_args)->shm->table[(((Treatment_args * )t_args)->desk_number) - 1][RECEPTIONED]++;
	((Treatment_args * )t_args)->shm->table[(((Treatment_args * )t_args)->desk_number) - 1][IN_RECEPTION]--;
	
	res = pthread_mutex_unlock(&((Treatment_args * )t_args)->shm->table_lock); //terminou alteracoes
	checkResult("pthread_mutex_unlock()\n", res);

	free(t_args);

	pthread_exit(NULL);
}

int readline(int fd, char *str) //funcao para ler do fifo
{
	int n;

	do{
		n = read(fd,str,1);
	}while(n>0 && *str++ != '\0');

	return (n>0);
} 

int toInt(char a[]) //converter string para inteiro
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

  void destroy_shm(Shared_memory *shm, char * mem_name)
  {
	if(munmap(shm, sizeof(Shared_memory)) < 0) //desmapear objecto e respetiva verificacao de erros
	{
		perror("Failure in munmap()");
		exit(EXIT_FAILURE);
	}
	
	if(shm_unlink(mem_name) < 0) //remove nome de objecto de memoria partilhada e respetiva verificacao de erros
	{
		perror("Failure in shm_unlink()");
		exit(EXIT_FAILURE);
	}

} 


Shared_memory * create_shm(char * mem_name, int duration)
{
	int exist = 0;
	int shmfd;
	Shared_memory * shm;
	shmfd = shm_open(mem_name, O_CREAT|O_RDWR|O_EXCL, 0660); //Criar a regiao de memoria partilhada

	if(shmfd < 0) //Verificar se o shm_open nao falha
	{	
		if((shmfd = shm_open(mem_name, O_RDWR, 0660)) < 0) //Hipotese de falha gracas a existencia de uma shm com o mesmo nome, tenta abertura de read 
		{ 
			perror("Failure in shm_open()");
			return NULL;

		}else{ //Significa que existe uma memoria partilhada com este nome
			exist = 1;
		}
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

  //gerar o nome do log com o nome da memoria partilhada
	strcat(log_file_name, mem_name);
	strcat(log_file_name, ".log");
	chopN(log_file_name, 1);


	//inicializar dados na memoria partilhada

	if(exist == 0){ //Caso seja o primeiro balcao a ser criado

		shm->sum = 1;
		shm->running = 1;
		//Tempo do sistema
		time(&shm->mytime);

		pthread_mutex_init ( &shm->table_lock, NULL);

		char time_to_print[20];
		strftime(time_to_print, 20, "%Y-%m-%d %H:%M:%S", localtime(&shm->mytime)); //Uma maneira de passar o tempo para o formato correto

		printf("Nome da mem: %s\n", log_file_name);
		remove(log_file_name);
		log_file  =  open(log_file_name, O_CREAT | O_WRONLY, 0660);
		if (log_file < 0)
		{
			destroy_shm(shm, mem_name);
			perror("creating()");
			exit(EXIT_FAILURE);
		}


		char message1[] = "quando\t\t\t\t| quem\t | balcao\t| o_que\t\t\t\t| canal_criado/usado\n";
		char message2[] = "-------------------------------------------------------------------------------\n";
		char message3[MAX_SIZE];
		sprintf(message3, "%s\t| Balcao | 1\t\t| inicia_mempar\t\t| -\n", time_to_print);
		write(log_file, message1, strlen(message1));
		write(log_file, message2, strlen(message2));
		write(log_file, message3, strlen(message3));

		close(log_file);

	}else{ //Caso ja existam balcoes

		shm->sum++;
		shm->running++;

	}


	//Adicionar informacao ao log
	print_on_log(shm->sum, "cria_linh_mempart");


	//Adicionar uma linha a tabela
	shm->table[shm->sum - 1][DESK_NUM] = shm->sum;
	shm->table[shm->sum - 1][TIME] = time(NULL) - shm->mytime;
	shm->table[shm->sum - 1][DURATION] = -1;
	shm->table[shm->sum - 1][PID] = getpid();
	shm->table[shm->sum - 1][IN_RECEPTION] = 0;
	shm->table[shm->sum - 1][RECEPTIONED] = 0;
	shm->table[shm->sum - 1][AVERAGE_TIME] = 0;

	return shm;
}


void *thrFunc(void * args)
{
	char fifo_name[MAX_SIZE];      //Gerar o nome do fifo
	strcpy(fifo_name,"/tmp/fb_");
	char str_pid[MAX_SIZE];
	sprintf(str_pid, "%d", getpid());
	strcat(fifo_name, str_pid);

	mkfifo(fifo_name, 0660); //Criar o fifo com o nome gerado anteriormente

	Shared_memory * shm;
	if( (shm = create_shm(((ArgsToSend *)args)->name,((ArgsToSend *)args)->mytime)) == NULL ) 
	{
		//Caso tenha falhado em criar uma memoria partilhada
		printf("Shared memory could not be created\n");
		exit(EXIT_FAILURE);

	}
	printf("Store was created at %s", ctime(&shm->mytime));
	printf("Desk number %d\n", shm->sum);
	int desk_number = shm->sum; //Salvar numero de balcao porque a memoria partilhada vai ser alterada

	signal(SIGALRM, alarmhandler); 
  	alarm(((ArgsToSend *)args)->mytime);

	time_t opening_time = time(NULL); //Receber o tempo de abertura de balcao para controlar o tempo em que ele esta aberto

	fd1 = open(fifo_name, O_RDONLY);
	int fd2 = open(fifo_name, O_WRONLY); //Abrir um fifo de escrita e de leitura ao mesmo tempo para originar espera bloqueante

	if (fd1 < 0 && !alarmflag)
	{
		perror("FIFO open()");
		exit(EXIT_FAILURE);
	}



	putchar('\n');
	char str[100];

	pthread_t thread_array[5000];
	int size = 0;

	while(opening_time - shm->mytime < ((ArgsToSend *)args)->mytime){ //Enquanto o tempo nao passar
		if(readline(fd1,str)) //Se houver alguma coisa a ler (espera bloqueante porque ha um fifo de leitura e escrita abertos ao mesmo tempo, portanto so passa desta linha quando houver uma mensagem a receber do lado do cliente)
		{
			Treatment_args * t_args = malloc(sizeof(Treatment_args));
			t_args->shm = shm;
			t_args->desk_number = desk_number;
			strcpy(t_args->str, str);
			pthread_t treatment_thread;
			pthread_create(&treatment_thread, NULL, treatment_thr, t_args); //Cria uma thread de atendimento
			thread_array[size] = treatment_thread;
			size++;
		}

		opening_time = time(NULL); //Atualizar o tempo
	}
	//Depois de receber 
	while(size >= 0)
	{ //Espera que todas as threads criadas anteriormente terminem
		pthread_join(thread_array[size], NULL);
		size--;
	}

	close(fd1); //fecha os fifos
	close(fd2); 

 	shm->table[desk_number - 1][DURATION] = ((ArgsToSend *)args)->mytime; //Atualizar duracao na tabela quando o balcao fecha, valor diferente de -1
 	if(shm->table[shm->sum - 1][RECEPTIONED] > 0)
 		shm->table[desk_number - 1][AVERAGE_TIME] = time_sum / shm->table[shm->sum - 1][RECEPTIONED];



 	print_on_log(desk_number, "fecha_balcao");


	if(shm->running == 1) //Significa que este e o ultimo balcao em execucao, tem por isso que libertar a memoria partilhada
	{
		for(; shm->sum > 0; shm->sum--)
		{
			printf("Desk num: %d, opened at: %d, with duration of: %d, with pid of %d, with %d clients in reception, with %d receptioned clients, with %d seconds of average time\n",
				shm->table[shm->sum - 1][DESK_NUM], 
				shm->table[shm->sum - 1][TIME], 
				shm->table[shm->sum - 1][DURATION], 
				shm->table[shm->sum - 1][PID], 
				shm->table[shm->sum - 1][IN_RECEPTION], 
				shm->table[shm->sum - 1][RECEPTIONED],
				shm->table[shm->sum - 1][AVERAGE_TIME]);
		}

		print_on_log(desk_number, "fecha_loja");

		destroy_shm(shm,((ArgsToSend *)args)->name);
	}else{
		shm->running--;
	}

	close(log_file);
	free(args); //liberta memoria alocada anteriormente
	pthread_exit(NULL);
}

int main(int num, char * path[])
{
	if (num != 3) //O programa "balcao" espera 3 argumentos, caso contrario nao deve continuar a ser executado
	{
		printf("You are not using the right number of arguments\nUse it like: balcao < Shr_Mem Name > < Open Time >\n"); 
		exit(EXIT_FAILURE);
	}

	ArgsToSend * args =  malloc(sizeof(ArgsToSend));

	strcpy(args->name,path[1]);
	if(args->name[0] != '/')
	{
		char name[MAX_SIZE];
		strcpy(name, "/");
		strcat(name, args->name);
		*args->name = '\0'; //Apagar o valor atual da string
		strcpy(args->name, name);
	}

	args->mytime = toInt(path[2]);

	pthread_t thread;
	pthread_create(&thread, NULL, thrFunc, (void *)args); //Cria uma thread e passa uma struct de argumentos

	pthread_exit(0);

}