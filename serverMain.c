
// TODO -------------------------------------------------------------------------------------------------
//  - Il server in questo momento riceve dal client un comando e puo' mandare indietro una risposta
//  - Bisogna capire dove gestire l'uscita e la rimozione della lista, visto che parte del codice e' 'unreachable'
//  - Prossima cosa da fare e' far parsare i comandi al client, inviarli al server tramite API e far fare al server il dovuto
//  - Direi che per partire basta avere 2 comandi, add e remove, poi si puo' passare al lavoro con piu' thread
//  ------------------------------------------------------------------------------------------------------------------




// TODO ---------------------------------	INIZIO INCLUDE				---------------------------------------------

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


#include <ctype.h>


#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>

// TODO ---------------------------------	FINE INCLUDE				---------------------------------------------



// TODO ---------------------------------	INIZIO MACRO				---------------------------------------------

#define MAXSIZE 1024



#define EXTRA_LEN_PRINT_ERROR   512

#define SOCKNAME     "./cs_sock"
#define MAXBACKLOG   32




/* A macro that checks if the system call is successful. If it is not, it prints the error message and exits. */
#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }

/* A macro that is used to check if a system call has failed. If it has failed, it prints the error message and the error
number. */
#define SYSCALL_PRINT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	errno = errno_copy;			\
    }



/* A macro that checks if the value of X is not equal to val. If it is equal, it prints the name of the function that
was called, the error message, and the error number. It then exits the program. */
#define CHECK_EQ_EXIT(name, X, val, str, ...)	\
    if ((X)==val) {				\
        perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }


/* A macro that checks if the value of X is not equal to val. If it is not equal, it prints the name of the function that
was called, the error message, and the error number. It then exits the program. */
#define CHECK_NEQ_EXIT(name, X, val, str, ...)	\
    if ((X)!=val) {				\
        perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }



/* A macro that locks a mutex. */
#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { \
    fprintf(stderr, "ERRORE FATALE lock\n");		    \
    pthread_exit((void*)EXIT_FAILURE);			    \
  }
/* A macro that is used to unlock a mutex. */
#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      { \
  fprintf(stderr, "ERRORE FATALE unlock\n");		    \
  pthread_exit((void*)EXIT_FAILURE);				    \
  }
/* A macro that checks if the condition variable is waiting. If it is not, it prints an error message and exits the thread. */
#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { \
    fprintf(stderr, "ERRORE FATALE wait\n");		    \
    pthread_exit((void*)EXIT_FAILURE);				    \
}
/* ATTENZIONE: t e' un tempo assoluto! */
/* A macro that waits for a condition to be true. */
#define TWAIT(c,l,t) {							\
    int r=0;								\
    if ((r=pthread_cond_timedwait(c,l,t))!=0 && r!=ETIMEDOUT) {		\
      fprintf(stderr, "ERRORE FATALE timed wait\n");			\
      pthread_exit((void*)EXIT_FAILURE);					\
    }									\
  }
/* A macro that is used to signal a condition. */
#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       {	\
    fprintf(stderr, "ERRORE FATALE signal\n");			\
    pthread_exit((void*)EXIT_FAILURE);					\
  }
/* A macro that is used to broadcast a condition. */
#define BCAST(c)     if (pthread_cond_broadcast(c)!=0)    {		\
    fprintf(stderr, "ERRORE FATALE broadcast\n");			\
    pthread_exit((void*)EXIT_FAILURE);						\
  }

// TODO ---------------------------------	FINE MACRO					---------------------------------------------







// TODO ---------------------------------	INIZIO STORAGE SERVER	---------------------------------------------

// utilizzo una lista con inserimento in coda e rimozione in testa

//TODO Al momento la coda presenta solo una lock e una variabile condizione anziche' una per ogni nodo \
//TODO i vari nodi accettano il nome del file e il contenuto di esso come void*, quindi qualunque tipo

