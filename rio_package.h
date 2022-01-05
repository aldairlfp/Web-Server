//
// Created by aldairlfp on 1/4/22.
//
#ifndef WEB_SERVER_RIO_PACKAGE_H
#define WEB_SERVER_RIO_PACKAGE_H

#include <stdio.h>

#define RIO_BUFSIZE 8192

typedef struct {
    int rio_fd;                 /* Descriptor for this internal buf */
    int rio_cnt;                /* Unread bytes in internal buf */
    char *rio_bufptr;           /* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE];  /* Internal buffer */
} rio_t;

/** The rio_readn function transfers up to n bytes
 * from the current ﬁle position of descriptor fd
 * to memory location usrbuf */
ssize_t rio_readn(int fd, void *usrbuf, size_t n);

/** The rio_writen function transfers up
 * to n bytes from memory location usrbuf
 * to the current ﬁle position of descriptor fd */
ssize_t rio_writen(int fd, void *usrbuf, size_t n);

/** It associates
the descriptor fd with a read buffer of type rio_t at address rp */
void rio_readinitb(rio_t *rp, int fd);

/** Reads the next text line from ﬁle rp (including
 * the terminating newline character), copies it to
 * memory location usrbuf, and terminates the text line
 * with the null (zero) character */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

/** Reads up to n bytes from ﬁle rp to memory location usrbuf */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);

/** The rio_read function is a buffered version of the Unix read function.
 * When rio_read is called with a request to read n bytes, there are rp->rio_cnt
 * unread bytes in the read buffer. If the buffer is empty, then it is replenished with
 * a call to read. Receiving a short count from this invocation of read is not an error,
 * and simply has the effect of partially ﬁlling the read buffer. Once the buffer is
 * nonempty, rio_read copies the minimum of n and rp->rio_cnt bytes from the
 * read buffer to the user buffer and returns the number of bytes copied*/
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);

#endif //WEB_SERVER_RIO_PACKAGE_H
