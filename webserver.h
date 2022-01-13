#include <sys/socket.h>
#include <time.h>
#include "rio_package.h"

#define MAXLINE 8192

char orderState[MAXLINE];
char orderName[MAXLINE];

typedef struct sockaddr SA;

int open_clientfd(char* hostname, int port);
int open_listenfd(int port);
void echo(int connfd);

void connectionHandler(int connfd, char* directory);
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg);
void get_filetype(char* filename, char* filetype);
void read_requesthdrs(rio_t* rp);

char* parse_uri(char* ruta);
char* fileSize(char* fname, unsigned char type);
char* fileDate(struct dirent* ent);
void httpResponse();
int proccessDirectory(char* dirstring, int count, char** names, char** sizes, char** dates);
int countDirectory(char* dir);

int *sortDir(char **array, int count, char *state);