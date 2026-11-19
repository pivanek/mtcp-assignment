# mtcp: spolehlivý přenos dat nad UDP

Napíšeš v C vlastní miniaturní verzi TCP. UDP umí jen poslat samostatný balíček (datagram), který se klidně ztratí, zdvojí, poškodí nebo dorazí v jiném pořadí. Tvým úkolem je nad ním postavit **spolehlivý proud bajtů**: co jedna strana pošle, druhá dostane celé, správně a ve stejném pořadí.

Je to přesně ten typ problému, který řeší síťové vrstvy ve CDN (TCP, QUIC), a zároveň nejlepší způsob, jak si v C procvičit paměť, struktury, ukazatele a sockety.

**Odhad:** 2–4 večery podle toho, kolik času dáš ladění. Nejdůležitější je postupovat po etapách a po každé pustit testy.

---

## 1. Co dostaneš

```
mtcp-assignment/
├── ZADANI.md            ← tento soubor
├── Makefile
├── include/mtcp.h       ← veřejné rozhraní (NEMĚŇ)
├── src/mtcp.c           ← TVŮJ SOUBOR (kostra s TODO)
├── support/netsim.[ch]  ← simulátor špatné sítě (hotový, NEMĚŇ)
└── tests/test_mtcp.c    ← testy (hotové, NEMĚŇ)
```

Spuštění:

```
make            # přeloží testy
./test_mtcp     # všech 12 testů
./test_mtcp 6   # jen test č. 6
make asan       # totéž se sanitizéry (hlídají paměť)
```

Na začátku všechny testy padají, protože kostra nic nedělá. Cíl je mít **12 z 12**.

## 2. Rozhraní (`include/mtcp.h`)

| Funkce | Co dělá |
|---|---|
| `mtcp_listen(port)` | server: otevře UDP socket na portu |
| `mtcp_accept(l)` | server: počká na klienta (handshake), vrátí spojení |
| `mtcp_listener_close(l)` | zavře listener |
| `mtcp_connect(ip, port)` | klient: naváže spojení, při neúspěchu po ~5 s vrátí `NULL` |
| `mtcp_send(c, buf, len)` | pošle **celý** buffer, blokuje, dokud není vše potvrzeno |
| `mtcp_recv(c, buf, len)` | přečte aspoň 1 bajt (nejvýš `len`), vrátí `0`, když protistrana zavřela a data došla |
| `mtcp_close(c)` | korektně ukončí spojení a uvolní paměť |

Podrobnosti jsou v komentářích v `mtcp.h`. Spojení je **jedno na listener** (nemusíš řešit víc klientů najednou).

## 3. Důležitá pravidla

1. **Posílej přes `netsim_sendto()`**, nikdy přímo přes `sendto()`. Testy tím simulují ztráty. Příjem je normální `recvfrom()`.
2. **Nepoužívej `connect()` na UDP socketu.** Používej `sendto()` a `recvfrom()` a adresu protistrany si pamatuj sám.
3. **Datagram nad 1 kB je zbytečně velký.** Doporučená velikost dat v jednom paketu: **1024 B**. Větší `mtcp_send` rozděl na víc paketů.
4. **Časový limit opakování (RTO) na loopbacku nastav kolem 20 ms.** Při 200 ms by testy trvaly desítky sekund. Pozor: `recvfrom` blokuje, takže na timeout použij `poll()` (nebo `SO_RCVTIMEO`).
5. **Zpracovávej příchozí pakety i uvnitř `mtcp_send`.** Protistrana ti může právě posílat data (test č. 10, echo), a když je v `send` ignoruješ, dojde k zablokování.
6. **Každý přijatý paket s daty potvrď**, i duplicitní (starší číslo). Právě tak se opraví ztracené potvrzení.
7. **Ukončení (`mtcp_close`) a poslední potvrzení.** Ten, kdo zavírá jako první, pošle `FIN` a po potvrzení chvíli dál **odpovídá na opakované FIN** protistrany (v TCP se tomu říká `TIME_WAIT`; stačí ~100 ms). Ten, kdo zavírá až poté, co už dostal FIN od druhé strany, může své `FIN` opakovat, ale pokud se potvrzení nedočká (protistrana už je pryč), vrátí `0`, ne chybu.
8. Po dokončení nesmí zůstat **žádné úniky paměti** (`make asan` to ověří).

## 4. Doporučená hlavička paketu

Nemusíš ji dodržet přesně (testy ji neřeší), ale tohle je rozumný základ:

```c
struct hdr {
    uint8_t  flags;   // SYN=1, ACK=2, FIN=4, DATA=8
    uint8_t  pad;
    uint16_t len;     // délka dat za hlavičkou
    uint32_t seq;     // číslo odesílaného paketu (nebo bajtu)
    uint32_t ack;     // co očekávám jako další
    uint16_t csum;    // kontrolní součet celého datagramu
};
```

