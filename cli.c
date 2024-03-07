#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>

#define MAX_PROCESS 50

pthread_mutex_t lock;

pthread_t threadIDs[MAX_PROCESS];
pid_t processIDs[MAX_PROCESS];
int processCount = 0;
int threadCount = 0; 

void waitFunction(){
	    		
	// Join all threads
    	for (int i = 0; i < threadCount; i++) {
        	pthread_join(threadIDs[i], NULL);
    	}
    	
    	for(int i = 0; i < threadCount; i++){
    		threadIDs[i] = 0;	
    	}
    	threadCount = 0;
    	
	int status;
	
	// Wait all processes 
	for(int i = 0; i < processCount; i++){
		waitpid(processIDs[i], &status, 0);
	}
	
	for(int i = 0; i < processCount; i++){
		processIDs[i] = 0;
	}
	processCount = 0;
	
}

void *threadFunction(void *arg) {

	pthread_mutex_lock(&lock);
	
	int *fd = (int *)arg;
	pthread_t threadId = pthread_self();
	fprintf(stdout, "---- %lu\n", (unsigned long)threadId);
	fflush(stdout);
	
	ssize_t nbytes;
	char buffer[1024];
	
	while(nbytes = read(fd[0], buffer, sizeof(buffer) > 0)){
		fwrite(buffer, 1, nbytes, stdout);
	}
	fflush(stdout);
	
	close(fd[0]);
	
	fprintf(stdout, "---- %lu\n", (unsigned long)threadId);
	fflush(stdout);
	
	free(fd);
	
	pthread_mutex_unlock(&lock);

	return NULL;
}
	
int main(int argc, char *argv[]) {
	
	if (pthread_mutex_init(&lock, NULL) != 0) { 
		printf("\n mutex init has failed\n");
		return 1; 
    	} 
	
	FILE *fp;
	FILE *output;
	
	char buff[255];

	fp = fopen("commands.txt", "r");
	
	output = fopen("parse.txt", "w");
	fclose(output);
	
	while (fgets(buff, sizeof(buff), fp) != NULL) {
		
		char *myargs[10];
	
		int count = 0;
		int optionIndex = 0;
		int inputIndex = 0;
		
		bool fileRead = false;
		bool fileWrite = false;
		bool backgroundJob = false;
		char fileName[255];
		
		for(int i = 0; i < 10; i++){
			myargs[i] = NULL;
		}

		char word[255];
		strcpy(word, buff);
		char *s = word;
		
		while (word) {
			strcpy(word, strtok_r(s, " ", &s));
			
	   		if(strchr(word, '&') != NULL) {
	   			backgroundJob = true;
	   			break;
	   		}
	   		else if (strchr(word, '\n') != NULL) { // this is so that, end of line charcter is not included inside the word we want to get.
				word[strcspn(word, "\n")] = 0;
				if(fileRead || fileWrite){
					strcpy(fileName, word);
				}
				else{
					if(strchr(word, '-') != NULL)
						optionIndex = count;
	   				else
	   					inputIndex = count;
					myargs[count] = strdup(word);
					count++;
				}
				break;	
			}
	   		else if(fileRead || fileWrite) {
	   			strcpy(fileName, word);
	   		}
	   		else if(strchr(word, '<') != NULL) {
	   			fileRead = true;
	   		}
	   		else if(strchr(word, '>') != NULL) {
	   			fileWrite = true;
	   		}
	   		else {
	   			if(strchr(word, '-') != NULL)
	   				optionIndex = count;
	   			else
	   				inputIndex = count;
	   				
	   			myargs[count] = strdup(word);
	   			count++;
	   		}
		}
		
		if(strcmp(myargs[0], "wait") == 0) {
			waitFunction();
			continue;
		}
		
		int *fd = malloc(2 * sizeof(int));
		pipe(fd);
		
		pid_t rc;
		
		rc = fork();
		
		if (rc < 0) {
			fprintf(stderr, "fork failed\n");
			exit(1);
		} 
		else if(rc == 0){
			
			close(fd[0]);
			
			if(fileRead) {
				int fdf = open(("./%s", fileName) , 0);
				
				dup2(fdf, 0);
				close(fdf);
			}
			if(fileWrite){
				close(fd[1]);
				int fdf = open(("./%s", fileName), O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
				
				dup2(fdf, 1);
				close(fdf);
				execvp(myargs[0], myargs);
			}
			else {
				dup2(fd[1], 1);
				close(fd[1]);
				execvp(myargs[0], myargs);
			}
		}
		else {
			processIDs[processCount] = rc;
			processCount++;
			
			if(!fileWrite){
				pthread_t tid;
			
				close(fd[1]);
				pthread_create(&tid, NULL, threadFunction, fd);
				threadIDs[threadCount] = tid;
		    		threadCount++;
		    	}
		    	
		    	if(!backgroundJob){
		    		int status;
		    		waitpid(rc, &status, 0);
		    	}
		}
		
		output = fopen("parse.txt", "a");
		fprintf(output, "----------\n");
		fprintf(output, "Command: %s\n", myargs[0]);
		if(inputIndex == 0)
			fprintf(output, "Inputs:\n");
		else
			fprintf(output, "Inputs: %s\n", myargs[inputIndex]);
		if(optionIndex == 0)
			fprintf(output, "Options:\n");
		else
			fprintf(output, "Options: %s\n", myargs[optionIndex]);
		if(fileRead)
			fprintf(output, "Redirection: <\n");
		else if(fileWrite)
			fprintf(output, "Redirection: >\n");
		else
			fprintf(output, "Redirection: -\n");
		if(backgroundJob)
			fprintf(output, "Background Job: y\n");
		else
			fprintf(output, "Background Job: n\n");
		fprintf(output, "----------\n");
		fclose(output);
   	}
	
	fclose(fp);
	
	waitFunction();
	
	pthread_mutex_destroy(&lock);
	
return 0;
}
