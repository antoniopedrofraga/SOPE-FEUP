#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define READ 0
#define WRITE 1
#define MAX_SIZE 100 //Theres no problem at saving some extra memory

char * getTxtPath(char * pathToRead, char * txt){
	printf("I will get txtPath with %s and number %s\n",pathToRead,txt);
	char * temp = malloc (sizeof(char) * MAX_SIZE);
	strcpy(temp,pathToRead);
	strcat(temp,"/");
	strcat(temp,txt);
	strcat(temp,".txt");
	printf("getTxtPath() will return %s\n",temp);
	return temp;
}

char * getWordsPath(char * pathToRead){
	char * temp = malloc (sizeof(char) * MAX_SIZE);
	strcpy(temp,pathToRead);
	strcat(temp,"/words.txt");
	return temp;
}

int main(int argc, char * argv[])
{
	if(argc < 3){
		printf("Not enough arguments\n");
		exit(EXIT_FAILURE);
	}

	char * pathToRead = argv[1];

	char * txtPath = malloc(sizeof(char) * MAX_SIZE);
	strcpy(txtPath,getTxtPath(pathToRead , argv[2]));
	char * wordsPath = malloc(sizeof(char) * MAX_SIZE);
	strcpy(wordsPath,getWordsPath(pathToRead));

	int fd[2];
	pipe(fd);

	pid_t pid =  fork();
	if (pid == 0){ // Son proccess handler
		printf("Writing to pipe on SW son proccess\n");
		char * temp_txt = malloc(sizeof(char) * MAX_SIZE);
		strcpy(temp_txt,txtPath);
		char * temp_words = malloc(sizeof(char) * MAX_SIZE);
		strcpy(temp_words,wordsPath);
		printf("I'll grep from %s to %s\n",temp_words,temp_txt);
		close(fd[READ]);
		dup2(fd[WRITE], fileno(stdout));
		execlp("grep", "grep", "-n", "-o", "-w", "-f", temp_words, temp_txt, NULL);
		exit(EXIT_FAILURE);
	}else if (pid > 0){ // Father proccess handler
		wait(NULL);
		printf("Reading from pipe on SW proccess\n");
		close(fd[WRITE]);

		FILE * file = fdopen(fd[READ],"r");
		char word[MAX_SIZE];
		int line;

		printf("Son finished executing his code\n");
		
		char*temp_file_path= malloc(sizeof(char) * MAX_SIZE);
		strcpy(temp_file_path,argv[2]);
		strcat(temp_file_path,"temp.txt");

		FILE * temp_file = fopen(temp_file_path, "wa");

		if(temp_file == NULL){
			printf("Couldn't create %s",temp_file_path);
			exit(EXIT_FAILURE);
		} else {
			printf("File %s was created successfully\n",temp_file_path);
		}

		while(fscanf(file,"%d:%s",&line,word) == 2){ //returns number of reads
			printf("Writing word (%s) on line (%d) to file (%s)\n",word,line,temp_file_path);
			fprintf(temp_file,"%s : %s-%d\n",word,argv[2],line);
		}
		free(temp_file_path);
	}else{
		printf("SW son proccess couldn't be created\n");
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
