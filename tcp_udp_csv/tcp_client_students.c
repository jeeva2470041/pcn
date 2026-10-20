/*
 * File: tcp_client_students.c
 * Assignment 2(a): TCP Client to Retrieve Complete Student Academic Profile
 * Description: Connects to server, prompts user for Roll Number, sends query,
 *              and displays full profile received from the server.
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
#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    char roll_number[64];
    char response[BUFFER_SIZE];
    const char *target_ip = (argc > 1) ? argv[1] : SERVER_IP;

    // 1. Create client socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] TCP Client socket created successfully.\n");

    // 2. Configure server address
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, target_ip, &server_addr.sin_addr);

    // 3. Connect to server
    printf("[*] Connecting to Academic Records Server at %s:%d...\n", target_ip, PORT);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] connect() failed. Is the server running?");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[+] Connected to Academic Records Server!\n\n");

    // 4. Prompt user to enter Roll Number
    if (argc > 2) {
        strncpy(roll_number, argv[2], sizeof(roll_number) - 1);
        printf("Using provided Roll Number: %s\n", roll_number);
    } else {
        printf("Enter Student Roll Number to Query: ");
        if (!fgets(roll_number, sizeof(roll_number), stdin)) {
            close(sock_fd);
            return 0;
        }
        roll_number[strcspn(roll_number, "\r\n")] = 0; // Strip newline
    }

    // 5. Send Roll Number to server
    printf("[->] Sending query for Roll Number: \"%s\"...\n", roll_number);
    if (send(sock_fd, roll_number, strlen(roll_number), 0) < 0) {
        perror("[-] send() failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // 6. Receive and display server response
    bzero(response, BUFFER_SIZE);
    ssize_t bytes_received = recv(sock_fd, response, sizeof(response) - 1, 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("\n[<-] Server Response:\n%s\n\n", response);
    } else {
        printf("[-] No response received from server.\n");
    }

    // 7. Close socket connection
    close(sock_fd);
    printf("[*] Connection closed gracefully.\n");
    return 0;
}