/**
 * A Node_t is a struct that contains a filename, a pointer to some data, the size of the data, and a pointer to the next
 * Node_t.
 * @property {char} filename - The name of the file.
 * @property {void} data - The data that is stored in the file.
 * @property {size_t} size - The size of the data in bytes
 * @property next - pointer to the next node in the list
 */
typedef struct Node{

    char* filename;
    void* data;
    size_t size;
    struct Node* next;

} Node_t;


/**
 * A queue is a structure that contains a pointer to the head and tail of a linked list, a counter of the number of
 * elements in the list, a mutex and a condition variable.
 * @property {Node_t} head - the first element of the queue
 * @property {Node_t} tail - the last element of the queue
 * @property {unsigned long} qlen - the length of the queue
 * @property {pthread_mutex_t} qlock - a mutex that protects the queue from concurrent access
 * @property {pthread_cond_t} qcond - a condition variable that is used to signal the arrival of a new element in the
 * queue.
 */
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


/**
 * It allocates a new queue, initializes its mutex and condition variable, and returns a pointer to the new queue
 *
 * @return A pointer to a Queue_t struct.
 */
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


/**
 * It opens the file, reads the file into a buffer, allocates a node, and pushes the node onto the queue
 *
 * @param q the queue to push the file into
 * @param filename the name of the file to be pushed
 * @param dimFile the size of the file
 *
 * @return the number of bytes read from the file.
 */
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


/**
 * > The function takes a queue, a data pointer, and a filename pointer as arguments. It allocates a new node, sets the
 * node's data and filename pointers to the arguments, and then adds the node to the end of the queue
 *
 * @param q The queue to push to
 * @param data the data to be pushed into the queue
 * @param filename the name of the file that the data came from
 *
 * @return The return value is the number of bytes that were written to the file.
 */
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


/**
 * It locks the queue, checks if the queue is empty, if it is, it unlocks the queue and waits for a signal, otherwise it
 * stores the head of the queue in a temporary variable, moves the head to the next node, decrements the queue length,
 * stores the data of the head in a temporary variable, unlocks the queue, frees the head and returns the data
 *
 * @param coda the queue to pop from
 *
 * @return The data stored in the node.
 */
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


/**
 * It searches the queue for a node with a specific filename and returns it if found, otherwise it returns NULL
 *
 * @param coda the queue to search
 * @param filename the name of the file to search for
 *
 * @return The node that contains the filename.
 */
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


/**
 * It frees all the nodes in the queue, then it frees the queue itself
 *
 * @param coda the queue to be deleted
 */
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


/**
 * Lock the queue, get the length, unlock the queue, return the length.
 *
 * @param q The queue to operate on.
 *
 * @return The length of the queue.
 */
unsigned long length(Queue_t *q) {
    LockQueue(q);
    unsigned long len = q->qlen;
    UnlockQueue(q);
    return len;
}


/**
 * It prints the content of a file
 *
 * @param coda the queue where the file is stored
 * @param filename the name of the file to be printed
 */
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


/**
 * It prints the content of a node
 *
 * @param nodo the node to be printed
 */
void print_node(Node_t* nodo){

    if(nodo == NULL || nodo->filename == NULL || nodo->data == NULL){
        printf("Errore, nodo = NULL\n");
    }else {

        printf("-----------------           Nome file           -------------------\n\n %s\n", nodo->filename);
        printf("\n");
        printf("-----------------         Contenuto file        -------------------\n\n  %s\n\n", (char *) nodo->data);
    }

}


/**
 * It prints the list of files on the server
 *
 * @param coda the queue to print
 */
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



// TODO ---------------------------------	FINE STORAGE SERVER							---------------------------------------------


// TODO ---------------------------------	INIZIO FUNZIONI DI SUPPORTO ---------------------------------------------

// ritorna
//   0: ok
//   1: non e' un numero
//   2: overflow/underflow

