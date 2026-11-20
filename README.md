# mtcp: a reliable byte stream over UDP, in C

`mtcp` ("mini TCP") is a small learning project: a reliable, ordered byte stream
built on top of plain UDP sockets. UDP can only deliver single datagrams, and
those can be lost, duplicated, corrupted or reordered. The goal is to hide all
of that behind a simple connection API (`listen` / `accept` / `connect` /
`send` / `recv` / `close`), the same way TCP does it in the kernel.

## Why

I wanted to understand how TCP actually works (handshake, sequence numbers,
acknowledgements, retransmission timers, checksums, connection teardown) and,
at the same time, how sockets work in C (`socket`, `bind`, `sendto`,
`recvfrom`, `poll`, `sockaddr_in`, byte order, memory management and error
handling). The same ideas sit under QUIC and most of the network stack that
CDNs depend on, so this is a good exercise for transport-layer and
performance-oriented work.

## Background and honesty note

- **I have built something similar before as a school project.** This is a
  deliberate redo: I wanted to refresh the material and understand it properly
  this time instead of relying on what I half remembered.
- **AI assistance (Claude, by Anthropic).** I used Claude as a tutor and
  reviewer, and I want to be clear about what that means here:
  - The assignment text (`ZADANI.md`), the test suite and the network
    simulator that drops, duplicates, corrupts and reorders packets were
    prepared with Claude's help. They are the "given" part of the project.
  - Claude explained concepts (sockets, UDP vs. TCP, `bind` / `recvfrom`
    semantics, byte order, struct casting, QUIC), answered my questions, and
    reviewed my code, pointing out bugs and unclear style (for example wrong
    `sizeof` use, missing return-value checks, memory ownership, error
    handling). It occasionally showed small illustrative snippets, such as a
    cleanup pattern or the `recvfrom` length argument.
  - **The implementation in `src/mtcp.c` is written by me.** Claude did not
    write it for me; the agreed rule for the project is that it gives
    explanations and review, not finished solutions.

## Status

Work in progress. I update this section as I go.

- [x] Stage 0: UDP warm-up programs (`src/server.c`, `src/client.c`)
- [ ] Stage 1: handshake and close (in progress: `mtcp_listen` first)
- [ ] Stage 2: data transfer without loss (stop-and-wait)
- [ ] Stage 3: retransmission and duplicates
- [ ] Stage 4: checksum and reordering
- [ ] Stage 5: two-way communication and clean shutdown
- [ ] Stage 6 (bonus): sliding window, adaptive RTO (RFC 6298)

Test results: _to be filled in (target: 12 of 12, also clean under
AddressSanitizer)._

## Build and test

```
make            # build the test binary
./test_mtcp     # run all tests
./test_mtcp 6   # run just test number 6
make asan       # same with AddressSanitizer / UBSan
```

## Planned design

The protocol is not finished yet; this describes the intended shape.

- Packets carry a small header: flags (`SYN`, `ACK`, `FIN`, `DATA`), data
  length, sequence number, acknowledgement number and a checksum. All
  multi-byte fields go on the wire in network byte order.
- Connection setup: the client repeats `SYN` until it receives `SYN+ACK`; the
  server remembers the client's address and answers every repeated `SYN`.
- Reliability: every data packet is acknowledged, unacknowledged packets are
  retransmitted after a timeout (about 20 ms on loopback), duplicates are
  acknowledged but not delivered twice, and corrupted packets (bad checksum)
  or packets from unknown addresses are dropped.
- Teardown: `FIN` with acknowledgement, with the closing side lingering
  briefly to answer a repeated `FIN` (the idea behind TCP's `TIME_WAIT`).
- All sending goes through the provided simulator (`netsim_sendto`) so the
  tests can inject packet loss.

## Repository layout

```
include/mtcp.h        public API (fixed by the assignment)
src/mtcp.c            my implementation
src/server.c          UDP warm-up server
src/client.c          UDP warm-up client
support/netsim.[ch]   network simulator (provided)
tests/test_mtcp.c     test suite (provided)
ZADANI.md             the assignment (in Czech)
```

## Ideas for later

- IPv6 / dual-stack listening (a single `AF_INET6` socket with
  `IPV6_V6ONLY` disabled).
- Sliding window and adaptive retransmission timeout.
- Comparing throughput with plain stop-and-wait, and writing up the results.
