/*
 * File: http_server.c
 * Assignment 3: TCP Socket-Based HTTP Web Server in C
 * Description: Implements a concurrent/iterative HTTP 1.1 Web Server using TCP sockets.
 *              Listens for incoming client connections, parses HTTP GET requests,
 *              serves static resources (HTML, text, images, PDF) with appropriate HTTP headers,
 *              handles 404 Not Found errors, and logs requests with client IP and timestamps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8000
#define BUFFER_SIZE 8192
#define WEB_ROOT "."

// Helper function to determine Content-Type header based on file extension
const char *get_mime_type(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return "application/octet-stream";
    if (strcasecmp(dot, ".html") == 0 || strcasecmp(dot, ".htm") == 0) return "text/html";
    if (strcasecmp(dot, ".txt") == 0) return "text/plain";
    if (strcasecmp(dot, ".pdf") == 0) return "application/pdf";
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".png") == 0) return "image/png";
    if (strcasecmp(dot, ".gif") == 0) return "image/gif";
    if (strcasecmp(dot, ".css") == 0) return "text/css";
    if (strcasecmp(dot, ".js") == 0) return "application/javascript";
    return "application/octet-stream";
}

// Helper to send HTTP 404 Not Found
void send_404(int client_fd, const char *path) {
    char body[1024];
    snprintf(body, sizeof(body),
             "<!DOCTYPE html><html><head><title>404 Not Found</title></head>"
             "<body><h1>404 Not Found</h1><p>The requested URL %s was not found on this server.</p></body></html>\r\n",
             path);

    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n",
             strlen(body));

    send(client_fd, header, strlen(header), 0);
    send(client_fd, body, strlen(body), 0);
}

// Function to handle a single HTTP client request
void handle_http_client(int client_fd, struct sockaddr_in *client_addr) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }

    // Extract HTTP method and requested path
    char method[16], path[512], protocol[16];
    if (sscanf(buffer, "%15s %511s %15s", method, path, protocol) < 3) {
        close(client_fd);
        return;
    }

    // Timestamp for request logging
    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    printf("[%s] %s:%d -> %s %s %s\n",
           time_str,
           inet_ntoa(client_addr->sin_addr),
           ntohs(client_addr->sin_port),
           method, path, protocol);

    // Only handle GET requests
    if (strcasecmp(method, "GET") != 0) {
        const char *resp_501 = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(client_fd, resp_501, strlen(resp_501), 0);
        close(client_fd);
        return;
    }

    // Map URL path to local file system
    char filepath[1024];
    if (strcmp(path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", WEB_ROOT);
    } else {
        // Prevent directory traversal
        if (strstr(path, "..")) {
            send_404(client_fd, path);
            close(client_fd);
            return;
        }
        snprintf(filepath, sizeof(filepath), "%s%s", WEB_ROOT, path);
    }

    // Check if file exists and get size
    struct stat file_stat;
    if (stat(filepath, &file_stat) < 0 || S_ISDIR(file_stat.st_mode)) {
        printf("  [Response: 404 Not Found - %s]\n", filepath);
        send_404(client_fd, path);
        close(client_fd);
        return;
    }

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        send_404(client_fd, path);
        close(client_fd);
        return;
    }

    // Construct 200 OK response header
    const char *mime_type = get_mime_type(filepath);
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Server: Custom-C-HTTP-Server/1.0\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %ld\r\n"
                              "Connection: close\r\n\r\n",
                              mime_type,
                              (long)file_stat.st_size);

    send(client_fd, header, header_len, 0);

    // Transmit file contents
    char file_buffer[BUFFER_SIZE];
    size_t n;
    size_t total_sent = 0;
    while ((n = fread(file_buffer, 1, sizeof(file_buffer), fp)) > 0) {
        send(client_fd, file_buffer, n, 0);
        total_sent += n;
    }

    fclose(fp);
    printf("  [Response: 200 OK, Type: %s, Sent: %zu bytes]\n", mime_type, total_sent);

    close(client_fd);
}

int main(int argc, char *argv[]) {
    int server_port = PORT;
    if (argc > 1) {
        server_port = atoi(argv[1]);
    }

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 1. Create TCP Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEADDR option
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 2. Bind Socket to IP and Port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Listen for Incoming Connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("==================================================\n");
    printf("    Custom C HTTP Web Server Running on Port %d\n", server_port);
    printf("    Base Directory : %s\n", WEB_ROOT);
    printf("    Listening at   : http://localhost:%d/\n", server_port);
    printf("==================================================\n\n");

    // 4. Accept and handle requests loop
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        handle_http_client(client_fd, &client_addr);
    }

    close(server_fd);
    return 0;
}
