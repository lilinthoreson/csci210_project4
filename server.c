#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server;
    int targetfd;
    int dummyfd;
    struct message req;
    char targetFIFO[100];

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    server = open("serverFIFO", O_RDONLY);
    dummyfd = open("serverFIFO", O_WRONLY);

    while (1) {
        int n = read(server, &req, sizeof(struct message));
        if (n > 0) {
            printf("Received a request from %s to send the message %s to %s.\n", req.source, req.msg, req.target);

            snprintf(targetFIFO, sizeof(targetFIFO), "%s", req.target);

            targetfd = open(targetFIFO, O_WRONLY);
            if (targetfd == -1) {
                perror("open target FIFO failed");
                continue;
            }

            write(targetfd, &req, sizeof(struct message));
            close(targetfd);
        }
    }

    close(server);
    close(dummyfd);
    return 0;
}

