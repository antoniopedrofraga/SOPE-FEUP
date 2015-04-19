#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define MAX_SIZE 100  //Theres no problem at saving some extra memory
#define READ 0
#define WRITE 1

int getSize(int num){
	int size = 0;
	while(num != 0){
		num =  num / 10;
		size++;
	}
	return size;
}


int toInt(char a[]) {
  int pos, sign, offset, n = 0;
 
  if (a[0] == '-') {  // Handle negative integers
    sign = -1;
  }
 
  if (sign == -1)  // Set starting position to convert
    offset = 1;
  else
    offset = 0; 
 
  for (pos = offset; a[pos] != '\0'; pos++) //removing 0 from chars and adding it to n
    n = n * 10 + a[pos] - '0';
 
  if (sign == -1)
    n = -n;
 
  return n;
}


int main(int argc, char * argv[])
{
	if(argc != 3){
		printf("You're not using the right number of arguments\nUse it like this: csc <path> <number of files>\n");
		exit(EXIT_FAILURE);
	}
	
	char * path = argv[1];
	char * numOfFiles = argv[2];
	unsigned int num = toInt(numOfFiles);
	unsigned int num2 = toInt(numOfFiles);
	unsigned int num3 = toInt(numOfFiles);
	unsigned int num4 = toInt(numOfFiles);

	int fd[2];	
	pipe(fd);

	for(num; num > 0;num--){
		
		pid_t pid_cat = fork();

		if(pid_cat == 0){	
			char*pathTemp = malloc(sizeof(char) * MAX_SIZE);
			char*charNum = malloc(sizeof(char) * MAX_SIZE);
			sprintf(charNum, "%d", num);

			//preparing string to be used
			strcpy(pathTemp,"./");
			strcat(pathTemp,charNum);
			strcat(pathTemp,"temp.txt");

			//checking if file exists
			if(access(pathTemp, F_OK ) == -1) {
    			printf("Temp path (%s) is not valid\n", pathTemp);
				exit(EXIT_FAILURE);
			}
			printf("Path to file (%s) exists!\n",pathTemp);

			//opening file to read
			printf("Trying to open file (%s) to read\n",pathTemp);
			FILE * temp_file = fopen(pathTemp, "r");
			
			if(temp_file == NULL){
				printf("Could not open file (%s) to read\n", pathTemp);
				exit(EXIT_FAILURE);
			}

			close(fd[READ]);
			dup2(fd[WRITE], fileno(stdout));
			execlp("cat", "cat", pathTemp, NULL);
			exit(EXIT_FAILURE);
   		}else if(pid_cat < 0){
   			printf("Failed to create son csc proccess\n");
   			exit(EXIT_FAILURE);
   		}
	} 

	for(num3; num3 > 0; num3--){
		printf("Waiting for %d", num3);
		wait(NULL);
	}

	printf("Deleting temp files\n");

	for(num4; num4 > 0; num4--){
		char*pathTemp = malloc(sizeof(char) * MAX_SIZE);
		char*charNum = malloc(sizeof(char) * MAX_SIZE);
		sprintf(charNum, "%d", num4);
		strcpy(pathTemp,"./");
		strcat(pathTemp,charNum);
		strcat(pathTemp,"temp.txt");
		int status = remove(pathTemp);
		if( status == 0 )
      		printf("%s temp file deleted successfully.\n",pathTemp);
    	else
    	{
      		printf("Unable to delete the file (%s), exit unespectedly!\n", pathTemp);
      		exit(EXIT_FAILURE);
   		}

	}


	printf("Closing writing pipe\n");
	close(fd[WRITE]);


	char*temp_file_path = malloc(sizeof(char) * MAX_SIZE);
	strcpy(temp_file_path,path);
	strcat(temp_file_path,"/index_temp.txt");
	printf("Creating file to write (%s)...\n",temp_file_path);

	FILE * index = fopen(temp_file_path, "wa");


	if(index == NULL){
		printf("Couldn't create %s",temp_file_path);
		exit(EXIT_FAILURE);
	} else {
		printf("File %s was created successfully\n",temp_file_path);
	}
	
	char word[MAX_SIZE];
	int line;
	int number;
	FILE * pipeFS = fdopen(fd[READ],"r");
	while(fscanf(pipeFS,"%s : %d-%d",word,&number,&line) == 3){ //returns number of reads
		printf("Writing word (%s) from file number (%d) and line (%d) to index_temp\n",word,number,line);
		fprintf(index,"%s : %d-%d\n",word,number,line);
	}
	printf("Ended writing words to file (%s)\nClosing file...\n",temp_file_path);
	fclose(index); //have to close to sort

	printf("Sorting words with son proccess\n");

	pid_t pid = fork();

	if(pid == 0){
		printf("File to sort: (%s)\n",temp_file_path);
		char*temp = malloc(sizeof(char) * MAX_SIZE);
		strcpy(temp,temp_file_path);
		execlp("sort","sort","-V","-f","-o", temp, temp, NULL);
		printf("Could not execute sort\n");
		exit(EXIT_FAILURE);
	}else if (pid < 0){
		printf("Sort son proccess couldn't be created\n");
		exit(EXIT_FAILURE);
	}

	wait();
	printf("Ended sorting words\n");

	index = fopen(temp_file_path, "r");
	if(index == NULL){
		printf("Could not open file (%s) to read\n", temp_file_path);
		exit(EXIT_FAILURE);
	}

	char*final_path = malloc(sizeof(char) * MAX_SIZE);
	strcpy(final_path,path);
	strcat(final_path,"/index.txt");
	printf("Final Path no inicio: %s\n",final_path);

	FILE * final = fopen(final_path,"wa");
	if(final == NULL){
		printf("Could not open file (%s) to write\n", final_path);
		exit(EXIT_FAILURE);
	}

	fprintf(final,"INDEX\n\n");

	char word_2[MAX_SIZE];
	int line_2;
	int number_2;
	while(fscanf(index,"%s : %d-%d",word_2,&number_2,&line_2) == 3){ //returns number of reads
		printf("Writing word (%s) from file number (%d) and line (%d) to index\n",word_2,number_2,line_2);
		fprintf(final,"%s : %d-%d\n",word_2,number_2,line_2);
	}

	fclose(index);
	fclose(final);

	int status = remove(temp_file_path);

	fprintf(final,"INDEX\n\n");
	if( status == 0 )
      	printf("%s temp file deleted successfully.\n",temp_file_path);
    else
    {
      	printf("Unable to delete the file\n");
      	exit(EXIT_FAILURE);
   	}

	exit(EXIT_SUCCESS);
}