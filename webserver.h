#include <sys/socket.h>
#include <time.h>
#include "rio_package.h"

#define MAXLINE 8192

typedef struct dirinfo{
    char *name;
    char *size;
    char *date;
}dirinfo;

void copyDirinfo(struct dirinfo src[], struct dirinfo dest[], int count);

typedef struct sockaddr SA;

int open_clientfd(char *hostname, int port);
int open_listenfd(int port);
void echo(int connfd);

void connectionHandler(int connfd, char *directory);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void get_filetype(char *filename, char *filetype);
void read_requesthdrs(rio_t *rp);

long fileSize(char *fname);
char *fileDate(struct dirent *ent);
void httpResponse();
dirinfo proccessFile(char *ruta, struct dirent *ent);
int proccessDirectory(char *dirstring, dirinfo *directionInfo);
int countDirectory(char *dir);