#include <stdint.h>
#include <sys/socket.h>

#define BUF_SIZE 512

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

typedef enum {
    SYN = 1,
    ACK = 2,
    FIN = 4,
    DATA = 8
} flags;

typedef struct hdr {
    uint8_t  flags;   // SYN=1, ACK=2, FIN=4, DATA=8
    uint8_t  pad;
    uint16_t len;     // délka dat za hlavičkou
    uint32_t seq;     // číslo odesílaného paketu (nebo bajtu)
    uint32_t ack;     // co očekávám jako další
    uint16_t csum;    // kontrolní součet celého datagramu
} pckt_header;

typedef struct mtcp_listener {
    int fd;
    char buf[BUF_SIZE];

} mtcp_listener;

typedef struct mtcp_conn {
    int fd;
    sockaddr client_addr;
    socklen_t client_addr_size;
    /* TODO: adresa protistrany, čísla sekvence, buffer přijatých dat, ... */
} mtcp_conn;

mtcp_listener *mtcp_listen(uint16_t port);

mtcp_conn *mtcp_accept(mtcp_listener *l);

void mtcp_listener_close(mtcp_listener *l);

mtcp_conn *mtcp_connect(const char *ip, uint16_t port);

ssize_t mtcp_send(mtcp_conn *c, const void *buf, size_t len);

ssize_t mtcp_recv(mtcp_conn *c, void *buf, size_t len);

int mtcp_close(mtcp_conn *c);
