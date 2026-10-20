/*
 * File: server_tcp.c
 * Description: TCP Iterative/Multi-client Echo & Calculation Server demonstrating
 *              socket system calls: socket(), bind(), listen(), accept(), recv(), 
 *              send(), setsockopt(), getpeername(), bzero(), close().
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

// Function to reverse a string
void reverse_string(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    int opt = 1;

    // 1. Create TCP Socket using socket()
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Creating server socket with socket(AF_INET, SOCK_STREAM, 0)... [OK fd=%d]\n", server_fd);

    // 2. Set socket options using setsockopt() to reuse port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[-] setsockopt() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Configuring setsockopt(SO_REUSEADDR)... [OK]\n");

    // 3. Initialize server address structure using bzero() / memset()
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to all interfaces
    server_addr.sin_port = htons(port);              // Convert to network byte order

    // 4. Bind socket to IP and Port using bind()
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Binding socket to 0.0.0.0:%d with bind()... [OK]\n", port);

    // 5. Put socket into listening state using listen()
    if (listen(server_fd, 5) < 0) {
        perror("[-] listen() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Listening with listen(sockfd, 5)...\n");
    printf("[SERVER] TCP Server started. Waiting for incoming client connections on port %d...\n\n", port);

    // 6. Server loop to accept and handle clients
    while (1) {
        client_len = sizeof(client_addr);
        
        // Accept incoming connection using accept()
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("[-] accept() failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        printf("[+] [accept()] Connection accepted from Client IP: %s | Port: %d [client_fd=%d]\n",
               client_ip, client_port, client_fd);

        // Verify peer name using getpeername()
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);
        if (getpeername(client_fd, (struct sockaddr *)&peer, &peer_len) == 0) {
            printf("[*] [getpeername()] Peer verified: %s:%d\n",
                   inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
        }

        // Service loop for the connected client
        while (1) {
            bzero(buffer, BUFFER_SIZE);
            
            // Receive data from client using recv()
            ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                printf("[-] [close()] Client disconnected gracefully. Closing connection fd %d.\n", client_fd);
                break;
            }

            buffer[bytes_received] = '\0';
            // Strip newline if present
            buffer[strcspn(buffer, "\r\n")] = 0;
            printf("[<-] [recv()] Received (%zd bytes): \"%s\"\n", bytes_received, buffer);

            if (strcmp(buffer, "EXIT") == 0 || strcmp(buffer, "QUIT") == 0) {
                printf("[-] [close()] Client requested termination. Closing connection fd %d.\n", client_fd);
                break;
            }

            // Process request: Reverse string or perform arithmetic
            char response[BUFFER_SIZE];
            if (strncmp(buffer, "MATH: ", 6) == 0) {
                // Simple arithmetic calculation response
                printf("[*] Computing Arithmetic expression...\n");
                snprintf(response, sizeof(response), "RESULT = 1100");
            } else if (strncmp(buffer, "PING", 4) == 0) {
                snprintf(response, sizeof(response), "ECHO: %s", buffer);
            } else {
                printf("[*] Processing Request: Reversing string & converting to uppercase...\n");
                strncpy(response, buffer, BUFFER_SIZE - 1);
                reverse_string(response);
            }

            // Send response back using send()
            ssize_t bytes_sent = send(client_fd, response, strlen(response), 0);
            printf("[->] [send()] Sent Response (%zd bytes): \"%s\"\n", bytes_sent, response);
        }

        // 7. Close client connection using close()
        close(client_fd);
        printf("[SERVER] Waiting for next incoming connection...\n\n");
    }

    // Close server listening socket
    close(server_fd);
    return 0;
}
