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


// Tworzy gniazdo lokalne AF_UNIX i wypełnia strukturę adresu
// Zwraca deskryptor nowego gniazda

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

// Łączy się z lokalnym gniazdem UNIX socket o podanej nazwie
// Zwraca deskryptor połączenia

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

// Wykonywane przez serwer
// Tworzy i przygotowuje lokalne gniazdo do nasłuchu
// Usuwa istniejący plik socket (jeśli istnieje), nadaje mu adres i ustawia backlog (maksymalną liczbę    
// oczekujących połączeń)
// Zwraca deskryptor gniazda
// backlog_size - maksymalna liczba oczekujących połączeń

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


// Tworzy standardowe gniazdo TCP/IPv4
// Zwraca deskryptor gniazda

int make_tcp_socket(void)
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

// Tworzy strukturę adresu IPv4 na podstawie nazwy hosta i portu
// Używa getaddrinfo do rozwiązania nazwy
// Zwraca strukturę sockaddr_in z wypełnionymi polami
// result - struktura wypełniona przez getaddrinfo, zawierająca informacje o adresie
// hints - struktura używana do określenia kryteriów wyszukiwania adresu (np. rodzaj gniazda, rodzina adresów)

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

// Tworzy gniazdo TCP/IPv4 o podanym adresie i porcie i łączy się z nim
// Zwraca deskryptor połączonego gniazda TCP

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

// Tworzy, nadaje adres i ustawia gniazdo TCP na nasłuch na podanym porcie
// Ustawia SO_REUSEADDR, aby szybko ponownie użyć adresu po zamknięciu gniazda
// Zwraca deskryptor gniazda gotowego do akceptowania połączeń
// żeby gniazdo było nieblokujące -> użyć fcntl do ustawienia flagi O_NONBLOCK na zwróconym deskryptorze

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

// Akceptuje nowe połączenie na sockecie serwera
// Jeśli gniazdo jest ustawione na nieblokujące i nie ma klienta, zwraca -1 zamiast błędu
// Zwraca deskryptor nowego połączenia lub -1, jeśli nie ma oczekujących połączeń (w przypadku gniazda  nieblokującego)

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
