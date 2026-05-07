#define _GNU_SOURCE
#include "z1_shared.h"
#include "../w7-common.h"
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/epoll.h>
// socket -> bind -> listen -> accept

volatile sig_atomic_t work = 1;

void sigint_handler() {work = 0;}

int main(int argc, char** argv) {
    if (argc < 3) return 1;
    char* localPortName = argv[1];
    int port = atoi(argv[2]);

    int sockfd, sockfd2;

    sethandler(sigint_handler, SIGINT);
    sethandler(SIG_IGN, SIGPIPE);

    // tcp

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) ERR("sockfd");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int t = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t));

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        //printf(errno);
        ERR("bind");
    }

    if (listen(sockfd, 10) == -1) {
        //printf(errno);
        ERR("bind");
    }

    // unix
    sockfd2 = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd2 == -1) ERR("sockfd2");

    struct sockaddr_un addr2;
    memset(&addr2, 0, sizeof(addr2));
    addr2.sun_family = AF_UNIX;

    if (unlink(localPortName) < 0 && errno != ENOENT) ERR("unlink");

    strncpy(addr2.sun_path, localPortName, sizeof(addr2.sun_path) - 1);
    
    if (bind(sockfd2, (struct sockaddr*)&addr2, sizeof(addr2)) == -1) {
        //printf(errno);
        ERR("bind");
    }

    if (listen(sockfd2, 10) == -1) {
        //printf(errno);
        ERR("bind");
    }

    // epoll
    int epollfd = epoll_create1(0);
    if (epollfd < 0)
        ERR("epoll create");

    // tcp
    struct epoll_event event_tcp;
    event_tcp.events = EPOLLIN;
    event_tcp.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event_tcp) == -1) ERR("TCP");

    // unix
    struct epoll_event event_unix;
    event_unix.events = EPOLLIN;
    event_unix.data.fd = sockfd2;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd2, &event_unix) == -1) ERR("UNIX");

    struct epoll_event events[16];

    while (work) {
        int n = epoll_wait(epollfd, events, 16, -1);

        if (n < 0) {
            if (errno == EINTR) continue;  // przerwane sygnałem - to nie błąd
            ERR("epoll_wait");
        }

        for (int i = 0; i < n; i++) {
            // odczytywanie zdarzen
            int fd = events[i].data.fd;
            int client_fd;

            if (fd == sockfd || fd == sockfd2) {
                // gniazdo nasłuchujące - ktoś chce się połączyć
                client_fd = accept(fd, NULL, NULL);
                if (client_fd < 0) ERR("epollwait sockfd"); 

                // dodajemy nowego klienta do epolla
                struct epoll_event ev;
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;

                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &ev) < 0)
                    ERR("epoll_ctl ADD client");
            } else {
                // aktualny klient
                message_t buf;
                ssize_t r = bulk_read(fd, (char*)&buf, sizeof(buf));

                if (r == 0) {
                    // klient sie rozlaczyl - eof
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }
                if (r < 0) ERR("bulk read");
                if (r != sizeof(buf)) {
                    // niepelna wiadomosc - trakujemy jak rozlaczenie sie
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                msg_to_host(&buf);

                // tutaj liczymy wynik
                switch (buf.op)
                {
                case '+':
                    buf.result = buf.op1 + buf.op2;
                    buf.status = 1;
                    break;
                case '-':
                    buf.result = buf.op1 - buf.op2;
                    buf.status = 1;
                    break;
                case '*':
                    buf.result = buf.op1 * buf.op2;
                    buf.status = 1;
                    break;
                case '/':
                    if (buf.op2 == 0) {
                        buf.status = -1;
                        break;
                    }
                    buf.result = buf.op1 / buf.op2;
                    buf.status = 1;
                    break;        
                default:
                    buf.status = -1;
                    break;
                }

                // zostawiamy klienta podpietego pod epoll i wysylamy
                msg_to_network(&buf);
                if (bulk_write(fd, (char*)&buf, sizeof(buf)) < 0) ERR("bulk write"); // wszystko jest plikiem, fd to abstrakcja
            }
        }
    }
    
    close(sockfd);
    close(sockfd2);
    close(epollfd);
    unlink(localPortName);   // ← ważne, żeby nie zostawić pliku
}