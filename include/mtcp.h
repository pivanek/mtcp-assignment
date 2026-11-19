#ifndef MTCP_H
#define MTCP_H

/*
 * mtcp = "mini TCP": spolehlivý proud bajtů nad nespolehlivým UDP.
 * Toto rozhraní NEMĚŇ, testy se na něj spoléhají.
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct mtcp_listener mtcp_listener;
typedef struct mtcp_conn mtcp_conn;

/* Server: vytvoří UDP socket navázaný na port (SO_REUSEADDR).
 * Vrací NULL při chybě. */
mtcp_listener *mtcp_listen(uint16_t port);

/* Server: čeká, až se připojí klient (handshake), a vrátí spojení.
 * Podporuje jen JEDNO spojení na listener. Blokuje, dokud někdo nepřijde. */
mtcp_conn *mtcp_accept(mtcp_listener *l);

/* Zavře listener (jen jeho vlastní popisovač souboru). Spojení vrácená
 * z mtcp_accept tím zavřená nejsou. */
void mtcp_listener_close(mtcp_listener *l);

/* Klient: naváže spojení se serverem. Handshake se opakuje při ztrátě
 * paketů; po ~5 s bez odpovědi vrátí NULL. */
mtcp_conn *mtcp_connect(const char *ip, uint16_t port);

/* Pošle CELÝ buffer (i když je větší než jeden paket). Blokuje, dokud není
 * vše potvrzeno druhou stranou. Vrací len, při chybě -1. */
ssize_t mtcp_send(mtcp_conn *c, const void *buf, size_t len);

/* Přečte nejvýše len bajtů (aspoň 1, pokud nějaké přišly), blokuje, dokud
 * nejsou k dispozici data. Vrací počet přečtených bajtů, 0 když druhá
 * strana zavřela spojení a všechna data už byla přečtena, -1 při chybě.
 * Bajty musí přijít ve stejném pořadí, v jakém byly odeslány. */
ssize_t mtcp_recv(mtcp_conn *c, void *buf, size_t len);

/* Korektně ukončí spojení (pošle FIN, počká na potvrzení) a uvolní
 * veškerou paměť. Vrací 0, při chybě -1. */
int mtcp_close(mtcp_conn *c);

#endif
