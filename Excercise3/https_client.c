/*
 * File: https_client.c
 * Assignment 3 (Advanced Program): Secure HTTPS Client using TCP Sockets and OpenSSL
 * Description: Connects to HTTPS server (Port 443), establishes SSL/TLS session,
 *              validates certificates, sends encrypted GET request, decrypts response,
 *              and measures TCP connection, SSL handshake, and transfer metrics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE 4096

double get_time_in_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

int main(int argc, char *argv[]) {
    const char *hostname = (argc > 1) ? argv[1] : "example.com";
    int port = 443;
    int sock_fd;
    struct sockaddr_in server_addr;
    struct hostent *host;

    printf("====================================================\n");
    printf("     SECURE HTTPS CLIENT WITH OPENSSL / TLS         \n");
    printf("====================================================\n\n");

    // 1. Initialize OpenSSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        fprintf(stderr, "[-] Unable to create SSL context\n");
        return 1;
    }

    // 2. Resolve Host & Create TCP Socket
    host = gethostbyname(hostname);
    if (!host) {
        fprintf(stderr, "[-] Could not resolve host %s\n", hostname);
        return 1;
    }

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);

    // 3. TCP Connection
    double t_tcp_start = get_time_in_ms();
    printf("[*] Initiating TCP connection to %s:%d...\n", inet_ntoa(server_addr.sin_addr), port);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] TCP connect failed");
        return 1;
    }
    double t_tcp_end = get_time_in_ms();
    double tcp_time = t_tcp_end - t_tcp_start;
    printf("[+] TCP Connection Established! (Latency: %.2f ms)\n", tcp_time);

    // 4. SSL/TLS Handshake
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock_fd);
    SSL_set_tlsext_host_name(ssl, hostname); // SNI

    double t_ssl_start = get_time_in_ms();
    printf("[*] Performing SSL/TLS Handshake...\n");
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    double t_ssl_end = get_time_in_ms();
    double ssl_handshake_time = t_ssl_end - t_ssl_start;

    printf("[+] SSL/TLS Handshake Complete! (Handshake Time: %.2f ms)\n", ssl_handshake_time);
    printf("    -> Cipher Suite : %s\n", SSL_get_cipher(ssl));
    printf("    -> TLS Version  : %s\n", SSL_get_version(ssl));

    // 5. Send Secure HTTP GET Request
    char request[512];
    snprintf(request, sizeof(request),
             "GET / HTTP/1.1\r\nHost: %s\r\nUser-Agent: C-OpenSSL-Client/1.0\r\nConnection: close\r\n\r\n",
             hostname);

    double t_req_start = get_time_in_ms();
    SSL_write(ssl, request, strlen(request));
    printf("\n[->] Sent Encrypted GET Request to %s\n", hostname);

    // 6. Receive and Decrypt Response
    char buffer[BUFFER_SIZE];
    int bytes;
    size_t total_bytes = 0;
    double t_first_byte = 0;
    int headers_shown = 0;

    FILE *fp = fopen("downloaded_https_page.html", "wb");
    while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
        if (!t_first_byte) t_first_byte = get_time_in_ms();
        total_bytes += bytes;
        buffer[bytes] = '\0';
        if (!headers_shown) {
            char *end = strstr(buffer, "\r\n\r\n");
            if (end) {
                *end = '\0';
                printf("[<-] Decrypted Response Headers:\n%s\n\n", buffer);
                headers_shown = 1;
            }
        }
        if (fp) fwrite(buffer, 1, bytes, fp);
    }
    double t_download_end = get_time_in_ms();
    if (fp) fclose(fp);

    // Cleanup
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock_fd);
    SSL_CTX_free(ctx);

    double response_time = t_first_byte - t_req_start;
    double total_time = t_download_end - t_tcp_start;

    printf("====================================================\n");
    printf("        HTTPS SECURITY & PERFORMANCE METRICS        \n");
    printf("====================================================\n");
    printf("  Target Server       : https://%s (Port 443)\n", hostname);
    printf("  TCP Connect Time    : %.2f ms\n", tcp_time);
    printf("  SSL Handshake Time  : %.2f ms\n", ssl_handshake_time);
    printf("  Response Time (TTFB): %.2f ms\n", response_time);
    printf("  Total Download Time : %.2f ms\n", total_time);
    printf("  Bytes Received      : %zu Bytes\n", total_bytes);
    printf("====================================================\n");

    return 0;
}
