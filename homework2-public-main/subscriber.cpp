#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "helpers.h"
#include "common.h"
#define MAX_CONNECTIONS 10

void create_socket_tcp(int *tcp, char *argv)
{
    *tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(*tcp < 0, "socket");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(&argv[3]));
    serv_addr.sin_addr.s_addr = inet_addr(&argv[2]);

    // Setam socket-ul listenfd pentru ascultare
    int rc = connect(*tcp, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("\n Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    printf("%s %s %s\n", argv[1], argv[2], argv[3]);

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[2], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    struct pollfd poll_fds[MAX_CONNECTIONS];
    int nfds = 0;

    struct chat_packet received_packet;
    int tcp;
    create_socket_tcp(&tcp, *argv);

    poll_fds[nfds].fd = tcp;
    poll_fds[nfds].events = POLLIN;
    nfds++;

    poll_fds[nfds].fd = 0;
    poll_fds[nfds].events = POLLIN;
    nfds++;

    while (1)
    {
        rc = poll(poll_fds, nfds, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < nfds; i++)
        {
            if (poll_fds[i].revents & POLLIN)
            {
                if (poll_fds[i].fd == tcp)
                {
                }
                else if (poll_fds[i].fd == 0)
                {
                    char buf[1501];
                    memset(buf, 0, 1501);
                    int rc = recv(0, buf, 1501, 0);
                    printf("%s\n", buf);
                    rc = send(tcp, buf, 1501, 0);
                }
            }
        }
    }

    return 0;
}