// #include "../l8_common.h"
#include "../w8-common.h"
#define STACK_SIZE 16
#include <pthread.h>
#define DIVISION_NAMES_SIZE 128

void sleep_ms(int ms) {
    struct timespec req, rem;
    
    // Wyliczamy sekundy i resztę w nanosekundach
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;

    // Pętla na wypadek, gdyby nanosleep został przerwany przez sygnał (np. SIGINT)
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        // Jeśli przerwano, kontynuujemy spanie przez pozostały czas (rem)
        req = rem;
    }
}

typedef struct { // 576
    // przez to, ze te struktury sa jednobajtowe, to nie trzeba ich odwracac!
    int8_t X;
    int8_t Y;
    int8_t P; // 0 - wrogi, 1 - sojuszniczy
    char oddzial[128];
    // wiadomosc oznacza, ze podany oddzial przesunal sie na dana pozycje.
    struct sockaddr_in adres; // jest doklejane na koniec wiec spoko
} __attribute__((packed)) message_t; // packed bo testowanie netcatem jest spierdolone (mimikujemy strukture)

typedef struct {
    message_t data[STACK_SIZE];
    int top;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    char* divisionNames[DIVISION_NAMES_SIZE];
    int map[100*100];
    pthread_mutex_t mapMtxs[100 * 100];
    struct sockaddr_in addresses[DIVISION_NAMES_SIZE];
    int przeciwko[DIVISION_NAMES_SIZE];
    int server_sock; // dla napoleona, zeby mial jak wysalc
} adiutant_stack_t;

void print_message(message_t mes) {
    char* rodzajOddzialu = "nasz";
    if (mes.P == 1) rodzajOddzialu = "wrogi"; 
    printf("%s oddzial %s byl widziany na pozycji %d:%d\n", 
        rodzajOddzialu, mes.oddzial, mes.X, mes.Y);
}

int AddNameIfDoesntExist(char **names, char* name) {
    for (int i = 0; i < DIVISION_NAMES_SIZE; i++) {
        if (names[i]==NULL) {
            names[i] = strdup(name); // kopia napisu
            return i;
        } else if (strcmp(names[i], name) == 0) return i;
    }

    return -1;
}

void ResetLastPosition(int *map, int idx) { // tu powinien byc mutex na sprawdzanym elemencie mapy ale juz jebac to
    for (int i = 0; i < 100; i++)
        for (int j = 0; j < 100; j++)
            if (map[i * 100 + j] == idx) {
                map[i * 100 + j] = -1;
                return;
            }
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

        // e3
        pthread_mutex_lock(&(adiutant_stack->mapMtxs[personal_message.X * 100 + personal_message.Y]));

        int oddzial_idx = AddNameIfDoesntExist(adiutant_stack->divisionNames, personal_message.oddzial);
        if (oddzial_idx == -1) {
            pthread_mutex_unlock(&(adiutant_stack->mapMtxs[personal_message.X * 100 + personal_message.Y]));
            pthread_mutex_unlock(&(adiutant_stack->mtx));
            ERR("AddNameIfDoesntExist");
        }
        ResetLastPosition(adiutant_stack->map, oddzial_idx);
        adiutant_stack->map[personal_message.X * 100 + personal_message.Y] = oddzial_idx;

        // e4
        adiutant_stack->przeciwko[oddzial_idx] = personal_message.P;
        adiutant_stack->addresses[oddzial_idx] = personal_message.adres;

        pthread_mutex_unlock(&(adiutant_stack->mapMtxs[personal_message.X * 100 + personal_message.Y]));
        
        pthread_mutex_unlock(&(adiutant_stack->mtx));
        sleep_ms(50);
        print_message(personal_message);
    }
    return NULL;
}

sig_atomic_t Work = 1;

void sigintHandler(int sig) {
    if (sig == SIGINT) 
        Work = 0;
}

