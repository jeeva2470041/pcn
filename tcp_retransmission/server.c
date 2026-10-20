/*
 * File: server.c
 * Assignment: TCP 4-Packet Transmission with Intentional Packet Loss, Retransmission & Timers
 * Description: Server listens for TCP connection, receives 4 packets from client.
 *              Supports two simulation modes:
 *                Mode 1 (NACK): Intentionally replies that Packet #3 is MISSED (Explicit NACK)
 *                Mode 2 (TIMEOUT): Intentionally drops Packet #3 and withholds ACK,
 *                                  causing the Client's Retransmission Timer to expire.
 *              Receives retransmitted packet, confirms delivery, and displays metrics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define DEFAULT_PORT 8888
#define TOTAL_PACKETS 4
#define INTENTIONAL_DROP_SEQ 3

typedef struct {
    int seq_num;            // Sequence number (1 to 4)
    int is_retransmission;  // 0 = initial send, 1 = retransmission
    int data_len;           // Length of payload
    char data[256];         // Packet payload
} Packet;

typedef struct {
    int ack_seq;            // Sequence number acknowledged
    int status;             // 1 = ACK (Success), 0 = NACK (Missed / Drop)
    char message[256];      // Status message
} AckPacket;

// Helper to get time in milliseconds
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

// Ensure exact number of bytes are received over TCP stream
int recv_all(int sock_fd, void *buf, size_t len) {
    size_t total = 0;
    char *p = (char *)buf;
    while (total < len) {
        ssize_t n = recv(sock_fd, p + total, len - total, 0);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}

// Ensure exact number of bytes are sent over TCP stream
int send_all(int sock_fd, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = (const char *)buf;
    while (total < len) {
        ssize_t n = send(sock_fd, p + total, len - total, 0);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    int drop_seq = (argc > 2) ? atoi(argv[2]) : INTENTIONAL_DROP_SEQ;
    // mode 1: Explicit NACK ("Packet is missed"), mode 2: Drop ACK (trigger client timer timeout)
    int sim_mode = (argc > 3) ? atoi(argv[3]) : 1; 
    int opt = 1;

    printf("=================================================================\n");
    printf("   TCP SERVER: 4-PACKET RECEIVER WITH TIMERS & LOSS SIMULATION  \n");
    printf("=================================================================\n\n");

    // 1. Create TCP Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind Address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Listen
    if (listen(server_fd, 5) < 0) {
        perror("[-] listen() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[*] Server listening on port %d...\n", port);
    printf("[*] Target Packet to Simulate Loss : Packet #%d\n", drop_seq);
    printf("[*] Simulation Mode                : %s\n", 
           (sim_mode == 1) ? "Mode 1 (Send 'Packet Missed' NACK)" : "Mode 2 (Silent Drop to trigger Client Timer Timeout)");
    printf("[*] Waiting for client to connect...\n\n");

    // 4. Accept Connection
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("[-] accept() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[+] Client connected from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    printf("-----------------------------------------------------------------\n\n");

    double t_start = get_time_ms();
    int received_count = 0;
    int retransmission_count = 0;
    int intentional_drops = 0;
    size_t total_payload_bytes = 0;
    char reassembled_data[TOTAL_PACKETS + 1][256];
    int packet_received_flag[TOTAL_PACKETS + 1] = {0};

    // 5. Packet Processing Loop
    while (received_count < TOTAL_PACKETS) {
        Packet pkt;
        int n = recv_all(client_fd, &pkt, sizeof(Packet));
        if (n <= 0) {
            printf("[-] Client disconnected or stream closed.\n");
            break;
        }

        total_payload_bytes += pkt.data_len;
        AckPacket ack;
        memset(&ack, 0, sizeof(AckPacket));
        ack.ack_seq = pkt.seq_num;

        printf("[RECEIVE] Incoming Packet:\n");
        printf("          Sequence No    : #%d\n", pkt.seq_num);
        printf("          Data Content   : \"%s\"\n", pkt.data);
        printf("          Retransmission : %s\n", pkt.is_retransmission ? "YES (Retried)" : "NO (First Try)");

        // Check for intentional loss on target packet
        if (pkt.seq_num == drop_seq && pkt.is_retransmission == 0) {
            intentional_drops++;

            if (sim_mode == 1) {
                // Mode 1: Intentionally tell the client that the packet was missed
                printf("  [!] INTENTIONAL SIMULATION: Server intentionally notifies that Packet #%d is MISSED!\n", pkt.seq_num);
                ack.status = 0; // NACK / MISSED
                snprintf(ack.message, sizeof(ack.message),
                         "NACK: Packet #%d was MISSED / DROPPED by server! Please retransmit.", pkt.seq_num);

                send_all(client_fd, &ack, sizeof(AckPacket));
                printf("  [->] Sent NACK to Client for Packet #%d\n\n", pkt.seq_num);
            } else {
                // Mode 2: Silently drop the packet & withhold ACK (forcing Client's Timer to time out)
                printf("  [!] INTENTIONAL SIMULATION: Server withholds ACK for Packet #%d!\n", pkt.seq_num);
                printf("  [*] Waiting for Client's Retransmission Timer (Timeout) to expire...\n\n");
                // Do NOT send ACK -> Client socket timeout timer will expire
            }
        } else {
            // Packet accepted (first try or recovered via retransmission)
            if (pkt.is_retransmission) {
                retransmission_count++;
                printf("  [+] RETRANSMISSION ACCEPTED: Packet #%d successfully recovered!\n", pkt.seq_num);
            } else {
                printf("  [+] Packet #%d accepted on first try.\n", pkt.seq_num);
            }

            if (!packet_received_flag[pkt.seq_num]) {
                packet_received_flag[pkt.seq_num] = 1;
                strncpy(reassembled_data[pkt.seq_num], pkt.data, sizeof(reassembled_data[pkt.seq_num]) - 1);
                received_count++;
            }

            ack.status = 1; // ACK
            snprintf(ack.message, sizeof(ack.message),
                     "ACK: Packet #%d received & verified successfully by server.", pkt.seq_num);

            send_all(client_fd, &ack, sizeof(AckPacket));
            printf("  [->] Sent ACK to Client for Packet #%d\n\n", pkt.seq_num);
        }
    }

    double t_end = get_time_ms();
    double total_time_ms = t_end - t_start;

    // 6. Print Server Performance & Reassembly Report
    printf("=================================================================\n");
    printf("                  SERVER PERFORMANCE SUMMARY                     \n");
    printf("=================================================================\n");
    printf("  Expected Packets          : %d\n", TOTAL_PACKETS);
    printf("  Packets Successfully Recv : %d\n", received_count);
    printf("  Intentional Loss Triggered: %d (Packet #%d)\n", intentional_drops, drop_seq);
    printf("  Retransmissions Handled   : %d\n", retransmission_count);
    printf("  Total Transmission Time   : %.2f ms\n", total_time_ms);
    printf("  Total Payload Received    : %zu bytes\n", total_payload_bytes);
    printf("  Server Throughput         : %.2f KB/s\n", 
           (total_time_ms > 0) ? ((total_payload_bytes / 1024.0) / (total_time_ms / 1000.0)) : 0);
    printf("-----------------------------------------------------------------\n");
    printf("  FINAL REASSEMBLED DATA STREAM (IN ORDER):\n");
    for (int i = 1; i <= TOTAL_PACKETS; i++) {
        printf("    [Packet #%d] -> \"%s\"\n", i, reassembled_data[i]);
    }
    printf("=================================================================\n\n");

    close(client_fd);
    close(server_fd);
    printf("[*] Server finished and sockets closed.\n");
    return 0;
}
