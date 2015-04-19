#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#define MAX_SIZE 100  //Theres no problem at saving some extra memory


int getSize(int num){
	int size = 0;
	while(num != 0){
		num =  num / 10;
		size++;
	}
	return size;
}


int checkFiles(int num, char * path[]){
	int index = 1;
	if( num < 2 ){ 									//At least two arguments
		printf("No suficient arguments were given\n");
		exit(EXIT_FAILURE);
	}
	char * noBarPath = malloc(sizeof(char) * MAX_SIZE);

	int pathLen = 0;
	
	if(path[1][strlen(path[1]) - 1] == '/')
		for(pathLen; pathLen < strlen(path[1]) - 1; pathLen++){
			noBarPath[pathLen] = path[1][pathLen];
		}
	else
		for(pathLen; pathLen < strlen(path[1]); pathLen++){
			noBarPath[pathLen] = path[1][pathLen];
		}


	char * pathToRead = noBarPath;

	if( access(pathToRead, F_OK ) == -1 ) {

    	printf("Read path is not valid\n");

		exit(EXIT_FAILURE);

	}else{
		for( index; 1; index++){
			
			char *temp = malloc (sizeof(char) * MAX_SIZE);

   			if (temp)
       			strcpy (temp, pathToRead);
   			else {
   				printf("Error copying file path\n");
   				exit(EXIT_FAILURE);
   			}

			char num[getSize(index)];

			sprintf(num, "%d", index);

			strcat(temp,"/");
			strcat(temp,num);
			strcat(temp,".txt");

			printf("Checking file %s\n",temp);

			if( access(temp, F_OK ) == -1 ){
				printf("There is no file %s\n",temp);
				break;
			}

		}

		if(index == 1){
			printf("There's no text files\n");
			exit(EXIT_FAILURE);
		}
	}

	char *temp = malloc (sizeof(char) * MAX_SIZE);

   	if (temp){
   		strcpy (temp, pathToRead);
       	strcat (temp, "/words.txt");
       	printf("Checking file %s\n",temp);
       }
   	else {
   		printf("Error copying file path\n");
   		exit(EXIT_FAILURE);
   	}

   	if( access(temp, F_OK ) == -1 ){
		printf("There is no file %s\nExiting...",temp);
		exit(EXIT_FAILURE);
	}
	return index - 1;
}



int main(int argc,char *argv[]){

	if(argc != 2){
		printf("ERROR! You should call your exe like this: (...)/index <path>\n");
	}

	unsigned int numOfTxt = checkFiles(argc,argv);  // Exit if files are not acording to rulles, returns number of txt files to read if they are

	char * pathToRead = argv[1];

	printf("There are files at this directory (%s), with %d txt's\n", pathToRead, numOfTxt);

	unsigned int numTemp = numOfTxt;
	for(numTemp; numTemp > 0; numTemp--){
		pid_t pid_sw = fork();
		if (pid_sw == 0){ // Son proccess handler
			printf("Executing son proccess for text file number %d\n",numTemp);
			char*argument = malloc(sizeof(char) * MAX_SIZE);
			sprintf(argument, "%d", numTemp);
			execlp("./sw", "./sw", pathToRead,argument, NULL);
			exit(EXIT_FAILURE);
		}else if(pid_sw < 0){
			printf("Son proccess sw couldn't be created\n");
			exit(EXIT_FAILURE);
		}
		wait();
	}

	int numOfTxtTemp = numOfTxt;

	for(numOfTxtTemp; numOfTxtTemp > 0; numOfTxtTemp--) //wait for son processes to finish
	wait();


	pid_t pid_csc =  fork();
	if (pid_csc == 0){ // Son proccess handler
		char num[getSize(numOfTxt)];
		sprintf(num, "%d", numOfTxt);
		printf("Path to read: %s\n", pathToRead);
		execlp("./csc", "./csc", pathToRead,num, NULL);
		printf("Failed to exec csc\n");
		exit(EXIT_FAILURE);
	}else if(pid_csc < 0){
		printf("Son proccess csc couldn't be created\n");
		exit(EXIT_FAILURE);
	}

	wait(); //wait for csc proccess to finish
	printf("Proccess ended successfully with no errors\n");
	exit(EXIT_SUCCESS);
}