// czacik
void* napoleon_thread(void* arg) {
    adiutant_stack_t *stack = (adiutant_stack_t*)arg;
    srand(time(NULL));

    while (1) {
        sleep_ms(5000);

        printf("\n=== [MAPA NAPOLEONA] ===\n");
        int enemy_indices[DIVISION_NAMES_SIZE];
        int enemy_count = 0;
        int active_count = 0;

        // Napoleon przegląda stan struktur
        pthread_mutex_lock(&stack->mtx); // Blokujemy na chwilę odczyt struktur globalnych
        for (int i = 0; i < DIVISION_NAMES_SIZE; i++) {
            if (stack->divisionNames[i] != NULL) {
                active_count++;
                
                // Szukamy aktualnej pozycji na mapie
                int curr_X = -1, curr_Y = -1;
                for (int m = 0; m < 100*100; m++) {
                    if (stack->map[m] == i) {
                        curr_X = m / 100;
                        curr_Y = m % 100;
                        break;
                    }
                }

                printf("[%02d] Oddzial: %-15s | Status: %s | Ostatnia Pozycja: %d:%d\n", 
                       i, stack->divisionNames[i], 
                       stack->przeciwko[i] == 0 ? "WROG" : "SOJUSZNIK",
                       curr_X, curr_Y);

                // Zbieramy indeksy wrogów (zgodnie z Twoim warunkiem: "jeśli wrogie to wysyła")
                if (stack->przeciwko[i] == 0) {
                    enemy_indices[enemy_count] = i;
                    enemy_count++;
                }
            }
        }
        if (active_count == 0) printf("(Brak jednostek na mapie)\n");

        // Jeśli Cesarz wykrył wrogów, wybiera losowego i wysyła rozkaz/atak
        if (enemy_count > 0) {
            int random_idx = enemy_indices[rand() % enemy_count];
            
            message_t order;
            order.X = rand() % 100;
            order.Y = rand() % 100;
            order.P = 0; // Atakujemy wroga
            strncpy(order.oddzial, stack->divisionNames[random_idx], 128);

            // Wysyłamy pakiet na zapamiętany w tablicy adres
            sendto(stack->server_sock, &order, sizeof(message_t), 0, 
                   (struct sockaddr*)&(stack->addresses[random_idx]), sizeof(struct sockaddr_in));

            printf("[ATAK] Napoleon wyslal oddzial przeciwko wrogowi [%s] na pozycje %d:%d\n", 
                   order.oddzial, order.X, order.Y);
        }
        pthread_mutex_unlock(&stack->mtx);
    }
    return NULL;
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
    pthread_t napoleon_id;

    adiutant_stack_t adiutant_stack = {
        .top = 0,
        .cond = PTHREAD_COND_INITIALIZER,
        .mtx = PTHREAD_MUTEX_INITIALIZER,
        .map = {-1},
        .addresses = {0},
        .przeciwko = {-1}
    };
    // lub
    // adiutant_stack.top = 0;
    // pthread_mutex_init(&adiutant_stack.mtx, NULL);
    // pthread_cond_init(&adiutant_stack.cond, NULL);

    for (int i = 0; i < DIVISION_NAMES_SIZE; i++) 
        adiutant_stack.divisionNames[i] = NULL;

    for (int i = 0; i < DIVISION_NAMES_SIZE; i++) {
        adiutant_stack.divisionNames[i] = NULL;
        adiutant_stack.przeciwko[i] = -1;
        memset(&adiutant_stack.addresses[i], 0, sizeof(struct sockaddr_in));
    }

    // tworzenei socketu i bindowanie
    int sock = create_and_bind_udp_socket(atoi(port));
    adiutant_stack.server_sock = sock;

    for (int i = 0; i < 5; i++) {
        pthread_create(&adiuncts[i], NULL, thread_work, &adiutant_stack);
    }

    pthread_create(&napoleon_id, NULL, napoleon_thread, &adiutant_stack);

    while (Work) {
        memset(&buf, 0, sizeof(message_t)); // Czyścimy całą strukturę zerami
        // recvfrom blokujace
        n = recvfrom(sock, &buf, 131, 0, (struct sockaddr*)&client_addr, &addrlen); 
        // castujemy, bo recvfrom jest uniwersalne
        if (n < 0) 
            ERR("recvfrom");

        // Dokładna walidacja pakietu binarnego
        if (n < 4) continue; // Za krótki, żeby być poprawnym pakietem
        if (buf.P != 0 && buf.P != 1) continue; // Jeśli P to nie 0 ani 1, to znaczy że to losowy tekst!
        if (buf.X < 0 || buf.X >= 100 || buf.Y < 0 || buf.Y >= 100) continue;

        buf.adres = client_addr;

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

    for (int i = 0; i < 5; i++) {
        pthread_cancel(adiuncts[i]); // wyrok smierci
        pthread_join(adiuncts[i], NULL);
    }
        
    pthread_cancel(napoleon_id);
    pthread_join(napoleon_id, NULL);

    for (int i = 0; i < DIVISION_NAMES_SIZE; i++) {
        if (adiutant_stack.divisionNames[i] != NULL) {
            free(adiutant_stack.divisionNames[i]);
        }
    }
    
    close(sock);
    return EXIT_SUCCESS;
}