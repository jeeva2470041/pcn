/*
 * File: udp_server_students.c
 * Assignment 2(b): UDP Server to Verify Student Record
 * Description: Receives UDP datagram with Roll Number, searches students.csv,
 *              and replies with Roll Number + Student Name (or Not Found).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 9090
#define BUFFER_SIZE 1024
#define CSV_FILE "students.csv"

// Search CSV database for UDP verification (Returns only Roll Number & Name)
int search_student_basic(const char *roll_no, char *result_buffer) {
    FILE *fp = fopen(CSV_FILE, "r");
    if (!fp) {
        snprintf(result_buffer, BUFFER_SIZE, "Server Error: Could not open database file.");
        return -1;
    }

    char line[256];
    char roll[32], name[64], dept[64], sem[16], cgpa[16];
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        
        char *token = strtok(line, ",");
        if (!token) continue;
        strncpy(roll, token, sizeof(roll) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(name, token, sizeof(name) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(dept, token, sizeof(dept) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(sem, token, sizeof(sem) - 1);

        token = strtok(NULL, ",");
        if (token) strncpy(cgpa, token, sizeof(cgpa) - 1);

        if (strcmp(roll, roll_no) == 0) {
            snprintf(result_buffer, BUFFER_SIZE,
                     "=== STUDENT RECORD VERIFIED ===\n"
                     "Roll Number  : %s\n"
                     "Student Name : %s\n"
                     "===============================",
                     roll, name);
            found = 1;
            break;
        }
    }

    fclose(fp);

    if (!found) {
        snprintf(result_buffer, BUFFER_SIZE, "Student Record Not Found");
    }
    return found;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    // 1. Create UDP datagram socket
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] UDP Student Verification Server Socket created.\n");

    // 2. Configure server address structure
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // 3. Bind socket to IP and Port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] UDP Server bound to port %d.\n", PORT);
    printf("[SERVER] Ready to receive verification queries on port %d...\n\n", PORT);

    // 4. Server datagram listening loop
    while (1) {
        client_len = sizeof(client_addr);
        bzero(buffer, BUFFER_SIZE);

        ssize_t bytes_recv = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0,
                                      (struct sockaddr *)&client_addr, &client_len);
        if (bytes_recv < 0) {
            perror("[-] recvfrom() failed");
            continue;
        }

        buffer[bytes_recv] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        printf("[<-] Received UDP query from %s:%d | Roll Number: \"%s\"\n",
               client_ip, client_port, buffer);

        // Search database
        bzero(response, BUFFER_SIZE);
        search_student_basic(buffer, response);

        // Send datagram response back to client
        sendto(server_fd, response, strlen(response), 0,
               (struct sockaddr *)&client_addr, client_len);
        printf("[->] Sent verification reply back to %s:%d\n\n", client_ip, client_port);
    }

    close(server_fd);
    return 0;
}
