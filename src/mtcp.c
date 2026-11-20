/*
 * mtcp: spolehlivý proud bajtů nad UDP.  TOHLE JE TVŮJ SOUBOR.
 *
 * Nezapomeň: místo sendto() volej netsim_sendto() (viz support/netsim.h).
 * Postup po etapách najdeš v ZADANI.md.
 */

#include "mtcp.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static struct sockaddr_in get_listener_addr(uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    return addr;
}

static bool create_listener_socket(mtcp_listener *l) {
    l->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (l->fd < 0) {
        free(l);
        return false;
    }
    return true;
}

static bool bind_listener(mtcp_listener *l, uint16_t port) {
    struct sockaddr_in addr = get_listener_addr(port);
    
    if (bind(l->fd, (struct sockaddr*) &addr,sizeof addr) < 0) {
        close(l->fd);
        free(l);
        return false;
    }
    return true;
}

mtcp_listener *mtcp_listen(uint16_t port)
{
    mtcp_listener *listener = malloc(sizeof (struct mtcp_listener));
    if (listener == NULL) { perror("Malloc error."); return NULL; }

    if (!create_listener_socket(listener)) { perror("Socket error."); return NULL; }
    if (!bind_listener(listener, port)) { perror("Bind error."); return NULL; }

    return listener;
}

mtcp_conn *mtcp_accept(mtcp_listener *l)
{
    (void)l;
    /* TODO: čekej na SYN, zapamatuj si adresu klienta, odpověz SYN+ACK */
    return NULL;
}

void mtcp_listener_close(mtcp_listener *l)
{
    (void)l;
    /* TODO: close(fd) a free */
}

mtcp_conn *mtcp_connect(const char *ip, uint16_t port)
{
    (void)ip;
    (void)port;
    /* TODO: posílej SYN, dokud nepřijde SYN+ACK (max ~5 s) */
    return NULL;
}

ssize_t mtcp_send(mtcp_conn *c, const void *buf, size_t len)
{
    (void)c;
    (void)buf;
    (void)len;
    /* TODO: rozděl na pakety, každý opakuj, dokud není potvrzen */
    return -1;
}

ssize_t mtcp_recv(mtcp_conn *c, void *buf, size_t len)
{
    (void)c;
    (void)buf;
    (void)len;
    /* TODO */
    return -1;
}

int mtcp_close(mtcp_conn *c)
{
    (void)c;
    /* TODO: FIN, počkat na ACK, uvolnit paměť */
    return -1;
}
