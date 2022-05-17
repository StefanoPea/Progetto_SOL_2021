// ---------------------------------	INIZIO INCLUDE				---------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

// roba per i file
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
// ---------------------------------	FINE INCLUDE				---------------------------------------------



// ---------------------------------	INIZIO MACRO				---------------------------------------------

#define MAXSIZE 1024




// macro di utilità per controllare errori
#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

#define SYSCALL_PRINT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	errno = errno_copy;			\
    }

#define CHECK_EQ_EXIT(name, X, val, str, ...)	\
    if ((X)==val) {				\
        perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

#define CHECK_NEQ_EXIT(name, X, val, str, ...)	\
    if ((X)!=val) {				\
        perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }



#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { \
    fprintf(stderr, "ERRORE FATALE lock\n");		    \
    pthread_exit((void*)EXIT_FAILURE);			    \
  }
#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      { \
  fprintf(stderr, "ERRORE FATALE unlock\n");		    \
  pthread_exit((void*)EXIT_FAILURE);				    \
  }
#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { \
    fprintf(stderr, "ERRORE FATALE wait\n");		    \
    pthread_exit((void*)EXIT_FAILURE);				    \
}
/* ATTENZIONE: t e' un tempo assoluto! */
#define TWAIT(c,l,t) {							\
    int r=0;								\
    if ((r=pthread_cond_timedwait(c,l,t))!=0 && r!=ETIMEDOUT) {		\
      fprintf(stderr, "ERRORE FATALE timed wait\n");			\
      pthread_exit((void*)EXIT_FAILURE);					\
    }									\
  }
#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       {	\
    fprintf(stderr, "ERRORE FATALE signal\n");			\
    pthread_exit((void*)EXIT_FAILURE);					\
  }
#define BCAST(c)     if (pthread_cond_broadcast(c)!=0)    {		\
    fprintf(stderr, "ERRORE FATALE broadcast\n");			\
    pthread_exit((void*)EXIT_FAILURE);						\
  }

// ---------------------------------	FINE MACRO					---------------------------------------------



// ---------------------------------	INIZIO FUNZIONI DI SUPPORTO ---------------------------------------------

// ritorna
//   0: ok
//   1: non e' un numero
//   2: overflow/underflow

 int isNumber(const char* s, long* n) {
    if (s==NULL) return 1;
    if (strlen(s)==0) return 1;
    char* e = NULL;
    errno=0;
    long val = strtol(s, &e, 10);
    if (errno == ERANGE) return 2;    // overflow
    if (e != NULL && *e == (char)0) {
        *n = val;
        return 0;   // successo
    }
    return 1;   // non e' un numero
}

// ---------------------------------	FINE FUNZIONI DI SUPPORTO ---------------------------------------------



// ---------------------------------	INIZIO STORAGE SERVER	---------------------------------------------

// utilizzo una lista con inserimento in coda e rimozione in testa

//TODO Al momento la coda presenta solo una lock e una variabile condizione anziche' una per ogni nodo \
//TODO i vari nodi accettano il nome del file e il contenuto di esso come void*, quindi qualunque tipo





//TODO

typedef struct Node{

    char* filename;
    void* data;
    size_t size;
    struct Node* next;

} Node_t;

typedef struct Queue {

    Node_t *head;
    Node_t *tail;
    unsigned long qlen; // lunghezza coda
    pthread_mutex_t  qlock;
    pthread_cond_t qcond;

} Queue_t;

static inline Node_t  *allocNode()                  { return malloc( sizeof(Node_t));  }
static inline Queue_t *allocQueue()                 { return malloc(sizeof(Queue_t)); }
static inline void freeNode(Node_t *node)           { free(node->data);free((void*)node); }
static inline void LockQueue(Queue_t *q)            { LOCK(&q->qlock)   }
static inline void UnlockQueue(Queue_t *q)          { UNLOCK(&q->qlock) }
static inline void UnlockQueueAndWait(Queue_t *q)   { WAIT(&q->qcond, &q->qlock) }
static inline void UnlockQueueAndSignal(Queue_t *q) { SIGNAL(&q->qcond) UNLOCK(&q->qlock) }


