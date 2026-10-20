/*
 * File: server_udp.c
 * Description: UDP Connectionless Server demonstrating socket system calls:
 *              socket(), bind(), recvfrom(), sendto(), htons(), close().
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
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
    char response[BUFFER_SIZE];
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    // 1. Create UDP Socket using socket(AF_INET, SOCK_DGRAM, 0)
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Creating datagram socket with socket(AF_INET, SOCK_DGRAM, 0)... [OK fd=%d]\n", server_fd);

    // 2. Configure server address
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // 3. Bind socket to address and port using bind()
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Binding UDP socket to 0.0.0.0:%d with bind()... [OK]\n", port);
    printf("[SERVER] UDP Server is ready and listening for datagrams on port %d...\n\n", port);

    int packet_count = 1;
    while (1) {
        client_len = sizeof(client_addr);
        bzero(buffer, BUFFER_SIZE);

        // 4. Receive datagram from client using recvfrom()
        ssize_t bytes_recv = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0,
                                      (struct sockaddr *)&client_addr, &client_len);
        if (bytes_recv < 0) {
            perror("[-] recvfrom() failed");
            continue;
        }

        buffer[bytes_recv] = '\0';
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        printf("=== DATAGRAM %d RECEIVED ===\n", packet_count++);
        printf("[<-] [recvfrom()] Received %zd bytes from [%s:%d]\n", bytes_recv, client_ip, client_port);
        printf("     Message Payload: \"%s\"\n", buffer);

        if (strcmp(buffer, "QUIT") == 0) {
            printf("[*] [close()] Server received stop signal. Closing socket fd %d.\n", server_fd);
            break;
        }

        if (strcmp(buffer, "QUERY: GET_SYSTEM_TIME") == 0) {
            snprintf(response, sizeof(response), "TIME: 2026-06-25 10:30:15 IST");
        } else {
            snprintf(response, sizeof(response), "ACK: %s", buffer);
        }

        // 5. Send reply datagram using sendto()
        ssize_t bytes_sent = sendto(server_fd, response, strlen(response), 0,
                                    (struct sockaddr *)&client_addr, client_len);
        printf("[->] [sendto()] Transmitted ACK datagram (%zd bytes) back to %s:%d\n\n",
               bytes_sent, client_ip, client_port);
    }

    close(server_fd);
    return 0;
}
