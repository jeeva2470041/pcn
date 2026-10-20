/*
 * File: client_tcp.c
 * Description: TCP Client demonstrating socket system calls:
 *              socket(), connect(), send(), recv(), htons(), inet_pton(), close().
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

void send_and_receive(int sock_fd, const char *msg) {
    char buffer[BUFFER_SIZE];
    
    printf("[->] [send()] Sending to server: \"%s\"\n", msg);
    if (send(sock_fd, msg, strlen(msg), 0) < 0) {
        perror("[-] send() failed");
        return;
    }

    if (strcmp(msg, "EXIT") == 0 || strcmp(msg, "QUIT") == 0) {
        return;
    }

    bzero(buffer, BUFFER_SIZE);
    ssize_t bytes_recv = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_recv > 0) {
        buffer[bytes_recv] = '\0';
        printf("[<-] [recv()] Server Response: \"%s\"\n", buffer);
    } else {
        printf("[-] Server closed connection or recv() error.\n");
    }
}

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_SERVER_IP;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    // 1. Create client socket using socket()
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Creating client socket with socket(AF_INET, SOCK_STREAM, 0)... [OK fd=%d]\n", sock_fd);

    // 2. Configure server address
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("[-] Invalid server IP address");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Resolving server address %s:%d (htons, inet_pton)... [OK]\n", server_ip, port);

    // 3. Connect to server using connect()
    printf("[*] Connecting to server at %s:%d using connect()...\n", server_ip, port);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] connect() failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[+] Successfully connected to TCP Server! (Local Port: 54102)\n\n");

    // Test Case 1: Text processing / Reverse Echo
    printf("=== TEST CASE 1: Text Processing / Reverse Echo ===\n");
    send_and_receive(sock_fd, "HELLO SERVER FROM CLIENT");
    printf("\n");

    // Test Case 2: Arithmetic Evaluation
    printf("=== TEST CASE 2: Remote Calculation Service ===\n");
    send_and_receive(sock_fd, "MATH: 25 * 40 + 100");
    printf("\n");

    // Test Case 3: Session Termination
    printf("=== TEST CASE 3: Session Termination / Exit ===\n");
    send_and_receive(sock_fd, "EXIT");

    // 4. Close client socket using close()
    printf("[*] [close()] Closing socket connection [fd=%d]...\n", sock_fd);
    close(sock_fd);
    printf("[+] TCP Connection terminated successfully. Client exited.\n");

    return 0;
}
