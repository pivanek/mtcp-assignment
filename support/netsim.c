#include "netsim.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#define NETSIM_MAX 2048

static double p_loss, p_dup, p_corrupt, p_reorder;
static uint64_t rng = 88172645463325252ULL;

static int has_held;
static int held_fd;
static uint8_t held_buf[NETSIM_MAX];
static size_t held_len;
static struct sockaddr_storage held_to;
static socklen_t held_tolen;

static uint64_t next_rand(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}

static double rnd(void)
{
    return (double)(next_rand() >> 11) / 9007199254740992.0;
}

void netsim_configure(double loss, double dup, double corrupt, double reorder,
                      unsigned long long seed)
{
    p_loss = loss;
    p_dup = dup;
    p_corrupt = corrupt;
    p_reorder = reorder;
    rng = (seed * 0x9E3779B97F4A7C15ULL) | 1ULL;
    has_held = 0;
}

ssize_t netsim_sendto(int fd, const void *buf, size_t len,
                      const struct sockaddr *to, socklen_t tolen)
{
    uint8_t tmp[NETSIM_MAX];

    if (len > NETSIM_MAX || tolen > sizeof held_to) {
        errno = EMSGSIZE;
        return -1;
    }

    /* ztráta: paket se prostě nepošle */
    if (p_loss > 0 && rnd() < p_loss)
        return (ssize_t)len;

    memcpy(tmp, buf, len);

    /* poškození: změní se jeden náhodný bajt */
    if (p_corrupt > 0 && len > 0 && rnd() < p_corrupt)
        tmp[next_rand() % len] ^= (uint8_t)(1 + next_rand() % 255);

    int copies = (p_dup > 0 && rnd() < p_dup) ? 2 : 1;

    /* prohození pořadí: paket se zdrží a odešle až po dalším */
    if (p_reorder > 0 && !has_held && rnd() < p_reorder) {
        memcpy(held_buf, tmp, len);
        held_len = len;
        held_fd = fd;
        memcpy(&held_to, to, tolen);
        held_tolen = tolen;
        has_held = 1;
        return (ssize_t)len;
    }

    for (int i = 0; i < copies; i++)
        sendto(fd, tmp, len, 0, to, tolen);

    if (has_held) {
        sendto(held_fd, held_buf, held_len, 0,
               (struct sockaddr *)&held_to, held_tolen);
        has_held = 0;
    }
    return (ssize_t)len;
}
