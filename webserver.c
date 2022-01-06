//
// Created by aldairlfp on 12/30/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "webserver.h"

char* parse_uri(char* ruta);

int open_clientfd(char* hostname, int port) {
    int clientfd;
    struct hostent* hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1; /* Check errno for cause of error */

    /* Fill in the server's IP address and port */
    if ((hp = gethostbyname(hostname)) == NULL)
        return -2; /* Check h_errno for cause of error */

    bzero((char*)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char*)hp->h_addr_list[0], (char*)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    /* Establish a connection with the server */
    if (connect(clientfd, (SA*)&serveraddr, sizeof(serveraddr)) < 0)
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
        (const void*)&optval, sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an end point for all requests to port
     * on any IP address for this host */
    bzero((char*)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (SA*)&serveraddr, sizeof(serveraddr)) < 0)
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

void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg) {
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

void get_filetype(char* filename, char* filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".giff"))
        strcpy(filetype, "image/giff");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".rar"))
        strcpy(filetype, "application/vnd.rar");
    else
        strcpy(filetype, "text/plain");
}

void connectionHandler(int connfd, char* directory) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char* filename, cgiargs[MAXLINE], body[MAXLINE];
    char* newDir = (char*)malloc(sizeof(char) * MAXLINE);
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
    filename = parse_uri(uri);

    char buff[MAXLINE];
    strcpy(newDir, ".");
    newDir = strcat(newDir, uri);

    if (stat(newDir, &sbuf) < 0) {
        clienterror(connfd, filename, "404", "Not found",
            "Tiny couldn’t find this file");
        return;
    }

    if (!S_ISDIR(sbuf.st_mode)) {

        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(connfd, filename, "403", "Forbidden",
                "Tiny couldn’t read the file");
            return;
        }
        else {
            char filetype[MAXLINE];
            /* Send response headers to client */
            get_filetype(filename, filetype);
            sprintf(buff, "HTTP/1.0 200 OK\r\n");
            sprintf(buff, "%sServer: Tiny Web Server\r\n", buff);
            sprintf(buff, "%sContent-length: %d\r\n", buff, sbuf.st_size);
            sprintf(buff, "%sContent-type: %s\r\n\r\n", buff, filetype);
            rio_writen(connfd, buff, strlen(buff));

            /* Send the file to download */
            int srcfd;
            char* srcp;
            srcfd = open(filename, O_RDONLY, 0);
            srcp = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
            close(srcfd);
            rio_writen(connfd, srcp, sbuf.st_size);
            waitpid(-1, NULL, 0);
            munmap(srcp, sbuf.st_size);
        }
    }
    else {
        if (strcmp(uri, "/favicon.ico") != 0) {
            /* Build the HTTP response body */
            sprintf(body, "<html><head>Directorio %s%s</head>\r\n", directory, uri);
            sprintf(body, "%s<body><table>\r\n", body);
            sprintf(body, "%s<tr><th>Name</th><th>Size</th><th>Date</th></tr>", body);
            proccessDirectory(newDir, body);
            sprintf(body, "%s</table></body></html>", body);

            /* Print the HTTP response */
            sprintf(buff, "HTTP/1.0 %s %s\r\n", "200", "OK");
            rio_writen(connfd, buff, strlen(buff));
            sprintf(buff, "Content-type: text/html\r\n");
            rio_writen(connfd, buff, strlen(buff));
            sprintf(buff, "Content-length: %d\r\n\r\n", (int)strlen(body));
            rio_writen(connfd, buff, strlen(buff));
            rio_writen(connfd, body, strlen(body));
        }
    }
    filename = NULL;
    free(filename);
    free(newDir);
}

