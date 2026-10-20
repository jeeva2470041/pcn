/*
 * File: http_client.c
 * Assignment 3: TCP Socket-Based HTTP Client in C
 * Description: Accepts URL from user, parses host/port/path, connects via TCP socket,
 *              sends HTTP GET request, strips headers, saves downloaded content locally,
 *              and computes performance metrics: Response Time, Download Time, Size, Throughput.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 4096
#define DEFAULT_HTTP_PORT 80

// Structure to hold parsed URL components
typedef struct {
    char host[256];
    int port;
    char path[512];
    char filename[256];
} parsed_url_t;

// Function to parse URLs (e.g., "http://localhost:8000/sample.pdf")
int parse_url(const char *url, parsed_url_t *parsed) {
    char temp_url[1024];
    strncpy(temp_url, url, sizeof(temp_url) - 1);

    char *start = temp_url;
    if (strncmp(start, "http://", 7) == 0) {
        start += 7;
    }

    parsed->port = DEFAULT_HTTP_PORT;
    char *slash = strchr(start, '/');
    if (slash) {
        strncpy(parsed->path, slash, sizeof(parsed->path) - 1);
        *slash = '\0';
    } else {
        strcpy(parsed->path, "/");
    }

    char *colon = strchr(start, ':');
    if (colon) {
        *colon = '\0';
        parsed->port = atoi(colon + 1);
    }
    strncpy(parsed->host, start, sizeof(parsed->host) - 1);

    // Extract filename from path
    char *last_slash = strrchr(parsed->path, '/');
    if (last_slash && strlen(last_slash + 1) > 0) {
        snprintf(parsed->filename, sizeof(parsed->filename), "downloaded_%s", last_slash + 1);
    } else {
        strcpy(parsed->filename, "downloaded_index.html");
    }
    return 0;
}

// Function to get current time in milliseconds
double get_time_in_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

int main(int argc, char *argv[]) {
    char input_url[1024];
    parsed_url_t url_info;
    int sock_fd;
    struct sockaddr_in server_addr;
    struct hostent *host_entry;

    printf("====================================================\n");
    printf("     TCP SOCKET-BASED HTTP CLIENT APPLICATION       \n");
    printf("====================================================\n\n");

    // 1. Accept URL from keyboard or arguments
    if (argc > 1) {
        strncpy(input_url, argv[1], sizeof(input_url) - 1);
        printf("Using URL: %s\n", input_url);
    } else {
        printf("Enter URL: ");
        if (!fgets(input_url, sizeof(input_url), stdin)) {
            return 1;
        }
        input_url[strcspn(input_url, "\r\n")] = 0;
    }

    parse_url(input_url, &url_info);
    printf("\n[*] Parsed URL Details:\n");
    printf("    -> Hostname : %s\n", url_info.host);
    printf("    -> Port     : %d\n", url_info.port);
    printf("    -> Path     : %s\n", url_info.path);
    printf("    -> Save File: %s\n\n", url_info.filename);

    // 2. Resolve Hostname
    host_entry = gethostbyname(url_info.host);
    if (!host_entry) {
        fprintf(stderr, "[-] Error: Unable to resolve host \"%s\"\n", url_info.host);
        return 1;
    }

    // 3. Create TCP Socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[-] Error: socket() failed");
        return 1;
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(url_info.port);
    memcpy(&server_addr.sin_addr.s_addr, host_entry->h_addr_list[0], host_entry->h_length);

    // 4. Connect to Server (Measure TCP Connection Time)
    double t_start_connect = get_time_in_ms();
    printf("[*] Connecting to %s:%d...\n", inet_ntoa(server_addr.sin_addr), url_info.port);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error: connect() failed");
        close(sock_fd);
        return 1;
    }
    double t_connected = get_time_in_ms();
    double tcp_connect_time = t_connected - t_start_connect;
    printf("[+] Connected successfully! (TCP Connect Time: %.2f ms)\n", tcp_connect_time);

    // 5. Construct and Send HTTP GET Request
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: C-Socket-HTTP-Client/1.0\r\n"
             "Connection: close\r\n\r\n",
             url_info.path, url_info.host);

    double t_send_req = get_time_in_ms();
    send(sock_fd, request, strlen(request), 0);
    printf("\n[->] Sent HTTP Request:\n%s", request);

    // 6. Receive HTTP Response
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = 0;
    size_t total_payload_bytes = 0;
    int header_stripped = 0;
    double t_first_byte = 0;
    FILE *fp = NULL;

    while ((bytes_received = recv(sock_fd, buffer, sizeof(buffer), 0)) > 0) {
        if (!header_stripped) {
            t_first_byte = get_time_in_ms(); // TTFB
            
            // Locate the end of HTTP headers (\r\n\r\n)
            char *header_end = strstr(buffer, "\r\n\r\n");
            if (header_end) {
                *header_end = '\0';
                printf("[<-] Received HTTP Response Headers:\n");
                printf("----------------------------------------\n");
                printf("%s\n", buffer);
                printf("----------------------------------------\n\n");

                // Payload starts after \r\n\r\n (4 bytes)
                char *payload_start = header_end + 4;
                size_t header_len = (payload_start - buffer);
                size_t payload_len = bytes_received - header_len;

                fp = fopen(url_info.filename, "wb");
                if (!fp) {
                    perror("[-] Error: Cannot create output file");
                    break;
                }
                if (payload_len > 0) {
                    fwrite(payload_start, 1, payload_len, fp);
                    total_payload_bytes += payload_len;
                }
                header_stripped = 1;
            }
        } else {
            // Write binary payload directly to file
            if (fp) {
                fwrite(buffer, 1, bytes_received, fp);
                total_payload_bytes += bytes_received;
            }
        }
    }

    double t_download_end = get_time_in_ms();
    if (fp) fclose(fp);
    close(sock_fd);

    // 7. Calculate Performance Metrics
    double response_time = (t_first_byte > 0) ? (t_first_byte - t_send_req) : 0;
    double total_download_time = (t_download_end - t_send_req);
    double throughput_kbps = (total_download_time > 0) ? 
        ((total_payload_bytes / 1024.0) / (total_download_time / 1000.0)) : 0;

    printf("====================================================\n");
    printf("           PERFORMANCE ANALYSIS RESULTS             \n");
    printf("====================================================\n");
    printf("  Resource URL       : %s\n", input_url);
    printf("  Saved Destination  : %s\n", url_info.filename);
    printf("  Data Size          : %zu Bytes (%.2f KB)\n", total_payload_bytes, total_payload_bytes / 1024.0);
    printf("  TCP Connect Time   : %.2f ms\n", tcp_connect_time);
    printf("  Response Time(TTFB): %.2f ms\n", response_time);
    printf("  Total Download Time: %.2f ms\n", total_download_time);
    printf("  Throughput         : %.2f KB/s (%.3f MB/s)\n", throughput_kbps, throughput_kbps / 1024.0);
    printf("====================================================\n");

    return 0;
}