/**
 * It converts a string to a long integer, and returns 0 if the conversion was successful, 1 if the string is not a
 * number, and 2 if the number is too big
 *
 * @param s the string to be converted
 * @param n the number to be converted
 */
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



void cleanup() {
    unlink(SOCKNAME);
}

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
static inline int readn(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
        if ((r=read((int)fd ,bufptr,left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;   // EOF
        left    -= r;
        bufptr  += r;
    }
    return size;
}

/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
static inline int writen(long fd, void *buf, size_t size) {
    size_t left = size;
    int r;
    char *bufptr = (char*)buf;
    while(left>0) {
        if ((r=write((int)fd ,bufptr,left)) == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) return 0;
        left    -= r;
        bufptr  += r;
    }
    return 1;
}

/**
 * \brief Procedura di utilita' per la stampa degli errori
 *
 */
static inline void print_error(const char * str, ...) {
    const char err[]="ERROR: ";
    va_list argp;
    char * p=(char *)malloc(strlen(str)+strlen(err)+EXTRA_LEN_PRINT_ERROR);
    if (!p) {
        perror("malloc");
        fprintf(stderr,"FATAL ERROR nella funzione 'print_error'\n");
        return;
    }
    strcpy(p,err);
    strcpy(p+strlen(err), str);
    va_start(argp, str);
    vfprintf(stderr, p, argp);
    va_end(argp);
    free(p);
}



//TODO QUESTA E' ROBA CHE POI NON MI SERVIRA', E' QUI SOLO PER VEDERE SE FUNZIONA LA CONNESSIONE
// TRA CLIENT E SERVER


/**
 * tipo del messaggio
 */
typedef struct msg {
    int len;
    char *str;
} msg_t;


// converte tutti i carattere minuscoli in maiuscoli
void toup(char *str) {
    char *p = str;
    while(*p != '\0') {
        *p = (islower(*p)?toupper(*p):*p);
        ++p;
    }
}

//TODO QUESTA E' LA PARTE CHE DOVREBBE GESTIRE LE VARIE RICHIESTE DEL SERVER


int cmd(Queue_t * coda, long connfd) {
    msg_t str;
    if (readn(connfd, &str.len, sizeof(int))<=0) return -1;
    str.str = calloc((str.len), sizeof(char));
    if (!str.str) {
        perror("calloc");
        fprintf(stderr, "Memoria esaurita....\n");
        return -1;
    }
    if (readn(connfd, str.str, str.len*sizeof(char))<=0) return -1;

    printf("%s\n %d\n", str.str,str.len);
    /*
    toup(str.str);
    if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); return -1;}
    if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); return -1;}
    */

    if(strcmp(str.str, "add") == 0){

        //toup(str.str);
        //printf("%s\n", str.str);


        //realloc(str.str, strlen("tutto bene!"));

        //free(str.str);

        pushFile(coda, "terzo.txt",MAXSIZE);



        //printf("OK");
        char* response = "tutto bene!";
        //printf("OK");

        free(str.str);
        str.str = calloc(strlen(response)+1, sizeof(char));
        strcpy(str.str, response);
        str.len = strlen(str.str);
        //free(response);

        printf("%s\n %d\n", str.str,str.len);

        if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); return -1;}
        if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); return -1;}




        //if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); return -1;}
        //if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); return -1;}



        //printf("sono entrato dove dovevo entrare");
        //free(str.str);
        //str.str = malloc(strlen("tutto bene!"));

        //realloc(str.str, strlen(str.str) *sizeof(char));
        //free(str.str);
        //str.str=malloc(strlen(str.str) * sizeof(char));
        //str.str = "tutto bene";
        //strcpy(str.str, "tutto bene");
        //printf("%s\n", str.str);
        //str.len = strlen(str.str);
        //printf("%d\n", str.len);
        //if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); return -1;}
        //if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); return -1;}

    }



    free(str.str);
    return 0;
}


// ritorno l'indice massimo tra i descrittori attivi
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
        if (FD_ISSET(i, &set)) return i;
    assert(1==0);
    return -1;
}



