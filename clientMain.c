// ---------------------------------	INIZIO INCLUDE				---------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

// ---------------------------------	FINE INCLUDE				---------------------------------------------



// ---------------------------------	INIZIO MACRO				---------------------------------------------

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

    parseArgs(argc, argv);
    /* code */
    return 0;
}




// ---------------------------------	FINE MAIN CLIENT				---------------------------------------------
