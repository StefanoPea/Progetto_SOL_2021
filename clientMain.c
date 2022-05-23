// ---------------------------------	INIZIO INCLUDE				---------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>



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





// ---------------------------------	FINE INCLUDE				---------------------------------------------



// ---------------------------------	INIZIO MACRO				---------------------------------------------


#define SOCKNAME     "./cs_sock"
#define MAXBACKLOG   32
#define EXTRA_LEN_PRINT_ERROR   512
#define MAXSIZE 1024






#define SYSCALL_EXIT(name, r, sc, str, ...)	\
    if ((r=sc) == -1) {				\
	perror(#name);				\
	int errno_copy = errno;			\
	print_error(str, __VA_ARGS__);		\
	exit(errno_copy);			\
    }






// macro di utilità per controllare errori
#define CHECK_EQ_EXIT(X, val, str)	\
  if ((X)==val) {			\
    perror(#str);			\
    exit(EXIT_FAILURE);			\
  }
#define CHECK_NEQ_EXIT(X, val, str)	\
  if ((X)!=val) {			\
    perror(#str);			\
    exit(EXIT_FAILURE);			\
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


// funzione per tokenizzare una stringa e mettere le stringhe cosi' ottenute in  un array
void tokenizer_r(char *stringa, char* files[]) {
    char *tmpstr;
    char *token = strtok_r(stringa, ",", &tmpstr);
    int i = 0;
    while (token) {
        files[i] = token;
        printf("%s\n",files[i]);
        token = strtok_r(NULL, ",", &tmpstr);
        i++;
    }
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


// ---------------------------------	FINE FUNZIONI DI SUPPORTO		---------------------------------------------



// ---------------------------------	INIZIO CODA RICHIESTE CLIENT	---------------------------------------------

// ---------------------------------	FINE CODA RICHIESTE CLIENT		---------------------------------------------



// ---------------------------------	INIZIO PARSING COMANDI		    ---------------------------------------------

// esempio di comando help
int arg_h() {
    printf("funziona il -h\n");
    return 0;
}

// esempio di comando con parametro singolo necessaro
int arg_f(char* filename){
    printf("funziona anche il -f: il nome del file e' %s\n", filename);
    return 0;
}

// esempio di comando con parametri multipli necessari
int arg_F(char* filename){
    int i = 0;
    int numfiles = 0;
    while(filename[i] != '\0'){
        if(filename[i] == ','){
            numfiles++;
        }
        i++;
    }
    char** files =  malloc (numfiles * sizeof(char*));
    tokenizer_r(filename, files);
    return 0;
}

// esempio di comando con parametro singolo opzionale [va messo in fondo senno' non funziona]
int arg_R(){
    printf("non ho ricevuto argomenti per il comando -R\n");
    return 0;
}

int arg_R_opt(char* filename){
    printf("%s\n", filename);
    return 0;
}


void parseArgs(int argc, char *argv[]){

    int opt;
    while ((opt = getopt(argc,argv, ":hf:F:R:")) != -1) {
        switch(opt) {

            case 'h': arg_h(); exit(0);
            case 'f': arg_f(optarg); break;
            case 'F': arg_F(optarg); break;
            case 'R': arg_R_opt(optarg); break;
            case ':': {
                switch (optopt) {
                    case 'R':
                        arg_R();
                        break;
                    default:
                        fprintf(stderr, "option -%c is missing a required argument\n", optopt);
                        break;
                }
                break;
            }
            case '?': {  // restituito se getopt trova una opzione non riconosciuta
                printf("l'opzione '-%c' non e' gestita\n", optopt);
            } break;
            default: printf("default\n");
        }
    }
}



// ---------------------------------	FINE PARSING COMANDI			---------------------------------------------



// ---------------------------------	INIZIO MAIN CLIENT				---------------------------------------------



int main(int argc, char  *argv[]){

    if (argc == 1) {
        fprintf(stderr, "usa: %s stringa [stringa]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un serv_addr;
    int sockfd;
    SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);

    int notused;
    SYSCALL_EXIT("connect", notused, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");

    char *buffer=NULL;
    for(int i=1; i<argc;++i) {

        int n=strlen(argv[i])+1;
        //int n= MAXSIZE;
        /*
         *  NOTA: provare ad utilizzare writev (man 2 writev) per fare un'unica SC
         */
        SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
        SYSCALL_EXIT("writen", notused, writen(sockfd, argv[i], n*sizeof(char)), "write", "");


        SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read","");
        buffer = realloc(buffer, n*sizeof(char));
        //buffer = realloc(buffer, MAXSIZE);
        if (!buffer) {
            perror("realloc");
            fprintf(stderr, "Memoria esaurita....\n");
            break;
        }
        /*
         *  NOTA: provare ad utilizzare readv (man 2 readv) per fare un'unica SC
         */
        //printf("%s\n", buffer);

        SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n*sizeof(char)), "read","");

        //SYSCALL_EXIT("readn", notused, readn(sockfd, &n, MAXSIZE), "read","");
        //SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, MAXSIZE), "read","");

        printf("%d\n", n);
        buffer[n] = '\0';
        printf("result: %s\n", buffer);
    }
    close(sockfd);
    if (buffer) free(buffer);
    return 0;


    //parseArgs(argc, argv);

    //return 0;
}




// ---------------------------------	FINE MAIN CLIENT				---------------------------------------------
