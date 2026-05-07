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

int main(int argc, char** argv) {
    if (argc < 5) exit(EXIT_SUCCESS);
    char* address = argv[1];
    //int32_t portNumber = atoi(argv[2]);
    int32_t operand1 = atoi(argv[3]);
    int32_t operand2 = atoi(argv[4]);
    int32_t op = argv[5][0];

    if (op != '+' && op != '-' && op != '*' && op != '/') {
        fprintf(stderr, "Invalid operator: %s\n", argv[5]);
        return EXIT_FAILURE;
    }

    message_t msg;
    msg.op1 = operand1;
    msg.op2 = operand2;
    msg.op = op;
    msg.result = 0;
    msg.status = 0;

    // unix
    int s_fd = connect_local_socket(address);
    msg_to_network(&msg);

    if (bulk_write(s_fd, (char*)&msg, sizeof(msg)) < 0) ERR("write");

    if (bulk_read(s_fd, (char *)&msg, sizeof(msg)) != sizeof(msg)) {
        ERR("bulk read");
    }

    msg_to_host(&msg);

    if (msg.status == 1) {
        printf("%d\n", msg.result);
    } else {
        printf("Msg Status: %d", msg.status);
    }

    close(s_fd);

}