- Číselná pole posílej v **síťovém pořadí bajtů** (`htons`/`htonl`, `ntohs`/`ntohl`).
- Kontrolní součet: sečti 16bitová slova, přenos zahrň zpět (1's complement) a výsledek neguj. Při počítání pole `csum` nastav na 0.
- Přijatý datagram zahoď, pokud: je kratší než hlavička, nesedí `len`, nesedí `csum`, nebo přišel od cizí adresy.

## 5. Etapy

Doporučuju jít přesně v tomto pořadí. U každé etapy je napsáno, které testy by měly projít a co si v C procvičíš.

### Etapa 0: rozcvička se sockety (mimo projekt)
Napiš dva malé programy: UDP server, který vypisuje, co dostal, a klient, který mu pošle řetězec. Vyzkoušej `socket`, `bind`, `sendto`, `recvfrom`, `poll`.
*C:* `struct sockaddr_in`, převody adres (`inet_pton`), kontrola návratových hodnot (`-1` + `errno`).

### Etapa 1: handshake a zavření (test 1)
Implementuj `listen`, `connect`, `accept`, jednoduchý `close`. Klient posílá `SYN`, dokud nepřijde `SYN+ACK`. Server si při příjmu SYN zapamatuje adresu klienta.
*C:* `malloc`/`free` pro struct `mtcp_conn`, ukazatele na struktury, `dup()` pro předání socketu z listeneru.
*Past:* když se ztratí `SYN+ACK`, klient pošle `SYN` znovu. Server musí znovu odpovědět.

### Etapa 2: přenos bez ztrát (testy 2, 3, 4, 5)
Stop-and-wait: pošli jeden paket, počkej na `ACK`, pošli další. Přijímač si příchozí data ukládá do vlastního bufferu, ze kterého je `mtcp_recv` vydává po částech.
*C:* dynamické pole (`realloc`), `memcpy`, ukazatelová aritmetika, práce s `size_t` a `ssize_t`.
*Past:* `mtcp_recv(c, buf, 7)` musí vrátit jen 7 bajtů a zbytek si pamatovat do příštího volání.

### Etapa 3: opakování a duplikáty (testy 6, 7)
Když `ACK` do RTO nepřijde, pošli paket znovu. Přijímač rozlišuje: nový paket (ulož a potvrď), duplikát (jen potvrď), cizí číslo (ignoruj).
*C:* `poll()` s timeoutem, měření času (`clock_gettime`).
*Past:* zdvojený paket nesmí vložit data do bufferu dvakrát.

### Etapa 4: kontrolní součet a pořadí (testy 8, 9)
Přidej `csum`. Poškozené pakety zahoď, jako by se ztratily. Prohozené pořadí by se mělo vyřešit samo, pokud číslování paketů používáš správně.
*C:* bitové operace, pole bajtů jako `uint8_t[]`, přetypování ukazatele na `struct hdr *` (a proč je to na hraně, pokud není zarovnání zaručeno).

### Etapa 5: oboustranná komunikace a zavírání (testy 10, 11)
Oba směry fungují zároveň, `close` dodrží pravidlo č. 7. Zkus testy opakovaně a pod `make asan`.

### Etapa 6 (bonus): rychlost a pokročilé věci
Test 12 projde už se stop-and-wait. Pokud chceš víc:
- **posuvné okno** (poslat víc paketů najednou), případně Go-Back-N nebo Selective Repeat,
- **adaptivní RTO** podle naměřeného RTT (jako v RFC 6298),
- porovnání rychlosti vůči stop-and-wait a krátká zpráva o výsledku do README.

## 6. Co ti pomůže při ladění

- **Padá program nebo něco "zmizí"?** `gdb ./test_mtcp` → `run 6`. Pro test běžící ve víc procesech: `set follow-fork-mode child`.
- **Podezření na paměť?** `make asan` ukáže přesnou řádku přetečení nebo úniku.
- **Nevíš, co se posílá?** Dočasně si vypisuj `flags/seq/ack` každého odeslaného a přijatého paketu (`fprintf(stderr, ...)`). Na loopbacku jde i `tcpdump -i lo -X udp port 41006` nebo Wireshark.
- **Test visí?** Počkej, nejpozději za 30 s ho test sám ukončí a napíše, že vypršel časový limit. Skoro vždy je to zablokovaný `recvfrom` bez timeoutu nebo někde nezpracovaný paket.
- Simulátor sítě používá pokaždé stejná náhodná čísla, takže chyby se obvykle opakují a dají se ladit. Když test jednou projde a podruhé ne, hledej časování (RTO, nezpracovaný paket).

## 7. Hotovo, když…

- [ ] `./test_mtcp` hlásí **12 z 12**,
- [ ] `make asan` projde bez hlášení,
- [ ] kód je okomentovaný tam, kde není zřejmý (zejména protokol),
- [ ] v repozitáři je **README** s popisem formátu paketu, popisem protokolu (stavy handshake/close, opakování) a výsledky testů.

Pak to dej na GitHub a přidej do CV jednou větou, např. *"mtcp: reliable byte stream over UDP in C (handshake, retransmission, checksum, loss/reorder simulation, tested under ASan)."* Přesně tohle se u CDN77 hodí.

## 8. Když se zasekneš

V novém chatu mi pošli:
1. kus kódu, kde si nevíš rady (a jaký test padá a jak),
2. co konkrétně nechápeš. I "nevím, proč tohle vrací -1" stačí.

Vysvětlím, co se děje, a navedu tě k řešení. Hotové řešení ti radši nedávám. Největší efekt to má, když to napíšeš sám.
