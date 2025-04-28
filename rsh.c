#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {
    "cp", "touch", "mkdir", "ls", "pwd",
    "cat", "grep", "chmod", "diff", "cd",
    "exit", "help", "sendmsg"
};

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

void sendmsg(char *user, char *target, char *msg) {
    struct message req;
    strcpy(req.source, user);
    strcpy(req.target, target);
    strcpy(req.msg, msg);

    int server = open("serverFIFO", O_WRONLY);
    if (server != -1) {
        write(server, &req, sizeof(req));
        close(server);
    }
}

void *messageListener(void *arg) {
    char *pipeName = (char *)arg;
    struct message incomingMsg;
    int userFifo = open(pipeName, O_RDONLY);

    while (1) {
        int bytesRead = read(userFifo, &incomingMsg, sizeof(struct message));
        if (bytesRead > 0) {
            printf("Incoming message from %s: %s\n", incomingMsg.source, incomingMsg.msg);
            fflush(stdout);
        }
    }
    close(userFifo);
    pthread_exit(0);
}

int isAllowed(const char *cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }

    signal(SIGINT, terminate);
    strcpy(uName, argv[1]);

    pthread_t tid;
    pthread_create(&tid, NULL, messageListener, (void *)uName);
    pthread_detach(tid);

    char line[256];
    char **cargv;
    char *path;
    int status;
    posix_spawnattr_t attr;
    pid_t pid;

    while (1) {
        fprintf(stderr, "rsh>");
        if (fgets(line, sizeof(line), stdin) == NULL) continue;
        if (strcmp(line, "\n") == 0) continue;

        line[strcspn(line, "\n")] = '\0';

        char cmd[256], line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " "));

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *target = strtok(NULL, " ");
            if (!target) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }
            char *msg = strtok(NULL, "");
            if (!msg) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }
            sendmsg(uName, target, msg);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else if (targetDir) {
                chdir(targetDir);
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i + 1, allowed[i]);
            }
            continue;
        }

        cargv = malloc(sizeof(char*));
        path = malloc(strlen(cmd) + 1);
        strcpy(path, cmd);
        cargv[0] = malloc(strlen(cmd) + 1);
        strcpy(cargv[0], cmd);

        char *arg = strtok(line2, " ");
        arg = strtok(NULL, " ");
        int n = 1;
        while (arg != NULL) {
            n++;
            cargv = realloc(cargv, sizeof(char*) * n);
            cargv[n - 1] = malloc(strlen(arg) + 1);
            strcpy(cargv[n - 1], arg);
            arg = strtok(NULL, " ");
        }
        cargv = realloc(cargv, sizeof(char*) * (n + 1));
        cargv[n] = NULL;

        posix_spawnattr_init(&attr);
        if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(EXIT_FAILURE);
        }

        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }

        posix_spawnattr_destroy(&attr);
    }

    return 0;
}

