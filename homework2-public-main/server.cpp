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
#include <netinet/tcp.h>

#include "helpers.h"
#include "common.h"
#define MAX_CONNECTIONS 10

struct udp_message {
    char topic[51];
    unsigned int date_type;
    char content[1501];
};

void create_socket_tcp(int *listenfd, int port)
{
    *listenfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(*listenfd < 0, "socket");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    int enable = 1;
    if (setsockopt(*listenfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    

    // Asociem adresa serverului cu socketul creat folosind bind
    int rc = bind(*listenfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind");

    // Setam socket-ul listenfd pentru ascultare
    rc = listen(*listenfd, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");
}

void create_socket_udp(int *socket_udp, int port)
{
    // Creating socket file descriptor
    if ((*socket_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* Make ports reusable, in case we run this really fast two times in a row */
    int enable = 1;
    if (setsockopt(*socket_udp, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // Fill the details on what destination port should the
    // datagrams have to be sent to our process.
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // IPv4
    /* 0.0.0.0, basically match any IP */
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // Bind the socket with the server address. The OS networking
    // implementation will redirect to us the contents of all UDP
    // datagrams that have our port as destination
    int rc = bind(*socket_udp, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind failed");
}

int main(int argc, char *argv[]) {
    if (argc != 2)
    {
        printf("\n Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    printf("%hu\n", port);

    struct pollfd poll_fds[MAX_CONNECTIONS];
    int nfds = 0;
    

    struct chat_packet received_packet;
    int listenfd;
    create_socket_tcp(&listenfd, port);

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
    // multimea read_fds
    poll_fds[nfds].fd = listenfd;
    poll_fds[nfds].events = POLLIN;
    nfds++;

    int socket_udp;
    create_socket_udp(&socket_udp, port);

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
    // multimea read_fds
    poll_fds[nfds].fd = socket_udp;
    poll_fds[nfds].events = POLLIN;
    nfds++;

    poll_fds[nfds].fd = 0; // stdin
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
                if (poll_fds[i].fd == listenfd)
                {
                    // a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
                    // pe care serverul o accepta
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    printf("sad\n");
                    int newsockfd =
                        accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    // se adauga noul socket intors de accept() la multimea descriptorilor
                    // de citire
                    poll_fds[nfds].fd = newsockfd;
                    poll_fds[nfds].events = POLLIN;
                    nfds++;

                    printf("Noua conexiune de la %s, port %d, socket client %d\n",
                           inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),
                           newsockfd);
                }
                else if (poll_fds[i].fd == socket_udp)
                {
                    struct sockaddr_in client_addr;
                    udp_message p;
                    socklen_t clen = sizeof(client_addr);
                    int rc = recvfrom(socket_udp, &p, sizeof(udp_message), 0,
                                      (struct sockaddr *)&client_addr, &clen);
                    printf("%s\n", p.topic);
                }
                else
                {
                    // // s-au primit date pe unul din socketii de client,
                    // // asa ca serverul trebuie sa le receptioneze
                    // int rc = recv_all(poll_fds[i].fd, &received_packet,
                    //                   sizeof(received_packet));
                    // DIE(rc < 0, "recv");

                    if (rc == 0)
                    {
                        // conexiunea s-a inchis
                        printf("Socket-ul client %d a inchis conexiunea\n", i);
                        close(poll_fds[i].fd);

                        // se scoate din multimea de citire socketul inchis
                        for (int j = i; j < nfds - 1; j++)
                        {
                            poll_fds[j] = poll_fds[j + 1];
                        }

                        nfds--;
                    }
                    else
                    {
                        printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n",
                               poll_fds[i].fd, received_packet.message);
                        // /* TODO 2.1: Trimite mesajul catre toti ceilalti clienti, mai putin pe unde a venit */
                        // for (int j = 0; j < nfds; ++j)
                        // {
                        //     if (poll_fds[j].fd != listenfd && poll_fds[j].fd != poll_fds[i].fd)
                        //     {
                        //         int rc = send_all(poll_fds[j].fd, &received_packet, sizeof(received_packet));
                        //         DIE(rc < 0, "send_all()");
                        //     }
                        // }
                    }
                }
            }
        }
    }

    // Inchidem listenfd
    close(listenfd);

    return 0;
}