Queue_t *initQueue(){
    Queue_t *coda = allocQueue();
    if (!coda) return NULL;
    coda->head = NULL;
    coda->tail = coda->head;
    coda->qlen = 0;
    if (pthread_mutex_init(&coda->qlock, NULL) != 0) {
        perror("mutex init");
        return NULL;
    }
    if (pthread_cond_init(&coda->qcond, NULL) != 0) {
        perror("mutex cond");
        if (&coda->qlock) pthread_mutex_destroy(&coda->qlock);
        return NULL;
    }
    return coda;
}




int pushFile(Queue_t* q, char* filename, size_t dimFile){

    if ((q == NULL) || (filename == NULL)) { errno= EINVAL; return -1;}
    Node_t *n = allocNode();

    if (!n) return -1;
    n->filename = filename;
    n->next = NULL;


    char* str = malloc(MAXSIZE);
    FILE* file = fopen(filename,"r");

    if (file == NULL) {
        printf("file can't be opened \n");
        return-1;
    }

    n->data = calloc(1,MAXSIZE);


    while (fgets(str, MAXSIZE, file) != NULL) {
       strncat(n->data, str,MAXSIZE);

    }


    free(str);
    fclose(file);
    //free(file);

    LockQueue(q);
    if(q->qlen == 0){
        q->head = q->tail = n;
    }else{
        q->tail->next = n;
        q->tail = n;
    }
    q->qlen      += 1;
    UnlockQueueAndSignal(q);

    return 0;
}





int push(Queue_t *q, void *data, char* filename) {
    if ((q == NULL) || (data == NULL) || (filename == NULL)) { errno= EINVAL; return -1;}
    Node_t *n = allocNode();
    if (!n) return -1;
    n->data = data;
    n->filename = filename;
    n->next = NULL;
    LockQueue(q);
    if(q->qlen == 0){
        q->head = q->tail = n;
    }else{
        q->tail->next = n;
        q->tail = n;
    }
    q->qlen      += 1;
    UnlockQueueAndSignal(q);
    return 0;
}


void* pop(Queue_t* coda) {

    if (coda == NULL) { errno= EINVAL; return NULL;}
    if(coda->qlen == 0){
        printf("cazzo togli, scemo\n");
        return NULL;
    }

    LockQueue(coda);

    //while(coda->head == coda->tail) {
     //   UnlockQueueAndWait(coda);
    //}

    //locked
    Node_t * tmp;
    /*Storing the head to a temporary variable*/
    tmp = coda->head;
    /*Moving head to the next node*/
    coda->head = coda->head->next;
    coda->qlen--;
    void* data = tmp->data;
    UnlockQueue(coda);
    /*Deleting the first node*/
    freeNode(tmp);
    return data;
}


void* searchQueue(Queue_t* coda, char* filename){

    Node_t * current = coda->head; // Initialize current
    while (current != NULL)
    {
        if (current->filename == filename)
            return current;
        current = current->next;
    }
    return NULL;
}

void deleteQueue(Queue_t* coda){

    while(coda->head != NULL) {
        Node_t *p = (Node_t*)coda->head;
        coda->head = coda->head->next;
        freeNode(p);
    }
    if (coda->head) freeNode((void*)coda->head);
    if (&coda->qlock)  pthread_mutex_destroy(&coda->qlock);
    if (&coda->qcond)  pthread_cond_destroy(&coda->qcond);
    free(coda);

}


unsigned long length(Queue_t *q) {
    LockQueue(q);
    unsigned long len = q->qlen;
    UnlockQueue(q);
    return len;
}

void print_file(Queue_t* coda, char* filename){
    Node_t* nodo;
    nodo = searchQueue(coda, filename);
    if(nodo == NULL){
        printf("File non presente nel server\n");
    }
    else{
        printf(" ----------------- il  nome del file e': -------------------\n %s\n", nodo->filename);
        printf("----------------- il  contenuto del file e': -------------------\n  %s\n", ( char*) nodo->data);}
}

void print_node(Node_t* nodo){

    if(nodo == NULL || nodo->filename == NULL || nodo->data == NULL){
        printf("Errore, nodo = NULL\n");
    }else {

        printf("-----------------           Nome file           -------------------\n\n %s\n", nodo->filename);
        printf("\n");
        printf("-----------------         Contenuto file        -------------------\n\n  %s\n\n", (char *) nodo->data);
    }

}

