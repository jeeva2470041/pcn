/*
 * File: tcp_server_students.c
 * Assignment 2(a): Multi-Client Concurrent TCP Server using POSIX Threads (pthread)
 * Description: Reads student records from students.csv and handles multiple client
 *              requests concurrently to retrieve complete academic profiles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define CSV_FILE "students.csv"

// Structure to pass client connection details to thread worker
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_context_t;

// Function to search students.csv for complete profile
int search_student_record(const char *roll_no, char *result_buffer) {
    FILE *fp = fopen(CSV_FILE, "r");
    if (!fp) {
        snprintf(result_buffer, BUFFER_SIZE, "Server Error: Could not open database file.");
        return -1;
    }

    char line[256];
    char roll[32], name[64], dept[64], sem[16], cgpa[16];
    int found = 0;

    // Read CSV line by line
    while (fgets(line, sizeof(line), fp)) {
        // Strip newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Parse CSV fields: Roll Number, Name, Department, Semester, CGPA
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

        // Check if Roll Number matches (ignoring leading/trailing spaces)
        if (strcmp(roll, roll_no) == 0) {
            snprintf(result_buffer, BUFFER_SIZE,
                     "=== STUDENT ACADEMIC PROFILE ===\n"
                     "Roll Number : %s\n"
                     "Name        : %s\n"
                     "Department  : %s\n"
                     "Semester    : %s\n"
                     "CGPA        : %s\n"
                     "================================",
                     roll, name, dept, sem, cgpa);
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

// Thread handler function for each concurrent client
void *client_handler(void *arg) {
    client_context_t *ctx = (client_context_t *)arg;
    int client_sock = ctx->client_socket;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ctx->client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(ctx->client_addr.sin_port);
    pthread_t tid = pthread_self();

    printf("[THREAD %lu] Handling client %s:%d (Socket fd: %d)\n",
           (unsigned long)tid, client_ip, client_port, client_sock);

    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    // Read Roll Number from client
    bzero(buffer, BUFFER_SIZE);
    ssize_t bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0; // Strip newlines

        printf("[THREAD %lu] Received Roll Number query: \"%s\" from %s:%d\n",
               (unsigned long)tid, buffer, client_ip, client_port);

        // Search CSV database
        bzero(response, BUFFER_SIZE);
        search_student_record(buffer, response);

        // Send academic profile / error message back to client
        send(client_sock, response, strlen(response), 0);
        printf("[THREAD %lu] Sent profile response back to client.\n", (unsigned long)tid);
    }

    // Gracefully close client socket and free context
    close(client_sock);
    printf("[THREAD %lu] Client %s:%d disconnected. Closed socket.\n",
           (unsigned long)tid, client_ip, client_port);
    free(ctx);
    pthread_exit(NULL);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int opt = 1;

    // 1. Create TCP Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] TCP Multi-Threaded Server Socket created successfully.\n");

    // 2. Set socket options for port reuse
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. Configure server address structure
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // 4. Bind socket to IP and Port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SYSTEM] Server bound to port %d.\n", PORT);

    // 5. Listen for incoming client connections
    if (listen(server_fd, 10) < 0) {
        perror("[-] listen() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] Multi-threaded TCP Server is listening on port %d...\n", PORT);
    printf("[SERVER] Loaded database: %s\n\n", CSV_FILE);

    // 6. Main Accept loop - spawn POSIX thread per client
    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("[-] accept() failed");
            continue;
        }

        // Allocate context for the new thread
        client_context_t *ctx = (client_context_t *)malloc(sizeof(client_context_t));
        ctx->client_socket = client_fd;
        ctx->client_addr = client_addr;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, (void *)ctx) != 0) {
            perror("[-] pthread_create() failed");
            free(ctx);
            close(client_fd);
        } else {
            // Detach thread to allow kernel cleanup upon termination
            pthread_detach(thread_id);
        }
    }

    close(server_fd);
    return 0;
}
