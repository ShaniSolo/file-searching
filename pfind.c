#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <fnmatch.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

typedef struct node { 
	char* path;
	struct node* next;
} node;		

node* pointerToHead = NULL;
node* pointerToTail = NULL;
node* thisNode;

pthread_t* thrds;
pthread_mutex_t qlock;
pthread_cond_t notEmpty;

int ActiveThreads;
int totalThreads;
int foundFiles = 0;


void enqueue(node* node) {
    pthread_mutex_lock(&qlock);
    if(pointerToHead == NULL) {
		pointerToHead = node;
    }
    else {
    	pointerToTail->next = node;
    }
    pointerToTail = node;
    pthread_mutex_unlock(&qlock);
}

node* dequeue() {
	pthread_mutex_lock(&qlock);
	while(pointerToHead == NULL) {
	    if(ActiveThreads <= 1) {
	    	pthread_t selfTID = pthread_self();
	    	for(int i=0; i < totalThreads; i++) {
				if(thrds[i] != selfTID) {
	    			pthread_cancel(thrds[i]);
					sleep(0.1);
	    			pthread_mutex_unlock(&qlock);
	    		}
	    	}
	    	printf("Done searching, found %d files\n", foundFiles);
	    	pthread_cancel(selfTID);
	    	pthread_mutex_unlock(&qlock);
	    	exit(0);
	    }
	ActiveThreads --;
	pthread_cond_wait(&notEmpty,&qlock);
	ActiveThreads ++;
    }
    node* firstNode;
    firstNode = malloc(sizeof(node)*1);
    firstNode = pointerToHead;
    pointerToHead =pointerToHead->next;
    pthread_mutex_unlock(&qlock);
    if(pointerToHead == NULL) {
    	pointerToTail  = NULL;
    }
    return firstNode;
}

int name_corresponds(const char *path, const char *str) {
	return fnmatch(str,strrchr(path,'/')+1 ,0) == 0;
}

void treat_file(const char* path, const char *str){
	if(name_corresponds(path, str)) {
		printf("%s\n", path);
		__sync_fetch_and_add(&foundFiles, 1);
	}
	  	
}

void browse(char* path, char* str) {
	DIR *dir = opendir(path);
	struct dirent *entry;
	struct stat dir_stat;
	if (! dir) { 
		perror(path); 
		return;
	 }
	while((entry = readdir(dir)) != NULL){
		char buff[strlen(path)+strlen(entry->d_name)+2];
        sprintf(buff,"%s/%s",path,entry->d_name);
        lstat(buff, &dir_stat);
		if(strcmp(entry->d_name,"..") != 0){
			if(((dir_stat.st_mode & __S_IFMT) == __S_IFDIR) && strcmp(entry->d_name, ".") != 0) {
				thisNode= malloc(sizeof(node));
				thisNode->path = malloc(strlen(buff)+1);
				strcpy(thisNode->path, buff);
				thisNode->next = NULL;
				enqueue(thisNode);
			}
			else {
				treat_file(buff, str);
			}
		 }
	 }
	closedir(dir);
}

void error() {
	printf("The data do not meet the requirements\n");
	exit(1);
}

void* threadsAction(void* vStr) {
	char* str;
	str = (char*)vStr;
	while(1){
		node* newNode = malloc(sizeof(node));
		newNode = dequeue();
		browse(newNode->path,str);
	}
}
void sigHandler() {
	for(int i=0; i < totalThreads; i++) {
		pthread_cancel(thrds[i]);
	}
	printf("Search stopped, found %d files\n",foundFiles);
	exit(0);
}




int main(int argc, char **argv) { 
	if(argc != 4) error();
	if (signal(SIGINT, &sigHandler) == SIG_ERR) {
		fprintf(stderr, "%s", strerror(errno));
		exit(1);
	}


	char *path;
	char* str  = malloc ((strlen(argv[2])+3)*sizeof(char));
	path = argv[1];

	char  Asterisk [1] = { '*' } ;
	strcat( str, Asterisk);
	strcat (str, argv[2]);
	strcat( str, Asterisk);
	
	totalThreads= atoi(argv[3]);
	ActiveThreads = totalThreads;
	thrds = malloc(sizeof(pthread_t)*totalThreads);
	
	node* startNode;
	startNode = malloc(sizeof(node));
	startNode->path = path;
	startNode->next = NULL;
	pointerToHead = startNode;
	pointerToTail = startNode;
	int creatPthread;
	for (int i=0; i<= totalThreads; i++)
	{
		creatPthread = pthread_create(&thrds[i], NULL, threadsAction, str);
		if(creatPthread) {
			printf("error in creating thread: %s\n", strerror(creatPthread));
			exit(-1);
		}
	}		
	
	for(int i=0; i<=totalThreads; i++) {
		pthread_join(thrds[i], NULL);
	}
	return 0;
}