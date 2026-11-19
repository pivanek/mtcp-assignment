#ifndef NETSIM_H
#define NETSIM_H

/*
 * Simulátor špatné sítě. Je to hotová pomůcka, nemusíš ji upravovat.
 *
 * Místo sendto() VŽDY volej netsim_sendto() (stejné argumenty bez `flags`).
 * Ve výchozím stavu všechno jen přepošle. Testy z něj udělají nespolehlivou
 * síť: pakety ztrácí, duplikuje, poškozuje a prohazuje.
 */

#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>

/* pravděpodobnosti jsou 0.0 až 1.0 */
void netsim_configure(double loss, double dup, double corrupt, double reorder,
                      unsigned long long seed);

ssize_t netsim_sendto(int fd, const void *buf, size_t len,
                      const struct sockaddr *to, socklen_t tolen);

#endif