// TODO ---------------------------------	FINE FUNZIONI DI SUPPORTO ---------------------------------------------


// TODO ---------------------------------	INIZIO FUNZIONI API SERVER				---------------------------------------------

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


//TODO Da finire il ritorno di n file random
void* readNFiles(int N, const char* dirname, Queue_t* coda){

    char* files = calloc(1,MAXSIZE);

    Node_t * current_node = coda->head;

    if(N<=0){

        if(current_node == NULL){
            strncat(files, "no files on server\n", MAXSIZE);
        }

        while ( current_node != NULL){
            strncat(files, "file: ",MAXSIZE);
            strncat(files, current_node->filename,MAXSIZE);
            strncat(files, "\n",MAXSIZE);
            strncat(files, "contenuto:\n",MAXSIZE);
            strncat(files, current_node->data,MAXSIZE);

            current_node = current_node->next;

        }
    }
    return files;
}

//TODO Da fare del tutto
void* writeFile(){

    return 0;
}


// TODO ---------------------------------	FINE FUNZIONI API SERVER					---------------------------------------------



// TODO ---------------------------------	INIZIO CODA RICHIESTE SERVER				---------------------------------------------

// TODO ---------------------------------	FINE CODA RICHIESTE SERVER					---------------------------------------------



// TODO ---------------------------------	INIZIO LOG SERVER							---------------------------------------------

// TODO ---------------------------------	FINE LOG SERVER								---------------------------------------------



// TODO ---------------------------------	INIZIO PARSING FILE DI INIZIALIZZAZIONE		---------------------------------------------

// TODO ---------------------------------	FINE PARSING FILE DI INIZIALIZZAZIONE		---------------------------------------------



// TODO ---------------------------------	INIZIO MAIN									---------------------------------------------

