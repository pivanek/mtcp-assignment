/*
 * Testy pro mtcp. Každý test běží ve vlastním procesu a má tři role:
 *   - testovací proces (řídí průběh a hlídá časový limit),
 *   - server (dítě, volá mtcp_accept),
 *   - klient (testovací proces, volá mtcp_connect).
 * Síť mezi nimi je simulovaná přes netsim (ztráty, duplikáty, poškození,
 * prohození pořadí).
 *
 * Spuštění:  ./test_mtcp        (všechny testy)
 *            ./test_mtcp 4      (jen test číslo 4)
 */

#define _GNU_SOURCE
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "mtcp.h"
#include "netsim.h"

#define TEST_TIMEOUT_S 30

typedef struct {
    const char *name;
    double loss, dup, corrupt, reorder; /* simulace sítě */
    size_t total;                       /* kolik bajtů se přenáší */
    size_t chunk;                       /* po kolika bajtech klient volá send */
    size_t rbuf;                        /* velikost bufferu pro recv */
    double max_seconds;                 /* 0 = neměřit */
} cfg_t;

typedef int (*role_fn)(mtcp_conn *c, const cfg_t *cfg);

static pid_t g_server_pid;

static void on_alarm(int sig)
{
    (void)sig;
    if (g_server_pid > 0)
        kill(g_server_pid, SIGKILL);
    dprintf(2, "\n    chyba: vyprsel casovy limit %d s (zacyklene nebo zablokovane volani?)\n",
            TEST_TIMEOUT_S);
    _exit(124);
}

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            fprintf(stderr, "\n    chyba: " __VA_ARGS__);                     \
            fprintf(stderr, "  (%s:%d)\n", __FILE__, __LINE__);               \
            return 1;                                                         \
        }                                                                     \
    } while (0)

/* ---------- deterministická testovací data ---------- */

static uint8_t pat_byte(uint64_t seed, uint64_t i)
{
    uint64_t z = seed + i * 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return (uint8_t)((z ^ (z >> 31)) >> 24);
}

