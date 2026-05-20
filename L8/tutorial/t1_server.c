#include "../w8-common.h"

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

typedef struct {
    struct sockaddr_in addr;
    int32_t last_chunk;
    int32_t total_chunks;
    int active;
} client_state_t;

// Zwraca:
//   0  - zaktualizowano (nowy chunk lub duplikat - main musi sprawdzić last_chunk)
//   1  - usunięto klienta (ostatni chunk)
//   2  - dodano nowego klienta
//  -1  - brak miejsca (6-ty klient, ignoruj)
int aktualizujStan(struct sockaddr_in addr, client_state_t* arr, int32_t chunk, int32_t total_chunks) {
    // klient istnieje
    for (int i = 0; i < 5; i++) {
        if (arr[i].active == 1 && arr[i].addr.sin_addr.s_addr == addr.sin_addr.s_addr && arr[i].addr.sin_port == addr.sin_port) 
        {
           if (arr[i].last_chunk + 1 == chunk) {
                arr[i].last_chunk = chunk;
                if (arr[i].last_chunk == total_chunks - 1) {
                    arr[i].active = -1;
                    return 1;  // ostatni
                }
                return 3;  // nowy chunk
            }
            return 0;  // duplikat
        }
    }

    // dodajemy nowego klienta
    for (int i = 0; i < 5; i++) {
        if (arr[i].active == -1) {
            arr[i].addr = addr;
            arr[i].active = 1;
            arr[i].total_chunks = total_chunks;
            arr[i].last_chunk = chunk;

            if (chunk == total_chunks - 1) {
                arr[i].active = -1; // Zwolnij miejsce, bo to caly plik
                return 1;
            }
            
            return 2; // dodano
        }
    }

    return -1; // blad
}

// flow: sokcet -> bind
// petla: recvfrom, sendto
int main(int argc, char** argv) {
    /*
    1. Odbierz datagram
    2. Zwroc info ze odebrales
    3. Wypisz
    3. Naraz mozna odebrac maks 5 plikow, 6 ma byc zignorowany 
    */
    sethandler(SIG_IGN, SIGINT);

    int sockfd = create_and_bind_udp_socket(atoi(argv[1]));

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    message_t buf;
    ack_t bufAck;
    client_state_t clients[5];

    for (int i = 0; i < 5; i++)
        clients[i].active = -1;

    while (1) {
        addr_len = sizeof(client_addr); // dobry zwyaczaj, bo addr_len moze byc ronzej dlugosci
        int n = recvfrom(sockfd, &buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (n < 0) ERR("recvfrom");

        buf.id = ntohl(buf.id);
        buf.curr = ntohl(buf.curr);
        buf.total = ntohl(buf.total);

        int result = aktualizujStan(client_addr, clients, buf.curr, buf.total);
        if (result == -1)
            continue;

        if (result == 1 || result == 2 || result == 3) {
            printf("%.*s", (int)(n - 3*sizeof(int32_t)), buf.message);
            fflush(stdout);
        }

        bufAck.id = htonl(buf.id);
        bufAck.curr = htonl(buf.curr);

        if (sendto(sockfd, &bufAck, sizeof(bufAck), 0, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) 
            ERR("sendto");
    }

}