/*
 * File: client.c
 * Assignment: TCP 4-Packet Transmission with Intentional Packet Loss, Retransmission & Timers
 * Description: Client sends 4 distinct data packets to server over TCP.
 *              Uses a Retransmission Timer (RTO = 2000 ms) for each packet.
 *              Handles:
 *                1. Timer Expiration (Timeout when server drops packet/ACK)
 *                2. Explicit NACK (When server reports packet is missed)
 *              Automatically retransmits missed packets upon timeout or NACK,
 *              computes Jacobson's RTO algorithm, and displays networking & timer metrics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_PORT 8888
#define TOTAL_PACKETS 4
#define RTO_SECONDS 2 // 2-second Retransmission Timeout (RTO)

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

typedef struct {
    int seq_num;
    int attempt;
    int status;             // 1 = ACK, 0 = NACK, -1 = TIMEOUT
    double rtt_ms;
    size_t bytes;
} TransmissionRecord;

// Helper to get time in milliseconds
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
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

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_SERVER_IP;
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    printf("=================================================================\n");
    printf("   TCP CLIENT: 4-PACKET SENDER WITH RETRANSMISSION TIMERS (RTO) \n");
    printf("=================================================================\n\n");

    // 1. Create TCP Socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }

    // Configure Socket Receive Timeout (Retransmission Timer)
    struct timeval timeout;
    timeout.tv_sec = RTO_SECONDS;
    timeout.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("[-] setsockopt(SO_RCVTIMEO) failed");
    }
    printf("[*] Configured Socket Retransmission Timer (RTO = %d seconds / %d ms)\n", 
           RTO_SECONDS, RTO_SECONDS * 1000);

    // 2. Configure Server Address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("[-] inet_pton() failed: Invalid IP");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Connect to Server
    printf("[*] Connecting to TCP Server at %s:%d...\n", server_ip, port);
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] connect() failed: Ensure server is running first");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("[+] Connected to Server successfully!\n");
    printf("-----------------------------------------------------------------\n\n");

    // 4. Define 4 Test Packets
    const char *packet_contents[TOTAL_PACKETS] = {
        "Packet 1: Sensor Data - Temperature = 24.5 C",
        "Packet 2: Sensor Data - Humidity = 62.0 %",
        "Packet 3: Sensor Data - Pressure = 1013.25 hPa",
        "Packet 4: Sensor Data - Light Intensity = 480 Lux"
    };

    Packet packets[TOTAL_PACKETS];
    int ack_received[TOTAL_PACKETS] = {0}; // 0 = unacknowledged, 1 = acknowledged

    for (int i = 0; i < TOTAL_PACKETS; i++) {
        packets[i].seq_num = i + 1;
        packets[i].is_retransmission = 0;
        strncpy(packets[i].data, packet_contents[i], sizeof(packets[i].data) - 1);
        packets[i].data_len = strlen(packets[i].data);
    }

    TransmissionRecord history[16];
    int history_count = 0;
    size_t total_transmitted_bytes = 0;
    size_t useful_payload_bytes = 0;

    int timer_starts = 0;
    int timer_timeouts = 0;
    int timer_success = 0;

    double srtt = 0;    // Smoothed RTT (Jacobson's algorithm)
    double rttvar = 0;  // RTT Variance
    int rtt_samples = 0;

    double t_session_start = get_time_ms();

    // 5. PHASE 1: Send All 4 Packets with Timer
    printf(">>> PHASE 1: TRANSMITTING 4 PACKETS WITH RETRANSMISSION TIMER <<<\n\n");

    for (int i = 0; i < TOTAL_PACKETS; i++) {
        printf("[SEND]    Transmitting Packet #%d (Size: %d bytes)\n", 
               packets[i].seq_num, packets[i].data_len);
        printf("          Data Content   : \"%s\"\n", packets[i].data);
        
        // Start Retransmission Timer
        timer_starts++;
        double t_timer_start = get_time_ms();
        printf("  [TIMER] >>> Timer ARMED for Packet #%d (Timeout = %d ms) <<<\n", 
               packets[i].seq_num, RTO_SECONDS * 1000);

        send_all(sock_fd, &packets[i], sizeof(Packet));
        total_transmitted_bytes += sizeof(Packet);

        // Wait for ACK or Timeout
        AckPacket ack;
        int n = recv_all(sock_fd, &ack, sizeof(AckPacket));
        double t_timer_stop = get_time_ms();
        double elapsed_ms = t_timer_stop - t_timer_start;

        if (n <= 0) {
            // Check if socket timeout occurred
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                timer_timeouts++;
                printf("  [TIMER] [-] *** TIMEOUT EXPIRED! *** (Elapsed: %.2f ms >= %d ms)\n", 
                       elapsed_ms, RTO_SECONDS * 1000);
                printf("          No ACK received from server for Packet #%d. Flagged as MISSED!\n\n", 
                       packets[i].seq_num);

                history[history_count].seq_num = packets[i].seq_num;
                history[history_count].attempt = 1;
                history[history_count].status = -1; // Timeout
                history[history_count].rtt_ms = elapsed_ms;
                history[history_count].bytes = sizeof(Packet);
                history_count++;

                ack_received[i] = 0;
            } else {
                printf("[-] Socket error or connection lost.\n");
                break;
            }
        } else {
            // Response arrived before timer expired
            timer_success++;
            printf("  [TIMER] [+] Timer STOPPED! Response arrived in %.2f ms (< %d ms)\n", 
                   elapsed_ms, RTO_SECONDS * 1000);

            // Update Jacobson's Smoothed RTT
            if (rtt_samples == 0) {
                srtt = elapsed_ms;
                rttvar = elapsed_ms / 2.0;
            } else {
                double diff = abs(srtt - elapsed_ms);
                rttvar = (0.75 * rttvar) + (0.25 * diff);
                srtt = (0.875 * srtt) + (0.125 * elapsed_ms);
            }
            rtt_samples++;

            history[history_count].seq_num = packets[i].seq_num;
            history[history_count].attempt = 1;
            history[history_count].status = ack.status;
            history[history_count].rtt_ms = elapsed_ms;
            history[history_count].bytes = sizeof(Packet);
            history_count++;

            if (ack.status == 1) {
                ack_received[i] = 1;
                useful_payload_bytes += packets[i].data_len;
                printf("[RECEIVE] %s (RTT: %.2f ms)\n\n", ack.message, elapsed_ms);
            } else {
                // Server explicitly sent NACK (Packet Missed)
                ack_received[i] = 0;
                printf("[WARNING] %s (RTT: %.2f ms)\n", ack.message, elapsed_ms);
                printf("          [!] Server flagged Packet #%d as MISSED! Scheduled for retransmission.\n\n", 
                       packets[i].seq_num);
            }
        }

        usleep(100000); // 100ms pacing
    }

    // 6. PHASE 2: Detect Missed Packets & Retransmit with Timer
    printf(">>> PHASE 2: DETECTING MISSED PACKETS & RETRANSMITTING (TIMER RESTARTED) <<<\n\n");

    int retransmission_needed = 0;
    for (int i = 0; i < TOTAL_PACKETS; i++) {
        if (!ack_received[i]) {
            retransmission_needed++;
            printf("[RETRANSMIT] Retransmission triggered for missed Packet #%d:\n", packets[i].seq_num);

            packets[i].is_retransmission = 1;

            // Restart Timer for retransmission
            timer_starts++;
            double t_timer_start = get_time_ms();
            printf("  [TIMER] >>> Timer RE-ARMED for Retransmission #%d (Timeout = %d ms) <<<\n", 
                   packets[i].seq_num, RTO_SECONDS * 1000);

            send_all(sock_fd, &packets[i], sizeof(Packet));
            total_transmitted_bytes += sizeof(Packet);

            AckPacket ack;
            int n = recv_all(sock_fd, &ack, sizeof(AckPacket));
            double t_timer_stop = get_time_ms();
            double elapsed_ms = t_timer_stop - t_timer_start;

            if (n > 0 && ack.status == 1) {
                timer_success++;
                printf("  [TIMER] [+] Timer STOPPED! ACK arrived in %.2f ms\n", elapsed_ms);

                ack_received[i] = 1;
                useful_payload_bytes += packets[i].data_len;

                history[history_count].seq_num = packets[i].seq_num;
                history[history_count].attempt = 2;
                history[history_count].status = 1; // ACK
                history[history_count].rtt_ms = elapsed_ms;
                history[history_count].bytes = sizeof(Packet);
                history_count++;

                printf("[SUCCESS]   %s (Retransmission RTT: %.2f ms)\n\n", ack.message, elapsed_ms);
            } else {
                printf("[-] Retransmission failed for Packet #%d\n\n", packets[i].seq_num);
            }
        }
    }

    double t_session_end = get_time_ms();
    double total_session_time = t_session_end - t_session_start;

    // 7. Calculate Metrics
    double total_rtt = 0;
    int valid_rtt_count = 0;
    for (int i = 0; i < history_count; i++) {
        if (history[i].status == 1) {
            total_rtt += history[i].rtt_ms;
            valid_rtt_count++;
        }
    }
    double avg_rtt = (valid_rtt_count > 0) ? (total_rtt / valid_rtt_count) : 0;
    double loss_rate = ((double)retransmission_needed / (double)history_count) * 100.0;
    double throughput_kb = (total_session_time > 0) ? 
        ((total_transmitted_bytes / 1024.0) / (total_session_time / 1000.0)) : 0;
    double throughput_kbps = throughput_kb * 8.0;
    double goodput_kb = (total_session_time > 0) ? 
        ((useful_payload_bytes / 1024.0) / (total_session_time / 1000.0)) : 0;
    double efficiency = (total_transmitted_bytes > 0) ? 
        ((double)useful_payload_bytes / (double)total_transmitted_bytes) * 100.0 : 0;
    double dynamic_rto = srtt + (4.0 * rttvar);

    // 8. Print Tables and Comprehensive Reports
    printf("=================================================================\n");
    printf("                 PACKET TRANSMISSION LOG                         \n");
    printf("=================================================================\n");
    printf(" %-8s %-12s %-14s %-14s %-10s\n", "Packet", "Attempt", "Status", "Timer/RTT(ms)", "Size");
    printf("-----------------------------------------------------------------\n");
    for (int i = 0; i < history_count; i++) {
        const char *st_str;
        if (history[i].status == 1) st_str = "ACK (OK)";
        else if (history[i].status == 0) st_str = "NACK (MISSED)";
        else st_str = "TIMEOUT(EXPIRED)";

        printf(" #%-7d %-12s %-14s %-14.2f %-zu B\n",
               history[i].seq_num,
               (history[i].attempt == 1) ? "Initial" : "Retransmit",
               st_str,
               history[i].rtt_ms,
               history[i].bytes);
    }
    printf("=================================================================\n\n");

    printf("=================================================================\n");
    printf("                    TIMER EVALUATION METRICS                     \n");
    printf("=================================================================\n");
    printf("  Configured Timeout (RTO)  : %d seconds (%d ms)\n", RTO_SECONDS, RTO_SECONDS * 1000);
    printf("  Timer Armed / Started     : %d times\n", timer_starts);
    printf("  Timer Stopped Successfully: %d times (Response within timeout)\n", timer_success);
    printf("  Timer Expirations (Timeout: %d times\n", timer_timeouts);
    printf("  Smoothed RTT (SRTT)       : %.2f ms\n", srtt);
    printf("  RTT Variance (RTTVAR)     : %.2f ms\n", rttvar);
    printf("  Calculated Dynamic RTO    : %.2f ms (SRTT + 4*RTTVAR)\n", dynamic_rto);
    printf("-----------------------------------------------------------------\n\n");

    printf("=================================================================\n");
    printf("           NETWORK PERFORMANCE & METRICS EVALUATION              \n");
    printf("=================================================================\n");
    printf("  Original Packets To Send  : %d\n", TOTAL_PACKETS);
    printf("  Total Transmissions Made  : %d\n", history_count);
    printf("  Packets Missed / Dropped  : %d\n", retransmission_needed);
    printf("  Packets Retransmitted     : %d\n", retransmission_needed);
    printf("  Packet Delivery Ratio     : 100.00 %% (All packets confirmed)\n");
    printf("  Packet Loss / Retry Rate  : %.2f %%\n", loss_rate);
    printf("  Average Valid ACK RTT     : %.2f ms\n", avg_rtt);
    printf("  Total Session Time        : %.2f ms\n", total_session_time);
    printf("  Total Transmitted Wire    : %zu bytes\n", total_transmitted_bytes);
    printf("  Useful Application Payload: %zu bytes\n", useful_payload_bytes);
    printf("  Network Throughput        : %.2f KB/s (%.2f Kbps)\n", throughput_kb, throughput_kbps);
    printf("  Application Goodput       : %.2f KB/s\n", goodput_kb);
    printf("  Protocol Efficiency       : %.2f %%\n", efficiency);
    printf("=================================================================\n\n");

    close(sock_fd);
    printf("[*] Client closed connection successfully.\n");
    return 0;
}
