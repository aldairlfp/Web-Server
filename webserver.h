#include <sys/socket.h>
#include "rio_package.h"

#define MAXLINE 8192

typedef struct sockaddr SA;

int open_clientfd(char *hostname, int port);
int open_listenfd(int port);
void echo(int connfd);

void proccessDirectory(int connfd, char *directory);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void get_filetype(char *filename, char *filetype);
void read_requesthdrs(rio_t *rp);

long fileSize(char *fname);
void httpResponse();
void proccessFile(char *ruta, struct dirent *ent);
