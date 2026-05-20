#include "../w8-common.h"
/*
Zadanie: napisa cklient-serwer UDP
Wyslanie pliku tekstowego podzielonego na datagramy - klient
serwer odbiera plik przeslany przez socket i go wyswietla

serwer wysyla potwierdzenie, jesli nie wysle w ciagu 0,5s, to wysylamy jeszcze raz
jesli 5 razy klient wysle i nic, to koniec programu

wszystkie dodatkowe dane poza tekstem (char = 1B) ma miec postac int32_t
Datagram Size <= 576B

Serwer moze otrzymac max 5 plikow jednoczesnie, 6. plik ma byc zignorowany

Program serwer dostaje jako parametr nr portu
klient dostaje adres i port serwera oraz nazwe pliku
*/
typedef struct {
    int32_t id;
    int32_t curr;
    int32_t total;
    char message[564];
} message_t;

typedef struct {
    int32_t curr;
    int32_t id;
} ack_t;

// klient: socket -> sendto, recvfrom
int main(int argc, char** argv) {
    if (argc < 4) ERR("argc");

    sethandler(SIG_IGN, SIGINT);

    message_t message; // struktura do wyslania
    char buf[564]; // buf uzywany do czytania i wysylania
    ack_t ackBuf;
    struct stat info; // stat pliku
    ssize_t n; // czytanie bajtow do buf z pliku
    int sendTry;

    int sockfd = make_udp_socket(); // czym wysylam
    struct sockaddr_in server_addr = make_address(argv[1], argv[2]); // gdzie wysylam

    int fd_file = open(argv[3], O_RDONLY);
    if (fd_file < 0) ERR("open");
    if (fstat(fd_file, &info) < 0) ERR("fstat");
    
    // ustawienie timeout 500ms na gniezdzie
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000; 
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    // recvfrom na sockfd staje sie blokujace na 0.5s Jesli nic nie
    // przyjdzie w tym czasie zwraca -1 z errno = EAGAIN lub EWOULDBLOCK

    /*
    TEMP_FAILURE_RETRY
    Prowadzący zabezpiecza wywołania sieciowe i systemowe (takie jak 
    recvfrom, sendto, close, open) makrem TEMP_FAILURE_RETRY. Jeśli
    wywołanie systemowe zostanie przerwane przez jakikolwiek sygnał
    (np. wspomniany SIGALRM u klienta), makro automatycznie ponowi
    próbę wykonania tej funkcji.
    
    Ja tego nie stosuje, bo nie uzywam sygnalow.
    */

    message.id = htonl(getpid());
    message.total = info.st_size / sizeof(message.message);
    if (info.st_size % sizeof(message.message) > 0) message.total++;
    message.total = htonl(message.total);
    
    int i = 0;
    while ((n = read(fd_file, buf, sizeof(buf))) > 0) {
        size_t actual_size = 3 * sizeof(int32_t) + n;

        message.curr = htonl(i++);
        memcpy(message.message, buf, n);

        sendTry = 0;
        int success = 0;

        while (sendTry < 5) {
            sendTry++;
            
            if (sendto(sockfd, &message, actual_size, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
                ERR("sendto");
            //sleep(1); // zastapione przez setsockopt

            if (recvfrom(sockfd, &ackBuf, sizeof(ack_t), 0, NULL, NULL) < 0)
                continue;

            if (message.id == ackBuf.id && message.curr == ackBuf.curr) 
            {
                success++;
                break;
            }
        }

        if (!success) {
            fprintf(stderr, "Blad: Brak potwierdzenia po 5 probach.\n");
            close(fd_file);
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }

    close(fd_file);
    close(sockfd);
    exit(EXIT_SUCCESS);
}

