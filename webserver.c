//
// Created by aldairlfp on 12/30/21.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <dirent.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "webserver.h"

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
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else
        strcpy(filetype, "text/plain");
}

void send_all(int socket, void* buffer, size_t length)
{
    char* ptr = (char*)buffer;
    while (length > 0)
    {
        int i = send(socket, ptr, length, 0);

        if (i < 1) return;
        ptr += i;
        length -= i;
    }
}

void connectionHandler(int connfd, char* directory) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char* filename, args[MAXLINE];
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

    char orderby[MAXLINE], order[MAXLINE];

    /* Parse URI from GET request */
    filename = parse_uri(&uri, &orderby, &order);

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
            srcfd = open(newDir, O_RDONLY, 0);
            srcp = mmap(0, sbuf.st_size, PROT_READ, MAP_SHARED, srcfd, 0);
            // close(srcfd);
            sendfile(connfd, srcfd, NULL, sbuf.st_size);
            // send_all(connfd, srcp, sbuf.st_size);
            // rio_writen(connfd, srcp, sbuf.st_size);
            close(srcfd);
            munmap(srcp, sbuf.st_size);
        }
    }
    else {
        if (strcmp(uri, "/favicon.ico") != 0) {
            int countdir;
            countdir = countDirectory(newDir);
            char body[countdir * 1024];

            char* names[countdir];
            char* sizes[countdir];
            char* dates[countdir];

            for (int i = 0; i < countdir; i++)
            {
                names[i] = (char*)malloc(sizeof(char) * 50);
                sizes[i] = (char*)malloc(sizeof(char) * 50);
                dates[i] = (char*)malloc(sizeof(char) * 50);
            }
            proccessDirectory(newDir, countdir, &names, &sizes, &dates);

            /* Build the HTTP response body */
            sprintf(body, "<html><head>Directorio %s%s</head>\r\n", directory, uri);
            sprintf(body, "%s<body><table>\r\n", body);
            sprintf(body, "%s<tr><th><a href='%s?order=ascending'>Name</a></th>", body, uri);
            sprintf(body, "%s<th>Size</th>", body);
            sprintf(body, "%s<th>Date</th></tr>", body);
            for (int i = 0; i < countdir; i++)
            {
                sprintf(body, "%s<tr>", body);
                if (strcmp(newDir, "./") == 0)
                    sprintf(body, "%s<td><a href='%s%s'>%s</a></td>", body, newDir, names[i], names[i]);
                else {
                    sprintf(body, "%s<td><a href='%s/%s'>%s</a></td>", body, filename, names[i], names[i]);
                }
                sprintf(body, "%s<td>%s</td>", body, sizes[i]);
                sprintf(body, "%s<td>%s</td>", body, dates[i]);
                sprintf(body, "%s</tr>", body);
            }
            sprintf(body, "%s</table></body></html>", body);

            /* Print the HTTP response */
            sprintf(buff, "HTTP/1.0 %s %s\r\n", "200", "OK");
            rio_writen(connfd, buff, strlen(buff));
            sprintf(buff, "Content-type: text/html\r\n");
            rio_writen(connfd, buff, strlen(buff));
            sprintf(buff, "Content-length: %d\r\n\r\n", (int)strlen(body));
            rio_writen(connfd, buff, strlen(buff));
            rio_writen(connfd, body, strlen(body));
            free(names);
            free(sizes);
            free(dates);
        }
    }
    filename = NULL;
    free(filename);
    free(newDir);
}

