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
} adiutant_stack_t;

void print_message(message_t mes) {
    char* rodzajOddzialu = "nasz";
    if (mes.P == 1) rodzajOddzialu = "wrogi"; 
    printf("%s oddzial %s byl widziany na pozycji %d:%d\n", 
        rodzajOddzialu, mes.oddzial, mes.X, mes.Y);
}

void* thread_work(void *arg) {
    adiutant_stack_t *adiutant_stack = (adiutant_stack_t*)arg;
    message_t personal_message;

    while (1) {
        pthread_mutex_lock(&(adiutant_stack->mtx)); // blokuje bo chce sprawdzic stos
        while (adiutant_stack->top == 0) {
            pthread_cond_wait(&(adiutant_stack->cond), &(adiutant_stack->mtx)); // zwalnia mutex i usypia sie
            // po obudzeniu przez system (cond_signal) blokuje mutex
        }
        adiutant_stack->top--;
        memcpy(&personal_message, &(adiutant_stack->data[adiutant_stack->top]), sizeof(message_t));
        // lub *personal_message = adiutant_stack->data[adiutant_stack->top];
        pthread_mutex_unlock(&(adiutant_stack->mtx));

        print_message(personal_message);
    }
    return NULL;
}

sig_atomic_t Work = 1;

void sigintHandler(int sig) {
    if (sig == SIGINT) 
        Work = 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Użycie: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* port = argv[1];
    // Jak sprawdzac czy wiadomosc jest zle sformatowana?
    sethandler(sigintHandler, SIGINT);

    // obsluga wiadomosci
    message_t buf;
    socklen_t addrlen;
    struct sockaddr_in client_addr;
    int n; // size of gotten DGRAM
    // obsluga watkow
    pthread_t adiuncts[5];
    adiutant_stack_t adiutant_stack = {
        .top = 0,
        .cond = PTHREAD_COND_INITIALIZER,
        .mtx = PTHREAD_MUTEX_INITIALIZER
    };
    // lub
    // adiutant_stack.top = 0;
    // pthread_mutex_init(&adiutant_stack.mtx, NULL);
    // pthread_cond_init(&adiutant_stack.cond, NULL);

    for (int i = 0; i < 5; i++) {
        pthread_create(&adiuncts[i], NULL, thread_work, &adiutant_stack);
    }

    // tworzenei socketu i bindowanie
    int sock = create_and_bind_udp_socket(atoi(port));
    while (Work) {
        memset(&buf, 0, sizeof(message_t)); // Czyścimy całą strukturę zerami
        // recvfrom blokujace
        n = recvfrom(sock, &buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &addrlen); 
        // castujemy, bo recvfrom jest uniwersalne
        if (n < 0) 
            ERR("recvfrom");

        pthread_mutex_lock(&adiutant_stack.mtx);

        if (adiutant_stack.top < 16) {
            memcpy(&(adiutant_stack.data[adiutant_stack.top]), &buf, sizeof(message_t));
            adiutant_stack.top++;

            pthread_cond_signal(&adiutant_stack.cond);
        }

        pthread_mutex_unlock(&adiutant_stack.mtx);
        // push message na stos
        // sygnal na conditional vari
    }

    for (int i = 0; i < 5; i++) 
        pthread_join(adiuncts[i], NULL);

    
}