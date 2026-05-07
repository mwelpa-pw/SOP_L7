#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}


// make_local_socket  [HELPER, używana wewnętrznie przez funkcje niżej]
// ---------------------------------------------------------------------
// Tworzy gniazdo AF_UNIX i wypełnia strukturę sockaddr_un ścieżką pliku.
// Nie robi bind/connect — to robią funkcje wyższego poziomu.
// 
// Sama nie jest używana bezpośrednio z main — wołają ją bind_local_socket
// i connect_local_socket.
// ---------------------------------------------------------------------
int make_local_socket(char* name, struct sockaddr_un* addr)
{
    int socketfd;
    if ((socketfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
        ERR("socket");
    memset(addr, 0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, name, sizeof(addr->sun_path) - 1);
    return socketfd;
}

// ---------------------------------------------------------------------
// connect_local_socket  [KLIENT — UNIX domain]
// ---------------------------------------------------------------------
// Tworzy gniazdo + connect do podanej ścieżki pliku gniazda.
// Zwraca deskryptor połączonego gniazda — można od razu pisać/czytać.
//
// KLIENT UNIX, kolejność:
//   1. connect_local_socket("/tmp/z1.sock")
//   2. bulk_write(fd, dane, rozmiar)
//   3. bulk_read(fd, bufor, rozmiar)
//   4. close(fd)
// ---------------------------------------------------------------------
int connect_local_socket(char* name)
{
    struct sockaddr_un addr;
    int socketfd;
    socketfd = make_local_socket(name, &addr);
    if (connect(socketfd, (struct sockaddr*)&addr, SUN_LEN(&addr)) < 0)
    {
        ERR("connect");
    }
    
    return socketfd;
}

// ---------------------------------------------------------------------
// bind_local_socket  [SERWER — UNIX domain]
// ---------------------------------------------------------------------
// Tworzy gniazdo nasłuchujące AF_UNIX:
//   1. unlink starego pliku (jeśli istnieje) — bez tego bind by się wywalił
//   2. socket
//   3. bind do ścieżki
//   4. listen z podanym backlog
// 
// Zwraca deskryptor gotowy do epolla / accept.
//
// Po zakończeniu serwera trzeba ręcznie unlink(name)!
// ---------------------------------------------------------------------
int bind_local_socket(char* name, int backlog_size)
{
    struct sockaddr_un addr;
    int socketfd;
    if (unlink(name) < 0 && errno != ENOENT)
        ERR("unlink");
    socketfd = make_local_socket(name, &addr);
    if (bind(socketfd, (struct sockaddr*)&addr, SUN_LEN(&addr)) < 0)
        ERR("bind");
    if (listen(socketfd, backlog_size) < 0)
        ERR("listen");
    return socketfd;
}


// =====================================================================
// SEKCJA 2: GNIAZDA TCP (AF_INET)
// =====================================================================

// ---------------------------------------------------------------------
// make_tcp_socket  [HELPER]
// ---------------------------------------------------------------------
// Tworzy goły socket TCP/IPv4. Nic więcej.
// Używana wewnętrznie przez bind_tcp_socket / connect_tcp_socket.
// ---------------------------------------------------------------------
int make_tcp_socket(void)
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

// ---------------------------------------------------------------------
// make_address  [HELPER, KLIENT TCP]
// ---------------------------------------------------------------------
// Konwertuje (nazwa_hosta, port) na strukturę sockaddr_in używaną
// przez connect/bind. Robi to przez getaddrinfo, więc działa zarówno
// dla nazw domenowych ("localhost", "google.com") jak i adresów IP
// ("127.0.0.1", "192.168.0.1").
//
// UWAGA: bierze tylko PIERWSZY wynik z getaddrinfo i nie iteruje
// po liście. W produkcyjnym kodzie powinno się próbować connect po
// każdym wyniku, ale dla zadania labowego to wystarczy.
// ---------------------------------------------------------------------
struct sockaddr_in make_address(char* address, char* port)
{
    int ret;
    struct sockaddr_in addr;
    struct addrinfo* result;
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    if ((ret = getaddrinfo(address, port, &hints, &result)))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }
    addr = *(struct sockaddr_in*)(result->ai_addr);
    freeaddrinfo(result);
    return addr;
}

// ---------------------------------------------------------------------
// connect_tcp_socket  [KLIENT — TCP]
// ---------------------------------------------------------------------
// Tworzy gniazdo + getaddrinfo + connect.
// Zwraca deskryptor połączonego gniazda — można od razu pisać/czytać.
//
// KLIENT TCP, kolejność:
//   1. connect_tcp_socket("localhost", "5151")
//   2. bulk_write(fd, dane, rozmiar)
//   3. bulk_read(fd, bufor, rozmiar)
//   4. close(fd)
// 
// UWAGA: port jest STRINGIEM, nie int — bo getaddrinfo tak chce.
// --------------------------------------------------------------------
int connect_tcp_socket(char* name, char* port)
{
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_tcp_socket();
    addr = make_address(name, port);
    if (connect(socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        ERR("connect");
    }
    return socketfd;
}

// ---------------------------------------------------------------------
// bind_tcp_socket  [SERWER — TCP]
// ---------------------------------------------------------------------
// Tworzy gniazdo nasłuchujące TCP/IPv4:
//   1. socket
//   2. wypełnij sockaddr_in (INADDR_ANY = nasłuch na każdym interfejsie)
//   3. SO_REUSEADDR — pozwala szybko ponownie zbindować port
//      (bez tego po zabiciu serwera trzeba czekać ~60s na TIME_WAIT)
//   4. bind do portu
//   5. listen z podanym backlog
// 
// Zwraca deskryptor gotowy do epolla / accept.
//
// Aby ustawić nieblokujące — po wywołaniu zrób:
//   fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
// ---------------------------------------------------------------------
int bind_tcp_socket(uint16_t port, int backlog_size)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_tcp_socket();
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        ERR("bind");
    if (listen(socketfd, backlog_size) < 0)
        ERR("listen");
    return socketfd;
}

// ---------------------------------------------------------------------
// add_new_client  [SERWER]
// ---------------------------------------------------------------------
// Wrapper na accept() z dwoma usprawnieniami:
//   1. TEMP_FAILURE_RETRY — automatyczny retry przy EINTR
//   2. EAGAIN/EWOULDBLOCK -> zwraca -1 zamiast wywalania programu
//      (przydatne, gdy gniazdo nasłuchujące jest nieblokujące i
//       epoll Cię obudził, ale klient zniknął zanim doszliśmy do accept)
//
// Zwraca:
//   >= 0  — deskryptor nowego klienta
//   -1    — chwilowo brak klientów (EAGAIN/EWOULDBLOCK)
//
// SERWER, użycie:
//   if (events[i].data.fd == listen_fd) {
//       int client = add_new_client(listen_fd);
//       if (client < 0) continue;
//       // dodaj do epolla...
//   }
// ---------------------------------------------------------------------
int add_new_client(int sfd)
{
    int nfd;
    if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, NULL, NULL))) < 0)
    {
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            return -1;
        ERR("accept");
    }
    return nfd;
}

