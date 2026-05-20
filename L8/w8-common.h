#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
    #include <sys/epoll.h>
#endif
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
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

// ============= HELPERY ============:
// AF_UNIX
int UDP_make_local_socket(char* name, struct sockaddr_un* addr) {
    int socketfd;
    if ((socketfd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0)
        ERR("socket");
    memset(addr, 0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    strncpy(addr->sun_path, name, sizeof(addr->sun_path) - 1);
    return socketfd;
}

// internet UDP AF_INET
int make_udp_socket(void) { 
    int sock;
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        ERR("socket");
    return sock;
}

// internet UDP AF_INET
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

// ============= FUNKCJE ============:

// KLIENT - AFUNIX: Connect local socket
//
// Zwraca deskrypot polaczonego gniazda - mozna od razu pisac/czytac
//  1. UDP_connect_local_socket("NAZWA")
int UDP_connect_local_socket(char* name)
{
    struct sockaddr_un addr;
    int socketfd;
    socketfd = UDP_make_local_socket(name, &addr);
    if (connect(socketfd, (struct sockaddr*)&addr, SUN_LEN(&addr)) < 0)
    {
        ERR("connect");
    }
    
    return socketfd;
}

// SERWER - AFUNIX: Binding local socket
// unlink old file -> socket -> bindowanie -> zwraca deskryptor gotowy do epolla / accept. Unlink(name) na koniec
int UDP_bind_local_socket(char* name)
{
    struct sockaddr_un addr;
    int socketfd;
    if (unlink(name) < 0 && errno != ENOENT)
        ERR("unlink");
    socketfd = UDP_make_local_socket(name, &addr);
    if (bind(socketfd, (struct sockaddr*)&addr, SUN_LEN(&addr)) < 0)
        ERR("bind");
    return socketfd;
}

// KLIENT - AF_INET - LACZENIE PRZEZ SOCKET
int connect_udp_socket(char* name, char* port) {
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_udp_socket();
    addr = make_address(name, port);
    if (connect(socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        ERR("connect");
    }
    return socketfd;
}

// SERWER - AF_INET
// socket + binding
int create_and_bind_udp_socket(uint16_t port)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_udp_socket();
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)))
        ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        ERR("bind");
    
    return socketfd;
}

/*
notatki:
socket(AF_INET, SOCK_DGRAM, 0); lub socket(AF_UNIX, SOCK_DGRAM, 0);
bind

sendto(sockfd, buf, len, flags, dstaddr, addrlen) - sends a single datagram
recvfrom(sockfd, buf, len, flags, arcaddr, addrlen) - receives a single incoming datagram

udp jest bezpolaczeniowe, wiec nie trzeba robic connect (klient) i listen (serwer)

RECVFROM
 flags:
MSG_WAITALL - blocks until the full amount of data can be returned
MSG_PEEK - Peeks at incoming message, but data is treated as unread

returned value: EAGAIN <-> O_NONBLOCK na sockecie, ECONNRESET - connection was forcibly closed by a peer

SENDTO:
Broadcast <-> SO_BROADCAST wpp fail
MSG_NOSIGNAL -> jesli cos poszlo nie tak, to nie wysylaj SIG; normlanie send() na rozlaczone polaczenie -> SIGPIPE -> EXIT
ale mozna tez zrobic sethandler(SIG_IGN, SIGPIPE);

message size > set size -> ucieta wiadomosc
message size <= set size -> cala wiadomosc 
*/
