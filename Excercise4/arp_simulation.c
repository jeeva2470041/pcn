/*
 * File: arp_simulation.c
 * Assignment 7 / 4: Address Resolution Protocol (ARP) Simulation in C
 * Description: Simulates the Address Resolution Protocol (ARP) using sockets / tables.
 *              Maintains an ARP Cache Table (IP Address <-> MAC Address).
 *              When a destination IP is queried:
 *              1. Performs an ARP Cache lookup.
 *              2. On Cache Miss: Broadcasts an ARP Request ("Who has IP X? Tell IP Y").
 *              3. The target node responds with a Unicast ARP Reply ("IP X is at MAC Z").
 *              4. Updates the local ARP cache table with the newly resolved entry.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 10

typedef struct {
    char ip[16];
    char mac[18];
} ARPEntry;

ARPEntry arp_cache[MAX_ENTRIES] = {
    {"192.168.1.1", "00:1A:2B:3C:4D:5E"},
    {"192.168.1.2", "A1:B2:C3:D4:E5:F6"},
    {"192.168.1.3", "12:34:56:78:9A:BC"}
};
int cache_count = 3;

// Global Simulated Network Host Database
ARPEntry network_hosts[] = {
    {"192.168.1.1", "00:1A:2B:3C:4D:5E"},
    {"192.168.1.2", "A1:B2:C3:D4:E5:F6"},
    {"192.168.1.3", "12:34:56:78:9A:BC"},
    {"192.168.1.4", "AA:BB:CC:DD:EE:01"},
    {"192.168.1.5", "FE:DC:BA:98:76:54"},
    {"192.168.1.10", "44:55:66:77:88:99"}
};
int total_network_hosts = 6;

void display_arp_cache() {
    printf("\n------------------------------------------------\n");
    printf(" Current ARP Cache Table on Host (192.168.1.100)\n");
    printf("------------------------------------------------\n");
    printf(" IP Address         | MAC Physical Address\n");
    printf("--------------------+---------------------------\n");
    for (int i = 0; i < cache_count; i++) {
        printf(" %-18s | %-17s\n", arp_cache[i].ip, arp_cache[i].mac);
    }
    printf("------------------------------------------------\n\n");
}

int search_cache(const char *ip) {
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(arp_cache[i].ip, ip) == 0) {
            return i;
        }
    }
    return -1;
}

void resolve_ip(const char *target_ip) {
    printf(">> Resolving Hardware MAC Address for IP: %s\n", target_ip);
    
    // Step 1: Check ARP Cache
    int idx = search_cache(target_ip);
    if (idx != -1) {
        printf("   [ARP CACHE HIT] MAC Address found: %s\n\n", arp_cache[idx].mac);
        return;
    }

    // Step 2: Cache Miss -> Broadcast ARP Request
    printf("   [ARP CACHE MISS] IP not found in local table.\n");
    printf("   [BROADCAST ARP REQUEST] Who has IP: %s? Tell 192.168.1.100\n", target_ip);

    // Step 3: Network Lookup (Simulating host listening on LAN)
    char resolved_mac[18] = "";
    for (int i = 0; i < total_network_hosts; i++) {
        if (strcmp(network_hosts[i].ip, target_ip) == 0) {
            strcpy(resolved_mac, network_hosts[i].mac);
            break;
        }
    }

    if (strlen(resolved_mac) > 0) {
        printf("   [UNICAST ARP REPLY] Host %s replies: \"My MAC is %s\"\n", target_ip, resolved_mac);
        
        // Step 4: Update ARP Cache
        if (cache_count < MAX_ENTRIES) {
            strcpy(arp_cache[cache_count].ip, target_ip);
            strcpy(arp_cache[cache_count].mac, resolved_mac);
            cache_count++;
            printf("   [CACHE UPDATED] Successfully added entry to local ARP table.\n\n");
        }
    } else {
        printf("   [ARP TIMEOUT] Request timed out. Destination host unreachable on local subnet.\n\n");
    }
}

int main() {
    printf("===============================================================\n");
    printf("   Address Resolution Protocol (ARP) Simulation in C\n");
    printf("===============================================================\n");

    display_arp_cache();

    // Query 1: Cache Hit Test
    resolve_ip("192.168.1.2");

    // Query 2: Cache Miss & Dynamic Resolution Test
    resolve_ip("192.168.1.4");

    // Query 3: Non-existent host (ARP Timeout Test)
    resolve_ip("192.168.1.99");

    // Display updated cache
    display_arp_cache();

    return 0;
}
