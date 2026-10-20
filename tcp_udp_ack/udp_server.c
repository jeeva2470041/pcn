/*
 * File: udp_server.c
 * Description: UDP Server that listens for datagram packets from a client,
 *              logs the client connection details, and sends an ACK back to the client.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 9090

int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    char ack_msg[BUFFER_SIZE];
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    int packet_seq = 0;

    printf("====================================================\n");
    printf("         UDP PACKET & ACKNOWLEDGEMENT SERVER        \n");
    printf("====================================================\n\n");

    // 1. Create UDP Socket (SOCK_DGRAM)
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[1] socket()   : UDP Socket created successfully (fd=%d)\n", server_fd);

    // 2. Configure Server Address Structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // 3. Bind Socket to IP & Port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[2] bind()     : Bound to 0.0.0.0:%d\n", port);
    printf("[*] Server is waiting for incoming UDP packets...\n");
    printf("----------------------------------------------------\n\n");

    // 4. Receive Packets and Send Acknowledgements
    while (1) {
        client_len = sizeof(client_addr);
        memset(buffer, 0, BUFFER_SIZE);

        ssize_t bytes_recv = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0,
                                      (struct sockaddr *)&client_addr, &client_len);

        if (bytes_recv < 0) {
            perror("[-] recvfrom() failed");
            continue;
        }

        buffer[bytes_recv] = '\0';

        // Check for termination packet
        if (strcasecmp(buffer, "exit") == 0 || strcasecmp(buffer, "quit") == 0) {
            printf("[*] Client [%s:%d] sent exit signal. Server stopping.\n",
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            break;
        }

        packet_seq++;
        printf("[RECEIVE] UDP Packet #%d received:\n", packet_seq);
        printf("          Source  : %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("          Data    : \"%s\"\n", buffer);
        printf("          Length  : %zd bytes\n", bytes_recv);

        // Prepare UDP Acknowledgement
        snprintf(ack_msg, sizeof(ack_msg), "ACK #%d: UDP packet received successfully (%zd bytes)",
                 packet_seq, bytes_recv);

        // Send ACK back to the sender
        ssize_t sent = sendto(server_fd, ack_msg, strlen(ack_msg), 0,
                              (struct sockaddr *)&client_addr, client_len);
        if (sent < 0) {
            perror("[-] sendto() ACK failed");
        } else {
            printf("[SEND]    Acknowledgement sent to client: \"%s\"\n\n", ack_msg);
        }
    }

    // 5. Close server socket
    close(server_fd);
    printf("[-] UDP Server closed successfully.\n");
    return 0;
}