int main(int argc, char const *argv[]){


    Queue_t * coda;
    coda = initQueue();





    //TODO PER IL MOMENTO HO SOLO COPIATO IL CODICE DEL PROF E FUNZIONA, OVVERO MANDA UNA STRINGA IL CLIENT
    // E IL SERVER LA RITORNA MAIUSCOLA
    cleanup();
    atexit(cleanup);

    int listenfd;
    SYSCALL_EXIT("socket", listenfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME)+1);
    int notused;



    SYSCALL_EXIT("bind", notused, bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)), "bind", "");
    SYSCALL_EXIT("listen", notused, listen(listenfd, MAXBACKLOG), "listen", "");



    //printf("bind e listen OK\n");
    fd_set set, tmpset;

    // azzero sia il master set che il set temporaneo usato per la select
    FD_ZERO(&set);
    FD_ZERO(&tmpset);

    // aggiungo il listener fd al master set
    FD_SET(listenfd, &set);

    // tengo traccia del file descriptor con id piu' grande
    int fdmax = listenfd;



    for(;;){
        // copio il set nella variabile temporanea per la select
        tmpset = set;

        // From "Unix Network Programming: The Sockets Networking Api"
        // by R. Stevens et al.
        // The maxfdp1 (fdmax+1 ed) argument specifies the number of descriptors to be tested.
        // Its value is the maximum descriptor to be tested plus one (hence our
        // name of maxfdp1). The descriptors 0, 1, 2, up through and including
        // maxfdp1 1 are tested. The constant FD_SETSIZE, defined by including
        // <sys/select.h>, is the number of descriptors in the fd_set datatype.
        // Its value is often 1024, but few programs use that many descriptors.
        // The maxfdp1 argument forces us to calculate the largest descriptor that
        // we are interested in and then tell the kernel this value. For example,
        // given the previous code that turns on the indicators for descriptors
        // 1, 4, and 5, the maxfdp1 value is 6. The reason it is 6 and not 5 is
        // that we are specifying the number of descriptors, not the largest value,
        // and descriptors start at 0.
        if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) { // attenzione al +1
            perror("select");
            return -1;
        }



        for(;;) {
            // copio il set nella variabile temporanea per la select
            tmpset = set;

            // From "Unix Network Programming: The Sockets Networking Api"
            // by R. Stevens et al.
            // The maxfdp1 (fdmax+1 ed) argument specifies the number of descriptors to be tested.
            // Its value is the maximum descriptor to be tested plus one (hence our
            // name of maxfdp1). The descriptors 0, 1, 2, up through and including
            // maxfdp1 1 are tested. The constant FD_SETSIZE, defined by including
            // <sys/select.h>, is the number of descriptors in the fd_set datatype.
            // Its value is often 1024, but few programs use that many descriptors.
            // The maxfdp1 argument forces us to calculate the largest descriptor that
            // we are interested in and then tell the kernel this value. For example,
            // given the previous code that turns on the indicators for descriptors
            // 1, 4, and 5, the maxfdp1 value is 6. The reason it is 6 and not 5 is
            // that we are specifying the number of descriptors, not the largest value,
            // and descriptors start at 0.
            if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) { // attenzione al +1
                perror("select");
                return -1;
            }
            // cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
            for(int i=0; i <= fdmax; i++) {
                if (FD_ISSET(i, &tmpset)) {
                    long connfd;
                    if (i == listenfd) { // e' una nuova richiesta di connessione
                        SYSCALL_EXIT("accept", connfd, accept(listenfd, (struct sockaddr*)NULL ,NULL), "accept", "");
                        FD_SET(connfd, &set);  // aggiungo il descrittore al master set
                        if(connfd > fdmax) fdmax = connfd;  // ricalcolo il massimo
                        continue;
                    }
                    connfd = i;  // e' una nuova richiesta da un client già connesso


                    //TODO QUESTA E' LA PARTE CHE ANDRA' MODIFICATA CON IL MIO COMANDO

                    // eseguo il comando e se c'e' un errore lo tolgo dal master set
                    if (cmd(coda,connfd) < 0) {
                        close(connfd);
                        FD_CLR(connfd, &set);
                        // controllo se deve aggiornare il massimo
                        if (connfd == fdmax) fdmax = updatemax(set, fdmax);
                    }
                }
                //deleteQueue(coda);
                //print_list(coda);
            }
        }

        close(listenfd);
        return 0;
    }




    //Queue_t * coda;
    //coda = initQueue();

    //push(coda,  stringa, stringa);
    //push(coda,   stringa2, stringa2);
    //push(coda,  stringa3, stringa3);
    //push(coda,  stringa4, stringa4);
    //push(coda,  stringa5, stringa5);
    //push(coda,  stringa6, stringa6);

    //pushFile(coda, "test.txt", MAXSIZE);
    //pushFile(coda, "lol.txt", MAXSIZE);
    //pushFile(coda, "terzo.txt", MAXSIZE);

    //print_list(coda);

    //pop(coda);
    //print_list(coda);

    //pop(coda);
    //pop(coda);
    //print_list(coda);

    //char *str = readNFiles(0, "lol", coda);

    //printf("%s\n", str);

    //free(str);

    //pushFile(coda, "lol.txt",MAXSIZE);
    //pushFile(coda, "terzo.txt", MAXSIZE);

    //print_list(coda);

    //removeFile(coda, "cazzo.txt");

    //removeFile(coda, "terzo.txt");

    //print_list(coda);

    //print_list(coda);

    //printf("\n");
    //printf("lista dopo la rimozione:\n");
    //printf("\n");

    //pop(coda);
    //pop(coda);

    //print_list(coda);

    //push(coda, "cazzo", "nuova stringa");

    //print_list(coda, 'd');

    //removeFile(coda, "quarta");
    //printf("-+-+-+-+-+-+-+-+-+-+-\n");

    //print_list(coda);



    //printf("----------------------- FINE ---------------------------\n");

}


// TODO ---------------------------------	FINE MAIN		---------------------------------------------