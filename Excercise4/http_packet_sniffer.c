/*
 * File: http_packet_sniffer.c
 * Assignment 4: Network Packet Sniffer & HTTP Protocol Analyzer in C
 * Description: Captures and analyzes network packets traversing network interfaces.
 *              Decodes Layer 3 (IP) and Layer 4 (TCP) headers, extracts HTTP GET requests,
 *              HTTP responses, status codes, and headers (such as Host, User-Agent,
 *              If-Modified-Since), mirroring Wireshark packet inspection in C.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_SIZE 65536

// Function to print formatted hexadecimal and ASCII packet dump (like Wireshark hex pane)
void print_hex_dump(const unsigned char *data, int length) {
    printf("\n  --- Packet Payload Hex/ASCII Dump (%d bytes) ---\n", length);
    for (int i = 0; i < length; i += 16) {
        printf("  %04x: ", i);
        for (int j = 0; j < 16; j++) {
            if (i + j < length)
                printf("%02x ", data[i + j]);
            else
                printf("   ");
            if (j == 7) printf(" ");
        }
        printf(" |");
        for (int j = 0; j < 16; j++) {
            if (i + j < length) {
                unsigned char c = data[i + j];
                printf("%c", (c >= 32 && c <= 126) ? c : '.');
            }
        }
        printf("|\n");
    }
}

// Function to parse and inspect HTTP protocol content
void inspect_http_payload(const unsigned char *payload, int payload_len) {
    if (payload_len <= 0) return;

    // Check if payload starts with standard HTTP methods or HTTP response signature
    if (strncmp((const char *)payload, "GET ", 4) == 0 ||
        strncmp((const char *)payload, "POST ", 5) == 0 ||
        strncmp((const char *)payload, "HTTP/", 5) == 0) {
        
        printf("  [>>> HTTP PROTOCOL DETECTED <<<]\n");
        char copy[BUFFER_SIZE];
        int copy_len = payload_len < (BUFFER_SIZE - 1) ? payload_len : (BUFFER_SIZE - 1);
        memcpy(copy, payload, copy_len);
        copy[copy_len] = '\0';

        // Print first 5 header lines
        char *line = strtok(copy, "\r\n");
        int count = 0;
        while (line && count < 6) {
            printf("    | %s\n", line);
            line = strtok(NULL, "\r\n");
            count++;
        }
    }
}

// Function to process a captured IP packet
void process_packet(unsigned char *buffer, int size, int packet_num) {
    struct iphdr *iph = (struct iphdr *)buffer;
    unsigned short iphdrlen = iph->ihl * 4;

    if (iph->protocol != IPPROTO_TCP) {
        return; // Only inspect TCP / HTTP packets
    }

    struct tcphdr *tcph = (struct tcphdr *)(buffer + iphdrlen);
    int tcphdrlen = tcph->doff * 4;
    int header_size = iphdrlen + tcphdrlen;
    int payload_size = size - header_size;
    unsigned char *payload = buffer + header_size;

    struct in_addr src_ip, dst_ip;
    src_ip.s_addr = iph->saddr;
    dst_ip.s_addr = iph->daddr;

    printf("\n======================================================================\n");
    printf(" Packet #%d | Length: %d bytes\n", packet_num, size);
    printf("----------------------------------------------------------------------\n");
    printf(" [IP Header]   Version: IPv%d | Header Len: %d bytes | TTL: %d\n",
           iph->version, iphdrlen, iph->ttl);
    printf("               Source IP: %s -> Dest IP: %s\n",
           inet_ntoa(src_ip), inet_ntoa(dst_ip));
    printf(" [TCP Header]  Source Port: %u | Dest Port: %u\n",
           ntohs(tcph->source), ntohs(tcph->dest));
    printf("               Seq: %u | Ack: %u | Flags: [%s%s%s%s]\n",
           ntohl(tcph->seq), ntohl(tcph->ack_seq),
           tcph->syn ? "SYN " : "",
           tcph->ack ? "ACK " : "",
           tcph->fin ? "FIN " : "",
           tcph->rst ? "RST " : "");

    // Inspect HTTP if ports are standard web ports (80, 8000, 8080)
    int src_port = ntohs(tcph->source);
    int dst_port = ntohs(tcph->dest);
    if (src_port == 80 || src_port == 8000 || src_port == 8080 ||
        dst_port == 80 || dst_port == 8000 || dst_port == 8080) {
        inspect_http_payload(payload, payload_size);
    }

    if (payload_size > 0 && payload_size <= 256) {
        print_hex_dump(payload, payload_size);
    }
}

int main(int argc, char *argv[]) {
    printf("===============================================================\n");
    printf("  Assignment 4: C Network Packet Sniffer & HTTP Analyzer\n");
    printf("===============================================================\n");

    int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (raw_sock < 0) {
        printf("Note: Raw socket creation requires administrative/root privileges.\n");
        printf("Switching to HTTP Traffic Inspection Simulation mode...\n\n");

        // Demonstration of HTTP packet dissection on sample Wireshark HTTP packets
        const char *sample_http_req = 
            "GET /wireshark-labs/HTTP-wireshark-file1.html HTTP/1.1\r\n"
            "Host: gaia.cs.umass.edu\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
            "Accept: text/html,application/xhtml+xml\r\n"
            "Accept-Language: en-US,en;q=0.9\r\n"
            "Connection: keep-alive\r\n\r\n";

        const char *sample_http_resp = 
            "HTTP/1.1 200 OK\r\n"
            "Date: Mon, 17 Aug 2026 12:00:00 GMT\r\n"
            "Server: Apache/2.4.6 (CentOS)\r\n"
            "Last-Modified: Mon, 17 Aug 2026 11:45:00 GMT\r\n"
            "ETag: \"1e-5a02c9\"\r\n"
            "Content-Length: 128\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<html>\nCongratulations! You have downloaded the Wireshark lab file.\n</html>\n";

        printf("--- Simulating Captured HTTP GET Request Frame ---\n");
        inspect_http_payload((const unsigned char *)sample_http_req, strlen(sample_http_req));
        print_hex_dump((const unsigned char *)sample_http_req, strlen(sample_http_req));

        printf("\n--- Simulating Captured HTTP 200 OK Response Frame ---\n");
        inspect_http_payload((const unsigned char *)sample_http_resp, strlen(sample_http_resp));
        print_hex_dump((const unsigned char *)sample_http_resp, strlen(sample_http_resp));
        return 0;
    }

    printf("Listening for TCP & HTTP packets on network interface...\n");
    unsigned char buffer[BUFFER_SIZE];
    int packet_count = 0;

    while (packet_count < 20) {
        int data_size = recvfrom(raw_sock, buffer, sizeof(buffer), 0, NULL, NULL);
        if (data_size < 0) {
            perror("Recvfrom error");
            break;
        }
        packet_count++;
        process_packet(buffer, data_size, packet_count);
    }

    close(raw_sock);
    return 0;
}
