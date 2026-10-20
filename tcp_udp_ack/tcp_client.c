/*
 * File: tcp_client.c
 * Description: TCP Client that connects to the server, sends data packets,
 *              and waits for acknowledgements (ACK) from the server.
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
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_PORT 8080

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char input_pkt[BUFFER_SIZE];
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_SERVER_IP;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
    int packet_seq = 0;

    printf("====================================================\n");
    printf("         TCP PACKET & ACKNOWLEDGEMENT CLIENT        \n");
    printf("====================================================\n\n");

    // 1. Create TCP Socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[1] socket()  : Client socket created successfully (fd=%d)\n", sock_fd);

    // 2. Configure Server Address Structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("[-] inet_pton() failed: Invalid IP address");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Connect to Server (TCP 3-Way Handshake)
    printf("[2] connect() : Connecting to Server %s:%d...\n", server_ip, port);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] connect() failed: Server is not running or unreachable");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[+] Connected to Server successfully!\n");
    printf("----------------------------------------------------\n");
    printf("Type a message to send as a packet (or type 'exit' to quit):\n\n");

    // 4. Send Packets and Receive Acknowledgements (ACK)
    while (1) {
        printf("Enter Packet Data: ");
        if (!fgets(input_pkt, sizeof(input_pkt), stdin)) {
            break;
        }

        // Remove newline character
        input_pkt[strcspn(input_pkt, "\r\n")] = '\0';

        if (strlen(input_pkt) == 0) {
            continue;
        }

        // Send Packet to Server
        if (send(sock_fd, input_pkt, strlen(input_pkt), 0) < 0) {
            perror("[-] send() failed");
            break;
        }

        // Check if user entered exit/quit
        if (strcasecmp(input_pkt, "exit") == 0 || strcasecmp(input_pkt, "quit") == 0) {
            printf("[*] Exiting client session...\n");
            break;
        }

        packet_seq++;
        printf("[SEND]    Packet #%d sent to server: \"%s\" (%zu bytes)\n", 
               packet_seq, input_pkt, strlen(input_pkt));

        // Wait for Acknowledgement (ACK) from Server
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_recv = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recv <= 0) {
            printf("[-] Server disconnected before sending ACK.\n");
            break;
        }

        buffer[bytes_recv] = '\0';
        printf("[RECEIVE] %s\n\n", buffer);
    }

    // 5. Close connection
    close(sock_fd);
    printf("[-] Connection closed. Client terminated.\n");
    return 0;
}
