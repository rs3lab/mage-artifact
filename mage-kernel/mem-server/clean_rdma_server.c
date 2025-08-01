#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "rdma_server.h"

void handle_signal(int signal) {
    server_shutdown();
    exit(signal);
}

int main(int argc, char **argv)
{
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <IP Address>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);

    printf("Start initializing the server\n");
    server_init(argv[1], MIND_DEFAULT_CONTROL_PORT);

    printf("Waiting for server completion\n");
    wait_for_server_done();

    printf("Shutting down server.\n");
    server_shutdown();
    return 0;
}
