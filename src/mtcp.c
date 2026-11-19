/*
 * mtcp: spolehlivý proud bajtů nad UDP.  TOHLE JE TVŮJ SOUBOR.
 *
 * Nezapomeň: místo sendto() volej netsim_sendto() (viz support/netsim.h).
 * Postup po etapách najdeš v ZADANI.md.
 */

#include "mtcp.h"
#include "netsim.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

struct mtcp_listener {
    int fd;
    /* TODO */
};

struct mtcp_conn {
    int fd;
    /* TODO: adresa protistrany, čísla sekvence, buffer přijatých dat, ... */
};

mtcp_listener *mtcp_listen(uint16_t port)
{
    (void)port;
    /* TODO: socket(AF_INET, SOCK_DGRAM, 0), SO_REUSEADDR, bind */
    return NULL;
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
