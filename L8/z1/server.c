// #include "../l8_common.h"
#include "../w8-common.h"
#define STACK_SIZE 16
#include <pthread.h>

typedef struct { // 576
    // przez to, ze te struktury sa jednobajtowe, to nie trzeba ich odwracac!
    int8_t X;
    int8_t Y;
    int8_t P; // 0 - wrogi, 1 - sojuszniczy
    char oddzial[128];
    // wiadomosc oznacza, ze podany oddzial przesunal sie na dana pozycje.
} message_t;

typedef struct {
    message_t data[STACK_SIZE];
    int top;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
} stack_t;

void print_message(message_t mes, int n) {
    char* rodzajOddzialu = "nasz";
    if (mes.P == 1) rodzajOddzialu = "wrogi"; 
    printf("%s oddzial %.*s byl widziany na pozycji %d:%d", 
        rodzajOddzialu, (int)(n - 3), mes.oddzial, mes.X, mes.Y);
}

void* thread_work(void *arg) {
    stack_t *stack = (stack_t*)arg;
    message_t personal_message;

    pthread_mutex_lock(&(stack->mtx)); // blokuje bo chce sprawdzic stos
    while (stack->top == 0) {
        pthread_cond_wait(&(stack->cond), &(stack->mtx)); // zwalnia mutex i usypie
    }
    // po obudzeniu blokuje

    pthread_mutex_unlock(&(stack->mtx));
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
    pthread_t adiuncts[5];
    stack_t stack {
        .top = 0;
        .cond = PTHREAD_COND_INITIALIZER,
        .mtx = PTHREAD_MUTEX_INITIALIZER
    };
    // lub
    // stack.top = 0;
    // pthread_mutex_init(&stack.mtx, NULL);
    // pthread_cond_init(&stack.cond, NULL);

    for (int i = 0; i < 5; i++) {
        pthread_create(&adiuncts[i], NULL, thread_work, &stack);
    }

    // tworzenei socketu i bindowanie
    int sock = create_and_bind_udp_socket(atoi(port));
    while (1) {
        n = recvfrom(sock, &buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &addrlen); 
        // castujemy, bo recvfrom jest uniwersalne
        if (n < 0) 
            ERR("recvfrom");

        // push message na stos
        // sygnal na conditional vari
         
    }


}