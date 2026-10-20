/*
 * File: http_traffic_generator.c
 * Assignment 4: HTTP Packet Traffic Generator for Wireshark Capture
 * Description: Generates standard HTTP GET requests and conditional GET requests
 *              (using If-Modified-Since / If-None-Match headers) to test HTTP 200 OK
 *              and HTTP 304 Not Modified responses, matching the Wireshark lab questions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 4096

// Function to send HTTP request and print status code and headers
void send_http_get(const char *host, int port, const char *path, const char *if_modified_since) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    struct hostent *he = gethostbyname(host);
    if (!he) {
        herror("Hostname lookup failed");
        close(sock);
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);

    printf("\nConnecting to %s (%s) on port %d...\n",
           host, inet_ntoa(server_addr.sin_addr), port);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sock);
        return;
    }

    // Construct HTTP request buffer
    char request[BUFFER_SIZE];
    if (if_modified_since && strlen(if_modified_since) > 0) {
        snprintf(request, sizeof(request),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: C-Wireshark-Traffic-Generator/1.0\r\n"
                 "Accept: text/html,application/xhtml+xml\r\n"
                 "Accept-Language: en-US,en;q=0.9\r\n"
                 "If-Modified-Since: %s\r\n"
                 "Connection: close\r\n\r\n",
                 path, host, if_modified_since);
    } else {
        snprintf(request, sizeof(request),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: C-Wireshark-Traffic-Generator/1.0\r\n"
                 "Accept: text/html,application/xhtml+xml\r\n"
                 "Accept-Language: en-US,en;q=0.9\r\n"
                 "Connection: close\r\n\r\n",
                 path, host);
    }

    printf("--- Transmitting HTTP Request ---\n%s", request);
    send(sock, request, strlen(request), 0);

    // Read response
    char response[BUFFER_SIZE];
    ssize_t bytes = recv(sock, response, sizeof(response) - 1, 0);
    if (bytes > 0) {
        response[bytes] = '\0';
        printf("--- Received Server Response Header ---\n");
        char *header_end = strstr(response, "\r\n\r\n");
        if (header_end) {
            *header_end = '\0';
            printf("%s\n", response);
        } else {
            printf("%s\n", response);
        }
    }

    close(sock);
}

int main(int argc, char *argv[]) {
    const char *host = "gaia.cs.umass.edu";
    int port = 80;
    const char *path = "/wireshark-labs/HTTP-wireshark-file1.html";

    printf("===================================================================\n");
    printf("  Assignment 4: C HTTP Packet Traffic Generator for Wireshark\n");
    printf("===================================================================\n");

    // Phase 1: Basic HTTP GET (Expect 200 OK)
    printf("\n[Phase 1] Sending Basic HTTP GET Request...\n");
    send_http_get(host, port, path, NULL);

    // Phase 2: Conditional GET with If-Modified-Since header (Expect 304 Not Modified)
    printf("\n[Phase 2] Sending Conditional GET with If-Modified-Since...\n");
    send_http_get(host, port, path, "Sat, 20 Oct 2026 12:00:00 GMT");

    printf("\nTraffic generation completed. Check Wireshark / sniffer output.\n");
    return 0;
}