// Czyta dokładnie count bajtów z deskryptora fd do bufora buf
// Zatrzymuje się przy błędzie lub końcu pliku
ssize_t bulk_read(int fd, char* buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (0 == c)
            return len;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

// Zapisuje dokładnie count bajtów z bufora buf do deskryptora fd
// Powtarza wywołanie write aż do przesłania wszystkich bajtów lub błędu
ssize_t bulk_write(int fd, char* buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

/*
// KLIENT TCP lub UNIX
1. connect_tcp_socket(host, port)     LUB    connect_local_socket(path)
2. msg_to_network(&msg)               // konwersja endianness przed wysyłką
3. bulk_write(fd, &msg, sizeof(msg))  // wyślij zapytanie
4. bulk_read(fd, &msg, sizeof(msg))   // odbierz odpowiedź
5. msg_to_host(&msg)                  // konwersja endianness po odbiorze
6. close(fd)                          // koniec

// SERWER (TCP + UNIX z epollem)
SETUP (raz przy starcie):
1. sethandler(sigint_handler, SIGINT)    // graceful shutdown
2. sethandler(SIG_IGN, SIGPIPE)          // ochrona przed śmiercią od EPIPE
3. tcp_fd  = bind_tcp_socket(port, backlog)
4. unix_fd = bind_local_socket(path, backlog)
5. (opcjonalnie) fcntl(*, F_SETFL, O_NONBLOCK) na obu
6. epoll_fd = epoll_create1(0)
7. epoll_ctl(EPOLL_CTL_ADD) dla tcp_fd i unix_fd

PĘTLA:
8. while (do_work):
9.   epoll_wait(epoll_fd, events, MAX, -1)
10.  dla każdego zdarzenia:
11.    jeśli to listen_fd (tcp lub unix):
12.       client_fd = add_new_client(fd)
13.       jeśli client_fd >= 0: epoll_ctl(EPOLL_CTL_ADD, client_fd)
14.    inaczej (klient):
15.       bulk_read(fd, &msg, sizeof(msg))
16.       jeśli EOF/błąd: epoll_ctl(EPOLL_CTL_DEL) + close
17.       inaczej: msg_to_host, oblicz, msg_to_network, bulk_write
                   (z obsługą EPIPE)

CLEANUP (po pętli):
18. close(tcp_fd), close(unix_fd), close(epoll_fd)
19. unlink(path)            // ważne — usuń plik gniazda lokalnego!
*/
