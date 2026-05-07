#include <stdint.h>
#include <arpa/inet.h>
#ifndef SHARED_H
#define SHARED_H

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct {
    int32_t op1;
    int32_t op2;
    int32_t result;
    int32_t op;
    int32_t status;   
} message_t;

// static inline void funkcja() {}
static inline void msg_to_network(message_t *m) {
    m->op1    = htonl(m->op1);
    m->op2    = htonl(m->op2);
    m->op     = htonl(m->op);
    m->result = htonl(m->result);
    m->status = htonl(m->status);
}
/*
wiadomosc wyslana siecia musi zawsze 
byc zakodowana htonl, a potem ja odkodowujemy ntohl

NIE KONWERTUJE SIE POJEDYNCZYCH BAJTOW
*/
static inline void msg_to_host(message_t *m) {
    m->op1    = ntohl(m->op1);
    m->op2    = ntohl(m->op2);
    m->op     = ntohl(m->op);
    m->result = ntohl(m->result);
    m->status = ntohl(m->status);
}

/*
EPIPE = "Broken pipe" = piszesz do gniazda/pipe, którego druga strona już zamknęła.
Konkretnie: Twój bulk_write(fd, ...) zwraca -1 z errno = EPIPE w sytuacji, gdy:

Klient zrobił close() na swoim końcu połączenia, ALBO
Klient się wywalił/został zabity (ten sam efekt, kernel zamyka jego deskryptory), ALBO
Klient został odłączony fizycznie (kabel sieciowy wyrwany, w przypadku TCP)

how2: sethandler(SIG_IGN, SIGPIPE);
*/

#endif