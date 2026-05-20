// #include "../l8_common.h"
#include "../w8-common.h"
#define STACK_SIZE 16

typedef struct { // 576
    // przez to, ze te struktury sa jednobajtowe, to nie trzeba ich odwracac!
    int8_t X;
    int8_t Y;
    int8_t P; // 0 - wrogi, 1 - sojuszniczy
    char oddzial[128];
    // wiadomosc oznacza, ze podany oddzial przesunal sie na dana pozycje.
} message_t;

void print_message(message_t mes, int n) {
    char* rodzajOddzialu = "nasz";
    if (mes.P == 1) rodzajOddzialu = "wrogi"; 
    printf("%s oddzial %.*s byl widziany na pozycji %d:%d", 
        rodzajOddzialu, (int)(n - 3), mes.oddzial, mes.X, mes.Y);
}

int main(int argc, char** argv) {
    char* port = argv[1];
    // Jak sprawdzac czy wiadomosc jest zle sformatowana?

    // obsluga wiadomosci
    message_t buf;
    socklen_t addrlen;
    struct sockaddr_in client_addr;
    int n; // size of gotten DGRAM
    // obsluga watkow
    pthread_t aduincts[5];


    // tworzenei socketu i bindowanie
    int sock = create_and_bind_udp_socket(atoi(port));
    while (1) {
        n = recvfrom(sock, &buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &addrlen); 
        if (n < 0) 
            ERR("recvfrom");

        print_message(buf, n);
         // castujemy, bo recvfrom jest uniwersalne
    }


}