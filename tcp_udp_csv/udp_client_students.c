/*
 * File: udp_client_students.c
 * Assignment 2(b): UDP Client to Verify Student Record
 * Description: Sends a Roll Number to the server via UDP datagram and displays
 *              the received verification response (Roll Number + Name, or Not Found).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9090
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    char roll_number[64];
    char response[BUFFER_SIZE];
    socklen_t addr_len = sizeof(server_addr);
    const char *target_ip = (argc > 1) ? argv[1] : SERVER_IP;

    // 1. Create UDP Socket
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] UDP Client socket created successfully.\n");

    // 2. Configure server address
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, target_ip, &server_addr.sin_addr);

    // 3. Prompt user for Roll Number
    if (argc > 2) {
        strncpy(roll_number, argv[2], sizeof(roll_number) - 1);
        printf("Using provided Roll Number: %s\n", roll_number);
    } else {
        printf("Enter Student Roll Number to Verify: ");
        if (!fgets(roll_number, sizeof(roll_number), stdin)) {
            close(sock_fd);
            return 0;
        }
        roll_number[strcspn(roll_number, "\r\n")] = 0;
    }

    // 4. Send datagram to server
    printf("[->] Sending UDP datagram to %s:%d (Roll Number: \"%s\")...\n",
           target_ip, PORT, roll_number);
    if (sendto(sock_fd, roll_number, strlen(roll_number), 0,
               (struct sockaddr *)&server_addr, addr_len) < 0) {
        perror("[-] sendto() failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // 5. Receive response from server
    bzero(response, BUFFER_SIZE);
    ssize_t bytes_recv = recvfrom(sock_fd, response, sizeof(response) - 1, 0,
                                  (struct sockaddr *)&server_addr, &addr_len);
    if (bytes_recv > 0) {
        response[bytes_recv] = '\0';
        printf("\n[<-] Server Response:\n%s\n\n", response);
    } else {
        printf("[-] No response received from UDP server.\n");
    }

    // 6. Close socket
    close(sock_fd);
    printf("[*] UDP Client execution completed.\n");
    return 0;
}
