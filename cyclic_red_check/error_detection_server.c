/*
 * File: error_detection_server.c
 * Assignment 6: TCP Error Detection Receiver Server for Text Strings (Checksum & CRC)
 * Description: Receives text string payloads with appended Checksum or CRC,
 *              recalculates verification totals, and reports ACK/NACK status.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define GENERATOR "10011" // G(x) = x^4 + x + 1

// Modulo-2 binary XOR division for CRC
void crc_modulo2_div(const char *data_bits, const char *gen, char *remainder) {
    int data_len = strlen(data_bits);
    int gen_len = strlen(gen);
    char temp[BUFFER_SIZE];
    strcpy(temp, data_bits);

    for (int i = 0; i <= data_len - gen_len; i++) {
        if (temp[i] == '1') {
            for (int j = 0; j < gen_len; j++) {
                temp[i + j] = (temp[i + j] == gen[j]) ? '0' : '1';
            }
        }
    }
    strncpy(remainder, temp + data_len - (gen_len - 1), gen_len - 1);
    remainder[gen_len - 1] = '\0';
}

// Convert binary bit stream back to readable text string
void bits_to_text(const char *bits, int num_chars, char *text_out) {
    for (int i = 0; i < num_chars; i++) {
        char ch = 0;
        for (int j = 0; j < 8; j++) {
            ch = (ch << 1) | (bits[i * 8 + j] - '0');
        }
        text_out[i] = ch;
    }
    text_out[num_chars] = '\0';
}

// 8-bit 1's complement addition with end-around carry
unsigned char verify_checksum(const unsigned char *data, int len, unsigned char received_checksum) {
    unsigned int sum = 0;
    printf("    [*] Receiver Computing 1's Complement Summation:\n");
    for (int i = 0; i < len; i++) {
        sum += data[i];
        while (sum >> 8) {
            sum = (sum & 0xFF) + (sum >> 8); // End-around carry
        }
        printf("        + Char '%c' (0x%02X) -> Intermediate Sum: 0x%02X\n", data[i], data[i], sum & 0xFF);
    }
    sum += received_checksum;
    while (sum >> 8) {
        sum = (sum & 0xFF) + (sum >> 8);
    }
    printf("        + Received Checksum: 0x%02X -> Final Total: 0x%02X\n", received_checksum, sum & 0xFF);
    return (~sum) & 0xFF;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE], response[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    printf("====================================================\n");
    printf("    TCP ERROR DETECTION RECEIVER SERVER (PORT %d)   \n", PORT);
    printf("====================================================\n");
    printf("[*] Generator Polynomial G(x) = x^4 + x + 1 (10011)\n");
    printf("[*] Ready to receive and verify string transmissions...\n\n");

    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) continue;

        bzero(buffer, BUFFER_SIZE);
        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) { close(client_fd); continue; }
        buffer[bytes] = '\0';

        char type[16];
        sscanf(buffer, "%s", type);

        if (strcmp(type, "CHECKSUM") == 0) {
            char message[256];
            unsigned int rec_chk;
            sscanf(buffer, "CHECKSUM %s %x", message, &rec_chk);
            int len = strlen(message);

            printf("[<-] Received CHECKSUM Frame for String: \"%s\" (Length: %d chars, Checksum: 0x%02X)\n",
                   message, len, rec_chk);
            unsigned char result = verify_checksum((unsigned char *)message, len, (unsigned char)rec_chk);

            if (result == 0x00) {
                snprintf(response, sizeof(response), "ACK: Checksum 0x%02X Verified (String \"%s\" Accepted Without Error)",
                         rec_chk, message);
                printf("    [+] Status: PASS (Result: 0x00) -> String \"%s\" Accepted.\n\n", message);
            } else {
                snprintf(response, sizeof(response), "NACK: Checksum Mismatch (Result: 0x%02X != 0x00 - Corrupted String Discarded)",
                         result);
                printf("    [-] Status: FAIL (Result: 0x%02X) -> Corrupted Frame Discarded.\n\n", result);
            }
            send(client_fd, response, strlen(response), 0);

        } else if (strcmp(type, "CRC") == 0) {
            char codeword[BUFFER_SIZE], remainder[16];
            int char_len;
            sscanf(buffer, "CRC %d %s", &char_len, codeword);
            
            char decoded_text[256];
            bits_to_text(codeword, char_len, decoded_text);

            printf("[<-] Received CRC Codeword for String: \"%s\" (%d bits)\n", decoded_text, (int)strlen(codeword));
            printf("    -> Raw Bitstream: %s\n", codeword);

            crc_modulo2_div(codeword, GENERATOR, remainder);
            printf("    [*] Recalculated CRC Remainder using G(x) [%s]: %s\n", GENERATOR, remainder);

            int is_zero = (strcmp(remainder, "0000") == 0);
            if (is_zero) {
                snprintf(response, sizeof(response), "ACK: CRC Remainder is 0000 (String \"%s\" Verified Error-Free)",
                         decoded_text);
                printf("    [+] Status: PASS (Remainder: %s) -> String \"%s\" Accepted.\n\n", remainder, decoded_text);
            } else {
                snprintf(response, sizeof(response), "NACK: Non-Zero CRC Remainder [%s] (Bit Errors Detected! String Discarded)",
                         remainder);
                printf("    [-] Status: FAIL (Remainder: %s) -> Bit Error Detected.\n\n", remainder);
            }
            send(client_fd, response, strlen(response), 0);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