void print_list(Queue_t* coda) {

    Node_t * current_node = coda->head;
    if(current_node == NULL){
        printf("No file on the server\n");
    }
    while ( current_node != NULL){
        print_node(current_node);
        printf("+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n\n");
        current_node = current_node->next;

    }


}




// ---------------------------------	FINE STORAGE SERVER							---------------------------------------------



// ---------------------------------	INIZIO FUNZIONI API SERVER				---------------------------------------------

void* removeFile(Queue_t* coda, const char* filename){

    if (coda == NULL) { errno= EINVAL; return NULL;}
    LockQueue(coda);
    //while(coda->head == coda->tail) {
      //  UnlockQueueAndWait(coda);
    //}
    // Store head node
    Node_t *temp = coda->head, *prev;
    // If head node itself holds the key to be deleted
    if (temp != NULL && temp->filename == filename) {
        coda->head = temp->next; // Changed head
        void* data = temp->data;
        freeNode(temp);        // free old head
        return data;
    }
    // Search for the key to be deleted, keep track of the
    // previous node as we need to change 'prev->next'
    while (temp != NULL && temp->filename != filename) {
        prev = temp;
        temp = temp->next;
    }
    // If key was not present in linked list
    if (temp == NULL)
        return NULL;
    // Unlink the node from linked list
    prev->next = temp->next;
    void* data = temp->data;
    freeNode(temp);// Free memory

    UnlockQueue(coda);
    return data;

}


// ---------------------------------	FINE FUNZIONI API SERVER					---------------------------------------------



// ---------------------------------	INIZIO CODA RICHIESTE SERVER				---------------------------------------------

// ---------------------------------	FINE CODA RICHIESTE SERVER					---------------------------------------------



// ---------------------------------	INIZIO LOG SERVER							---------------------------------------------

// ---------------------------------	FINE LOG SERVER								---------------------------------------------



// ---------------------------------	INIZIO PARSING FILE DI INIZIALIZZAZIONE		---------------------------------------------

// ---------------------------------	FINE PARSING FILE DI INIZIALIZZAZIONE		---------------------------------------------



// ---------------------------------	INIZIO MAIN									---------------------------------------------

int main(int argc, char const *argv[]){


    Queue_t * coda;
    coda = initQueue();







    //push(coda,  stringa, stringa);
    //push(coda,   stringa2, stringa2);
    //push(coda,  stringa3, stringa3);
    //push(coda,  stringa4, stringa4);
    //push(coda,  stringa5, stringa5);
    //push(coda,  stringa6, stringa6);

    /*
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            printf("%s\n", dir->d_name);
        }
        closedir(d);
    }
*/


    //pushFile(coda, "test.txt", MAXSIZE);
    pushFile(coda, "lol.txt", MAXSIZE);
    pushFile(coda, "terzo.txt", MAXSIZE);



    //print_list(coda);


    //pop(coda);
    //print_list(coda);

    pop(coda);
    //pop(coda);
    //print_list(coda);


    //pushFile(coda, "lol.txt",MAXSIZE);
    //pushFile(coda, "terzo.txt", MAXSIZE);


    //print_list(coda);

    //removeFile(coda, "cazzo.txt");

    //removeFile(coda, "terzo.txt");

    print_list(coda);





    //print_list(coda);

    //printf("\n");
    //printf("lista dopo la rimozione:\n");
    //printf("\n");

    //pop(coda);
    //pop(coda);

    //print_list(coda);

    //push(coda, "cazzo", "nuova stringa");

    //print_list(coda, 'd');





    //free(nodo);
    //if(found) {
    //    printf("trovata\n");
    //}
    //else printf("non trovata\n");

    //removeFile(coda, "quarta");
    //printf("-+-+-+-+-+-+-+-+-+-+-\n");

    //print_list(coda);

    deleteQueue(coda);
    //print_list(coda);

    //printf("----------------------- FINE ---------------------------\n");

    /* code */
    return 0;
}


// ---------------------------------	FINE MAIN		---------------------------------------------