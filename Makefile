CC      ?= gcc
CFLAGS  ?= -std=gnu11 -Wall -Wextra -g -O1
INCS     = -Iinclude -Isupport
SRC     ?= src/mtcp.c

all: test_mtcp

test_mtcp: tests/test_mtcp.c $(SRC) support/netsim.c include/mtcp.h
	$(CC) $(CFLAGS) $(INCS) -o $@ tests/test_mtcp.c $(SRC) support/netsim.c

# spustí všechny testy
test: test_mtcp
	./test_mtcp

# se sanitizéry (najde úniky paměti, přetečení bufferů, použití po free)
asan: tests/test_mtcp.c $(SRC) support/netsim.c
	$(CC) $(CFLAGS) -fsanitize=address,undefined $(INCS) -o test_mtcp_asan \
		tests/test_mtcp.c $(SRC) support/netsim.c
	./test_mtcp_asan

clean:
	rm -f test_mtcp test_mtcp_asan

.PHONY: all test asan clean
