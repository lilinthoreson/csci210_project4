#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod", "diff", "cd", "exit", "help", "sendmsg"};

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
    int serverfd;
    struct message m;
    strcpy(m.source, user);
    strcpy(m.target, target);
    strcpy(m.msg, msg);

    serverfd = open("serverFIFO", O_WRONLY);
    if (serverfd == -1) {
        perror("open serverFIFO failed");
        return;
    }

    write(serverfd, &m, sizeof(struct message));
    close(serverfd);
}

void* messageListener(void *arg) {
    int userfd;
    struct message m;

    char fifoName[100];
    snprintf(fifoName, sizeof(fifoName), "%s", uName);

    userfd = open(fifoName, O_RDONLY);
    if (userfd == -1) {
        perror("open user FIFO failed");
        pthread_exit((void*)1);
    }

    while (1) {
        int n = read(userfd, &m, sizeof(struct message));
        if (n > 0) {
            printf("\nIncoming message from %s: %s\n", m.source, m.msg);
            printf("rsh>");
            fflush(stdout);
        }
    }
    close(userfd);
    pthread_exit((void*)0);
}

int isAllowed(const char* cmd) {
    int i;
    for (i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv;
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;
    pthread_t tid;

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }
    signal(SIGINT, terminate);

    strcpy(uName, argv[1]);

    // Create the message listener thread
    if (pthread_create(&tid, NULL, messageListener, NULL) != 0) {
        perror("pthread_create failed");
        exit(1);
    }

    while (1) {
        fprintf(stderr, "rsh>");

        if (fgets(line, 256, stdin) == NULL) continue;

        if (strcmp(line, "\n") == 0) continue;

        line[strlen(line) - 1] = '\0'; // remove newline

        char cmd[256];
        char line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " "));

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *target = strtok(NULL, " ");
            if (target == NULL) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }

            char *message = strtok(NULL, "");
            if (message == NULL) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }

            sendmsg(uName, target, message);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else {
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

        cargv = (char**)malloc(sizeof(char*));
        cargv[0] = (char *)malloc(strlen(cmd) + 1);
        path = (char *)malloc(strlen(cmd) + 1);
        strcpy(path, cmd);
        strcpy(cargv[0], cmd);

        char *attrToken = strtok(line2, " "); // skip first word (cmd)
        attrToken = strtok(NULL, " ");
        int n = 1;
        while (attrToken != NULL) {
            n++;
            cargv = (char**)realloc(cargv, sizeof(char*) * n);
            cargv[n - 1] = (char *)malloc(strlen(attrToken) + 1);
            strcpy(cargv[n - 1], attrToken);
            attrToken = strtok(NULL, " ");
        }
        cargv = (char**)realloc(cargv, sizeof(char*) * (n + 1));
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