char* parse_uri(char* ruta, char* orderby, char* order) {
    int indexLastSlash = 0;
    int startParams = 0;
    char* tmpRute = malloc(sizeof(ruta));
    int k = 0;
    *tmpRute = '.';
    for (int i = 0; i < strlen(ruta); i++)
    {
        if (*(ruta + i) == '/')
            indexLastSlash = i;
        if (*(ruta + i) == '?')
            startParams = i + 1;
        if (startParams != 0)
            *(tmpRute + k++) = *(ruta + i + 1);
    }
    orderby = strchr(tmpRute, '=') + 1;
    order = strrchr(tmpRute, '=') + 1;
    // sscanf(tmpRute, "ORDER_BY=%s&ORDER=%s", orderby, order);
    free(tmpRute);
    char* rute;
    rute = malloc(strlen(ruta) + 1);
    *rute = '.';
    k = 1;
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

char* fileSize(char* fname, unsigned char type) {
    FILE* fich;
    long ftam = -1;
    char* cuantity;

    fich = fopen(fname, "r");
    fseek(fich, 0L, SEEK_END);
    ftam = ftell(fich);
    fclose(fich);

    double res;
    if (!S_ISDIR(type)) {
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
    char* size;
    char* resnum;
    asprintf(&resnum, "%f", res);
    size = strcat(resnum, cuantity);
    return size;
}

int proccessDirectory(char* dirstring, int count, char** names, char** sizes, char** dates) {
    int k = 0;
    /* Con un puntero a DIR abriremos el directorio */
    DIR* dir;
    /* en *ent habrá información sobre el archivo que se está "sacando" a cada momento */
    struct dirent* ent;

    /* Empezaremos a leer en el directorio actual */
    dir = opendir(dirstring);

    /* Miramos que no haya error */
    if (dir == NULL) {
        return -1;
    }

    char* filename;
    int tmp;

    /* Una vez nos aseguramos de que no hay error, ¡vamos a jugar! */
    /* Leyendo uno a uno todos los archivos que hay */
    while ((ent = readdir(dir)) != NULL) {
        /* Nos devolverá el directorio actual (.) y el anterior (..), como hace ls */
        if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)) {
            /* Una vez tenemos el archivo, lo pasamos a una función para procesarlo. */
            strcpy(names[k], ent->d_name);
            /* Sacamos el nombre completo con la ruta del archivo */
            tmp = strlen(dirstring);
            filename = malloc(
                tmp + strlen(ent->d_name) + 2); /* Sumamos 2, por el \0 y la barra de directorios (/) no sabemos si falta */
            if (dirstring[tmp - 1] == '/')
                sprintf(filename, "%s%s", dirstring, ent->d_name);
            else
                sprintf(filename, "%s/%s", dirstring, ent->d_name);
            char* size = fileSize(filename, ent->d_type);
            strcpy(sizes[k], size);
            char* date = fileDate(ent);
            strcpy(dates[k], date);

            k++;
        }
    }
    filename = NULL;
    free(filename);
    closedir(dir);
    return 0;
}

int countDirectory(char* dirstring) {
    int countdir = 0;

    /* Con un puntero a DIR abriremos el directorio */
    DIR* dir;
    /* en *ent habrá información sobre el archivo que se está "sacando" a cada momento */
    struct dirent* ent;

    /* Empezaremos a leer en el directorio actual */
    dir = opendir(dirstring);

    /* Miramos que no haya error */
    if (dir == NULL) {
        return -1;
    }

    /* Una vez nos aseguramos de que no hay error, ¡vamos a jugar! */
    /* Leyendo uno a uno todos los archivos que hay */
    while ((ent = readdir(dir)) != NULL) {
        /* Nos devolverá el directorio actual (.) y el anterior (..), como hace ls */
        if ((strcmp(ent->d_name, ".") != 0) && (strcmp(ent->d_name, "..") != 0)) {
            /* Una vez tenemos el archivo, lo pasamos a una función para procesarlo. */
            countdir++;
        }
    }
    closedir(dir);
    return countdir;
}

char* fileDate(struct dirent* ent) {
    char* time;
    struct stat buf;

    stat(ent->d_name, &buf);
    time = (char*)ctime(&buf.st_mtime);
    return time;
}

void sigchld_handler(int sig)
{
    while (waitpid(-1, 0, WNOHANG) > 0)
        ;
    return;
}

int main(int argc, char** argv) {
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    struct hostent* hp;
    char* dirstring, * haddrp;
    char buf[256];
    orderName = name;
    orderState = descending;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <dir>\n", argv[0]);
        exit(0);
    }

    port = atoi(argv[1]);
    dirstring = argv[2];

    printf("Listening on port: %i\n", port);
    printf("Servinf directory: %s\n", dirstring);

    chdir(dirstring);

    signal(SIGCHLD, sigchld_handler);
    listenfd = open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (SA*)&clientaddr, &clientlen);
        int pid;
        // if ((pid = fork()) == 0) {
        close(listenfd);
        hp = gethostbyaddr((const char*)&clientaddr.sin_addr.s_addr,
            sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        haddrp = inet_ntoa(clientaddr.sin_addr);
        connectionHandler(connfd, dirstring);
        //     exit(0);
        // }
        close(connfd);
    }
    return 0;
}