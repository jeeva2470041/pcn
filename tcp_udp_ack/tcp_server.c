/*
 * File: tcp_server.c
 * Description: TCP Server that establishes a connection with a client,
 *              receives data packets, and sends acknowledgements (ACK) back.
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
#define DEFAULT_PORT 8080

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    char ack_msg[BUFFER_SIZE];
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    int opt = 1;
    int packet_seq = 0;

    printf("====================================================\n");
    printf("         TCP PACKET & ACKNOWLEDGEMENT SERVER        \n");
    printf("====================================================\n\n");

    // 1. Create TCP Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[1] socket()     : TCP Socket created successfully (fd=%d)\n", server_fd);

    // 2. Set socket options to reuse address/port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] setsockopt() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Configure Server Address Structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // 4. Bind Socket to IP & Port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[2] bind()       : Bound to 0.0.0.0:%d\n", port);

    // 5. Listen for incoming client connections
    if (listen(server_fd, 5) < 0) {
        perror("[-] listen() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[3] listen()     : Server listening for client connection...\n\n");

    // 6. Accept Client Connection
    client_len = sizeof(client_addr);
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("[-] accept() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[+] Connection Established with Client!\n");
    printf("    -> Client IP   : %s\n", inet_ntoa(client_addr.sin_addr));
    printf("    -> Client Port : %d\n", ntohs(client_addr.sin_port));
    printf("----------------------------------------------------\n\n");

    // 7. Receive Packets and Send Acknowledgements
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_recv = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recv <= 0) {
            printf("[*] Client disconnected or closed connection.\n");
            break;
        }

        buffer[bytes_recv] = '\0';

        // Check for termination signal
        if (strcasecmp(buffer, "exit") == 0 || strcasecmp(buffer, "quit") == 0) {
            printf("[*] Client requested termination. Closing session.\n");
            break;
        }

        packet_seq++;
        printf("[RECEIVE] Packet #%d received from client:\n", packet_seq);
        printf("          Data    : \"%s\"\n", buffer);
        printf("          Length  : %zd bytes\n", bytes_recv);

        // Prepare ACK packet
        snprintf(ack_msg, sizeof(ack_msg), "ACK #%d: Packet received successfully (%zd bytes)", 
                 packet_seq, bytes_recv);

        // Send ACK back to Client
        if (send(client_fd, ack_msg, strlen(ack_msg), 0) < 0) {
            perror("[-] send() ACK failed");
            break;
        }
        printf("[SEND]    Acknowledgement sent to client: \"%s\"\n\n", ack_msg);
    }

    // 8. Close sockets
    close(client_fd);
    close(server_fd);
    printf("[-] Sockets closed. Server terminated gracefully.\n");
    return 0;
}
