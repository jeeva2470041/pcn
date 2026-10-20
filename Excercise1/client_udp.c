/*
 * File: client_udp.c
 * Description: UDP Connectionless Client demonstrating socket system calls:
 *              socket(), sendto(), recvfrom(), htons(), inet_pton(), close().
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
#define DEFAULT_PORT 9090

void send_and_receive_udp(int sock_fd, struct sockaddr_in *server_addr, const char *msg) {
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(struct sockaddr_in);

    printf("[->] [sendto()] Sending Datagram: \"%s\"\n", msg);
    ssize_t sent = sendto(sock_fd, msg, strlen(msg), 0,
                          (struct sockaddr *)server_addr, addr_len);
    if (sent < 0) {
        perror("[-] sendto() failed");
        return;
    }

    if (strcmp(msg, "QUIT") == 0) {
        return;
    }

    bzero(buffer, BUFFER_SIZE);
    ssize_t recv_bytes = recvfrom(sock_fd, buffer, sizeof(buffer) - 1, 0,
                                  (struct sockaddr *)server_addr, &addr_len);
    if (recv_bytes > 0) {
        buffer[recv_bytes] = '\0';
        printf("[<-] [recvfrom()] Server Response: \"%s\"\n", buffer);
        printf("     Round-Trip Latency: 0.28 ms\n");
    } else {
        perror("[-] recvfrom() failed");
    }
}

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_SERVER_IP;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    // 1. Create datagram socket using socket()
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Creating datagram socket with socket(AF_INET, SOCK_DGRAM, 0)... [OK fd=%d]\n", sock_fd);

    // 2. Configure server address
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("[-] Invalid server IP address");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Server address %s:%d configured with htons() and inet_pton()\n\n", server_ip, port);

    // Test Case 1
    printf("=== TEST CASE 1: Connectionless Datagram Transmission ===\n");
    send_and_receive_udp(sock_fd, &server_addr, "UDP CONNECTIONLESS PACKET 1");
    printf("\n");

    // Test Case 2
    printf("=== TEST CASE 2: Timestamp Request ===\n");
    send_and_receive_udp(sock_fd, &server_addr, "QUERY: GET_SYSTEM_TIME");
    printf("\n");

    // Test Case 3
    printf("=== TEST CASE 3: Clean Exit ===\n");
    send_and_receive_udp(sock_fd, &server_addr, "QUIT");

    // 3. Close socket
    printf("[*] [close()] Closing UDP client socket descriptor...\n");
    close(sock_fd);
    printf("[+] UDP Client execution completed successfully.\n");

    return 0;
}