char* parse_uri(char* ruta) {
    int indexLastSlash = 0;
    for (int i = 0; i < strlen(ruta); i++)
    {
        if (*(ruta + i) == '/')
            indexLastSlash = i;
    }
    char* rute;
    rute = malloc(strlen(ruta) + 1);
    *rute = '.';
    int k = 1;
    for (int i = indexLastSlash; i < strlen(ruta); i++)
    {
        *(rute + k) = *(ruta + i);
        k++;
    }
    *(rute + k) = '\0';
    return rute;
}

void read_requesthdrs(rio_t* rp) {
    char buf[MAXLINE];
    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n") != 0) {
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
}

long fileSize(char* fname) {
    FILE* fich;
    long ftam = -1;

    fich = fopen(fname, "r");
    fseek(fich, 0L, SEEK_END);
    ftam = ftell(fich);
    fclose(fich);
    return ftam;
}

void proccessFile(char* ruta, struct dirent* ent, char* body) {
    long ftam;
    char* nombrecompleto;
    char strtam[20];
    char* cuantity;
    char* fDate;

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
    fDate = fileDate(ent);
    tipo = ent->d_type;

    double res;
    if (tipo != DT_DIR) {
        if (ftam > 1073741824) {
            res = (double)ftam / 1024 / 1024 / 1024;
            cuantity = "G";
        }
        else if (ftam > 1048576 && ftam <= 1073741824) {
            res = (double)ftam / 1024 / 1024;
            cuantity = "M";
        }
        else if (ftam > 1024 && ftam <= 1048576) {
            res = (double)ftam / 1024;
            cuantity = "k";
        }
        else {
            res = (double)ftam;
            cuantity = "B";
        }
    }
    else {
        res = 0;
        cuantity = "";
    }
    sprintf(body, "%s<tr>", body);
    if (strcmp(ruta, "./") == 0)
        sprintf(body, "%s<td><a href='%s%s'>%s</a></td>", body, ruta, ent->d_name, ent->d_name);
    else {


        sprintf(body, "%s<td><a href='%s/%s'>%s</a></td>", body, parse_uri(ruta), ent->d_name, ent->d_name);
    }
    sprintf(body, "%s<td>%f%s</td>", body, res, cuantity);
    sprintf(body, "%s<td>%s</td>", body, fDate);
    sprintf(body, "%s</tr>", body);
    //    sprintf(strtam, "%f %s", res, cuantity);

    //    printf ("%30s (%s) \n", ent->d_name, strtam);

    free(nombrecompleto);
}

void proccessDirectory(char* dirstring, char* body) {
    /* Con un puntero a DIR abriremos el directorio */
    DIR* dir;
    /* en *ent habrá información sobre el archivo que se está "sacando" a cada momento */
    struct dirent* ent;

    /* Empezaremos a leer en el directorio actual */
    dir = opendir(dirstring);

    /* Miramos que no haya error */
    if (dir == NULL) {
        printf("No puedo abrir el directorio\n");
        return;
    }

    /* Una vez nos aseguramos de que no hay error, ¡vamos a jugar! */
    /* Leyendo uno a uno todos los archivos que hay */
    while ((ent = readdir(dir)) != NULL) {
        /* Nos devolverá el directorio actual (.) y el anterior (..), como hace ls */
        if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)) {
            /* Una vez tenemos el archivo, lo pasamos a una función para procesarlo. */
            proccessFile(dirstring, ent, body);
        }
    }
    closedir(dir);
}

char* fileDate(struct dirent* ent) {
    char* time;
    struct stat buf;

    stat(ent->d_name, &buf);
    time = (char*)ctime(&buf.st_mtime);
    return time;
}

int main(int argc, char** argv) {
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    struct hostent* hp;
    char* dirstring, * haddrp;
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

    listenfd = open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (SA*)&clientaddr, &clientlen);
        hp = gethostbyaddr((const char*)&clientaddr.sin_addr.s_addr,
            sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("server connected to (%s)\n", haddrp);
        // echo(connfd);
        connectionHandler(connfd, dirstring);
        close(connfd);
    }
    return 0;
}