static void fill(uint8_t *b, size_t n, uint64_t off)
{
    for (size_t i = 0; i < n; i++)
        b[i] = pat_byte(12345, off + i);
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ---------- společné stavební bloky ---------- */

/* pošle `total` bajtů vzorku po kouscích `chunk` */
static int send_pattern(mtcp_conn *c, size_t total, size_t chunk)
{
    uint8_t *buf = malloc(chunk);
    CHECK(buf != NULL, "malloc");
    size_t off = 0;
    while (off < total) {
        size_t n = total - off < chunk ? total - off : chunk;
        fill(buf, n, off);
        ssize_t r = mtcp_send(c, buf, n);
        CHECK(r == (ssize_t)n, "mtcp_send(%zu) vratil %zd", n, r);
        off += n;
    }
    free(buf);
    return 0;
}

/* čte, dokud recv nevrátí 0; kontroluje obsah i celkovou délku */
static int recv_pattern_to_eof(mtcp_conn *c, size_t total, size_t rbuf)
{
    uint8_t *buf = malloc(rbuf), *exp = malloc(rbuf);
    CHECK(buf && exp, "malloc");
    size_t off = 0;
    for (;;) {
        ssize_t r = mtcp_recv(c, buf, rbuf);
        CHECK(r >= 0, "mtcp_recv vratil chybu (%zd)", r);
        if (r == 0)
            break;
        CHECK(off + (size_t)r <= total, "prijato vic dat nez ma byt (%zu > %zu)",
              off + (size_t)r, total);
        fill(exp, (size_t)r, off);
        CHECK(memcmp(buf, exp, (size_t)r) == 0,
              "data se lisi (chybny obsah nebo poradi) na pozici %zu", off);
        off += (size_t)r;
    }
    CHECK(off == total, "prijato %zu B, ocekavano %zu B", off, total);
    free(buf);
    free(exp);
    return 0;
}

/* přečte přesně n bajtů (recv smí vracet méně) */
static int recv_exact(mtcp_conn *c, void *dst, size_t n)
{
    uint8_t *p = dst;
    size_t got = 0;
    while (got < n) {
        ssize_t r = mtcp_recv(c, p + got, n - got);
        CHECK(r > 0, "mtcp_recv vratil %zd, ale chybi jeste %zu B", r, n - got);
        got += (size_t)r;
    }
    return 0;
}

/* ---------- role pro jednotlivé testy ---------- */

static int server_expect_eof(mtcp_conn *c, const cfg_t *cfg)
{
    (void)cfg;
    char b[16];
    ssize_t r = mtcp_recv(c, b, sizeof b);
    CHECK(r == 0, "server mel dostat 0 (zavreni), dostal %zd", r);
    CHECK(mtcp_close(c) == 0, "mtcp_close selhal");
    return 0;
}

static int client_close_only(mtcp_conn *c, const cfg_t *cfg)
{
    (void)cfg;
    CHECK(mtcp_close(c) == 0, "mtcp_close selhal");
    return 0;
}

static int server_hello(mtcp_conn *c, const cfg_t *cfg)
{
    (void)cfg;
    char b[10];
    if (recv_exact(c, b, 10))
        return 1;
    CHECK(memcmp(b, "ahoj svete", 10) == 0, "server dostal jina data");
    CHECK(mtcp_send(c, "pong", 4) == 4, "server: send pong");
    return server_expect_eof(c, cfg);
}

static int client_hello(mtcp_conn *c, const cfg_t *cfg)
{
    (void)cfg;
    CHECK(mtcp_send(c, "ahoj svete", 10) == 10, "klient: send");
    char b[4];
    if (recv_exact(c, b, 4))
        return 1;
    CHECK(memcmp(b, "pong", 4) == 0, "klient dostal jina data nez pong");
    CHECK(mtcp_close(c) == 0, "mtcp_close selhal");
    return 0;
}

static int server_sink(mtcp_conn *c, const cfg_t *cfg)
{
    if (recv_pattern_to_eof(c, cfg->total, cfg->rbuf))
        return 1;
    CHECK(mtcp_close(c) == 0, "mtcp_close selhal");
    return 0;
}

static int client_source(mtcp_conn *c, const cfg_t *cfg)
{
    double t0 = now_s();
    if (send_pattern(c, cfg->total, cfg->chunk))
        return 1;
    CHECK(mtcp_close(c) == 0, "mtcp_close selhal");
    double dt = now_s() - t0;
    if (cfg->max_seconds > 0)
        CHECK(dt < cfg->max_seconds, "prenos trval %.2f s, limit je %.1f s", dt,
              cfg->max_seconds);
    return 0;
}

static int server_echo(mtcp_conn *c, const cfg_t *cfg)
{
    (void)cfg;
    uint8_t buf[4096];
    for (;;) {
        ssize_t r = mtcp_recv(c, buf, sizeof buf);
        CHECK(r >= 0, "server: mtcp_recv chyba");
        if (r == 0)
            break;
        CHECK(mtcp_send(c, buf, (size_t)r) == r, "server: send echa");
    }
    CHECK(mtcp_close(c) == 0, "mtcp_close selhal");
    return 0;
}

static int client_echo(mtcp_conn *c, const cfg_t *cfg)
{
    uint8_t *out = malloc(cfg->chunk), *in = malloc(cfg->chunk);
    CHECK(out && in, "malloc");
    size_t off = 0;
    while (off < cfg->total) {
        size_t n = cfg->total - off < cfg->chunk ? cfg->total - off : cfg->chunk;
        fill(out, n, off);
        CHECK(mtcp_send(c, out, n) == (ssize_t)n, "klient: send");
        if (recv_exact(c, in, n))
            return 1;
        CHECK(memcmp(out, in, n) == 0, "echo se lisi na pozici %zu", off);
        off += n;
    }
    CHECK(mtcp_close(c) == 0, "mtcp_close selhal");
    free(out);
    free(in);
    return 0;
}

/* ---------- spuštění jednoho testu ---------- */

static int run_one(int idx, const cfg_t *cfg, role_fn srv, role_fn cli)
{
    fflush(NULL);
    pid_t tp = fork();
    if (tp < 0) {
        perror("fork");
        return 1;
    }
    if (tp == 0) {
        signal(SIGALRM, on_alarm);
        alarm(TEST_TIMEOUT_S);
        uint16_t port = (uint16_t)(41000 + idx);

        mtcp_listener *l = mtcp_listen(port);
        if (!l) {
            fprintf(stderr, "\n    chyba: mtcp_listen(%u) vratil NULL\n", port);
            _exit(2);
        }
        pid_t sp = fork();
        if (sp == 0) {
            netsim_configure(cfg->loss, cfg->dup, cfg->corrupt, cfg->reorder, 2);
            mtcp_conn *c = mtcp_accept(l);
            if (!c) {
                fprintf(stderr, "\n    chyba: mtcp_accept vratil NULL\n");
                _exit(2);
            }
            mtcp_listener_close(l);
            _exit(srv(c, cfg) ? 1 : 0);
        }
        g_server_pid = sp;
        mtcp_listener_close(l);
        netsim_configure(cfg->loss, cfg->dup, cfg->corrupt, cfg->reorder, 3);

        mtcp_conn *c = mtcp_connect("127.0.0.1", port);
        if (!c) {
            fprintf(stderr, "\n    chyba: mtcp_connect vratil NULL\n");
            kill(sp, SIGKILL);
            waitpid(sp, NULL, 0);
            _exit(2);
        }
        int rc = cli(c, cfg);
        if (rc != 0)
            kill(sp, SIGKILL);
        int st = 0;
        waitpid(sp, &st, 0);
        int srv_ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
        if (rc == 0 && !srv_ok)
            fprintf(stderr, "\n    chyba: server skoncil s chybou (viz vyse)\n");
        _exit(rc == 0 && srv_ok ? 0 : 1);
    }
    int st = 0;
    waitpid(tp, &st, 0);
    return !(WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

int main(int argc, char **argv)
{
    static const cfg_t tests[] = {
        /* name,                       loss, dup, corr, reord, total, chunk, rbuf, max_s */
        { "handshake a zavreni",        0,    0,   0,    0,       0,     0,    0,   0 },
        { "ahoj / pong",                0,    0,   0,    0,       0,     0,    0,   0 },
        { "1 MB bez ztrat",             0,    0,   0,    0, 1 << 20,  4000, 4096,   0 },
        { "1 B po jednom (2000x)",      0,    0,   0,    0,    2000,     1,    7,   0 },
        { "jedno velke send (100 kB)",  0,    0,   0,    0,  100000, 100000, 13,   0 },
        { "ztrata 10 %",              0.10,   0,   0,    0,  100000,  4000, 4096,   0 },
        { "ztrata 30 % + duplikaty",  0.30, 0.20,  0,    0,   64000,  4000, 4096,   0 },
        { "poskozene pakety",         0.05,   0, 0.10,   0,   64000,  4000, 4096,   0 },
        { "prohozene poradi",         0.05,   0,   0,  0.20,  64000,  4000, 4096,   0 },
        { "echo oboustranne",         0.05,   0,   0,    0,   50000,  1000, 4096,   0 },
        { "vse najednou",             0.10, 0.10, 0.05, 0.10, 64000,  4000, 4096,   0 },
        { "rychlost: 4 MB do 5 s",      0,    0,   0,    0, 4 << 20,  8000, 8192, 5.0 },
    };
    const int n = (int)(sizeof tests / sizeof tests[0]);

    struct { role_fn srv, cli; } roles[] = {
        { server_expect_eof, client_close_only },
        { server_hello, client_hello },
        { server_sink, client_source },
        { server_sink, client_source },
        { server_sink, client_source },
        { server_sink, client_source },
        { server_sink, client_source },
        { server_sink, client_source },
        { server_sink, client_source },
        { server_echo, client_echo },
        { server_sink, client_source },
        { server_sink, client_source },
    };

    int only = argc > 1 ? atoi(argv[1]) : 0;
    int failed = 0, ran = 0;

    for (int i = 0; i < n; i++) {
        if (only && only != i + 1)
            continue;
        printf("[%2d] %-28s ", i + 1, tests[i].name);
        fflush(stdout);
        double t0 = now_s();
        int rc = run_one(i + 1, &tests[i], roles[i].srv, roles[i].cli);
        printf("%s  (%.2f s)\n", rc ? "-> FAIL" : "ok", now_s() - t0);
        fflush(stdout);
        ran++;
        failed += rc ? 1 : 0;
    }
    printf("\n%d z %d testu proslo\n", ran - failed, ran);
    return failed ? 1 : 0;
}
