//
// Created by aldairlfp on 12/30/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "webserver.h"

int parse_uri(char uri[8192], char filename[8192], char cgiargs[8192]);

int open_clientfd(char *hostname, int port) {
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; /* Check errno for cause of error */

    /* Fill in the server's IP address and port */
    if ((hp = gethostbyname(hostname)) == NULL)
        return -2; /* Check h_errno for cause of error */

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *) hp->h_addr_list[0], (char *) &serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    /* Establish a connection with the server */
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    return clientfd;
}

int open_listenfd(int port) {
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *) &optval, sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an end point for all requests to port
     * on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) port);
    if (bind(listenfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, 1024) < 0)
        return -1;

    return listenfd;
}

void echo(int connfd) {
    int n;
    char buf[1024];
    while ((n = read(connfd, buf, 1024)) != 0) {
        printf("server received %d bytes\n", n);
        printf("%s", buf);
    }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[1024], body[1024];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}

void get_filetype(char *filename, char *filetype) {
    if(strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if(strstr(filename, ".giff"))
        strcpy(filetype, "image/giff");
    else if(strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else strcpy(filetype, "text/plain");
}

void proccessDirectory(int connfd, char *directory) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE], body[MAXLINE];
    rio_t rio;

    rio_readinitb(&rio, connfd);
    rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET") != 0) {
        clienterror(connfd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }

    read_requesthdrs(&rio);

    /* Parse URI from GET request */
    parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
        clienterror(connfd, filename, "404", "Not found",
                    "Tiny couldn’t find this file");
        return;
    }

    char buff[MAXLINE];

    /* Build the HTTP response body */
    sprintf(body, "<html><head>Directorio %s</head></html>", directory);
//    sprintf(body, "%s<body>\r\n", body);
//    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
//    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
//    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);


    /* Print the HTTP response */
    sprintf(buff, "HTTP/1.0 %s %s\r\n", "200", "OK");
    rio_writen(connfd, buff, strlen(buff));
    sprintf(buff, "Content-type: text/html\r\n");
    rio_writen(connfd, buff, strlen(buff));
    sprintf(buff, "Content-length: %d\r\n\r\n", (int) strlen(body));
    rio_writen(connfd, buff, strlen(buff));
    rio_writen(connfd, body, strlen(body));
}

int parse_uri(char uri[8192], char filename[8192], char cgiargs[8192]) {
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {      /* Static content */
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri) - 1] == '/')
            strcat(filename, "home.html");
        return 1;
    } else {             /* Dynamic content */
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        } else
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];
    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n") != 0) {
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
}

long fileSize(char *fname) {
    FILE *fich;
    long ftam = -1;

    fich = fopen(fname, "r");
    fseek(fich, 0L, SEEK_END);
    ftam = ftell(fich);
    fclose(fich);

    return ftam;
}

void proccessFile(char *ruta, struct dirent *ent) {
    long ftam;
    char *nombrecompleto;
    char strtam[20];
    char *cuantity;

    int i;
    int tmp;
    unsigned char tipo;

    /* Sacamos el nombre completo con la ruta del archivo */
    tmp = strlen(ruta);
    nombrecompleto = malloc(
            tmp + strlen(ent->d_name) + 2); /* Sumamos 2, por el \0 y la barra de directorios (/) no sabemos si falta */
    if (ruta[tmp - 1] == '/')
        sprintf(nombrecompleto, "%s%s", ruta, ent->d_name);
    else
        sprintf(nombrecompleto, "%s/%s", ruta, ent->d_name);

    /* Calcula el tamaño */
    ftam = fileSize(nombrecompleto);
    tipo = ent->d_type;

    double res;
    if (tipo != DT_DIR) {
        if (ftam > 1073741824) {
            res = (double) ftam / 1024 / 1024 / 1024;
            cuantity = "G";
        } else if (ftam > 1048576 && ftam <= 1073741824) {
            res = (double) ftam / 1024 / 1024;
            cuantity = "M";
        } else if(ftam > 1024 && ftam <= 1048576){
            res = (double) ftam / 1024;
            cuantity = "k";
        }
        else {
            res = (double) ftam;
            cuantity = "B";
        }
    }
    else {
        res = 0;
        cuantity = "";
    }
    sprintf(strtam, "%f %s", res, cuantity);

    printf ("%30s (%s) \n", ent->d_name, strtam);

    free(nombrecompleto);
}

int main(int argc, char **argv) {
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *dirstring, *haddrp;
    char buf[256];

    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <dir>\n", argv[0]);
        exit(0);
    }

    port = atoi(argv[1]);
    dirstring = argv[2];

    printf("Listening on port: %i\n", port);
    printf("Servinf directory: %s\n", dirstring);

    chdir(dirstring);


///* Con un puntero a DIR abriremos el directorio */
//    DIR *dir;
//    /* en *ent habrá información sobre el archivo que se está "sacando" a cada momento */
//    struct dirent *ent;
//
////    if (argc != 2) {
////        error("Uso: ./directorio_2 <ruta>\n");
////    }
//    /* Empezaremos a leer en el directorio actual */
//    dir = opendir(".");
//
//    /* Miramos que no haya error */
//    if (dir == NULL)
//        printf("No puedo abrir el directorio\n");
//
//    /* Una vez nos aseguramos de que no hay error, ¡vamos a jugar! */
//    /* Leyendo uno a uno todos los archivos que hay */
//    while ((ent = readdir(dir)) != NULL) {
//        /* Nos devolverá el directorio actual (.) y el anterior (..), como hace ls */
//        if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)) {
//            /* Una vez tenemos el archivo, lo pasamos a una función para procesarlo. */
//            proccessFile(".", ent);
//        }
//    }
//    closedir(dir);


    listenfd = open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (SA *) &clientaddr, &clientlen);
        hp = gethostbyaddr((const char *) &clientaddr.sin_addr.s_addr,
                           sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("server connected to (%s)\n", haddrp);
        proccessDirectory(connfd, dirstring);
        close(connfd);
    }
    return 0;
}