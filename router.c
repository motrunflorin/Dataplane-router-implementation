#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "lib.h"
#include "trie.h"
#include "protocols.h"
#include <arpa/inet.h>

#define IP_LENGHT 32
#define ETHERTYPE_IP 0x0800
#define TABLE_LEN 1000000

struct route_table_entry *rtable;
int rtable_len;

struct arp_entry *arp_table;
int arp_table_len;

struct route_table_entry *get_best_route(uint32_t ip_dest, trie_node_t *root) {
	trie_node_t *node = root;
	struct route_table_entry *best = NULL;

	// Longest Prefix Match
	for (int i = 0; i < IP_LENGHT; ++i) {

		int bit = (ip_dest >> i) & 1;

		if (node->is_leaf)
			best = node->data;
		
		if (!node->child[bit])
			return best;

		node = node->child[bit];
	}

	if (node && node->is_leaf == 1)
		return node->data;

	return NULL;
}

struct arp_entry *get_mac_entry(uint32_t ip_dest) {
	// search for an entry that matches ip_dest

	for (int i = 0; i < arp_table_len; i++) {

		if (arp_table[i].ip == ip_dest)
			return &arp_table[i];
		
	}

	return NULL;
}

void icmp(struct iphdr *ip_hdr, struct ether_header *eth_hdr, uint8_t type, int interface) {

	
	struct icmphdr *icmp = calloc(1, sizeof(struct icmphdr));
	
	// set ICMP type 
	icmp->type = type;
	icmp->checksum = htons(checksum((uint16_t*)(icmp), sizeof(struct icmphdr)));

	struct iphdr *ip_icmp = calloc(1, sizeof(struct iphdr));
	
	// set IP
	ip_icmp->version = 4;
	ip_icmp->ihl = 5;
	ip_icmp->id = htons(1);
	ip_icmp->daddr = ip_hdr->saddr;
	ip_icmp->saddr = inet_addr(get_interface_ip(interface));
	ip_icmp->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
	ip_icmp->protocol = IPPROTO_ICMP;
	ip_icmp->check = htons(checksum((uint16_t*)(ip_icmp), sizeof(struct iphdr)));

	struct ether_header* eth_hdr2 = calloc(1, sizeof(struct ether_header));
	eth_hdr2->ether_type = htons(ETHERTYPE_IP);

	// set MAC address
	get_interface_mac(interface, eth_hdr2->ether_shost);
	memcpy(eth_hdr2->ether_dhost, eth_hdr->ether_shost, 6);

	// copy in ICMP response buffer
	char icmp_response[MAX_PACKET_LEN];
	memcpy(icmp_response, eth_hdr2, sizeof(struct ether_header));
	memcpy(icmp_response + sizeof(struct ether_header), ip_icmp, sizeof(struct iphdr));
	memcpy(icmp_response + sizeof(struct ether_header) + sizeof(struct iphdr), icmp, sizeof(struct icmphdr));

	send_to_link(interface, icmp_response, sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr));
	
}

int main(int argc, char *argv[]) {
	char buf[MAX_PACKET_LEN];

	// Do not modify this line
	init(argc - 2, argv + 2);

	// allocate MAC and route tables
	rtable = malloc(sizeof(struct route_table_entry) * TABLE_LEN);
	DIE(rtable == NULL, "memory");

	arp_table = malloc(sizeof(struct arp_entry) * TABLE_LEN);
	DIE(arp_table == NULL, "memory");

	// Read static routing table and MAC table
	rtable_len = read_rtable(argv[1], rtable);
	arp_table_len = parse_arp_table("arp_table.txt", arp_table);

	// trie
	trie_node_t *root = create_trie_node();
	for (int i = 0; i < rtable_len; ++i)
		insert(root, rtable[i].prefix, &rtable[i]);
		

	while (1) {

		int interface;
		size_t len;

		interface = recv_from_any_link(buf, &len);
		DIE(interface < 0, "recv_from_any_links");

		struct ether_header *eth_hdr = (struct ether_header *)buf;
		struct iphdr *ip_hdr = (struct iphdr *)(buf + sizeof(struct ether_header));

		// Check ip_hdr integrity
		uint16_t old_check = ntohs(ip_hdr->check);

		ip_hdr->check = 0;
		if (old_check != checksum((uint16_t *)ip_hdr, sizeof(struct iphdr))) {
			memset(buf, 0, sizeof(buf));
			continue;
		}

		if (ip_hdr->daddr == inet_addr(get_interface_ip(interface))) {

			icmp(ip_hdr, eth_hdr, 0, interface);
			continue;
		}

		struct route_table_entry *best_router = get_best_route(ip_hdr->daddr, root);
		if (best_router == NULL) {
			
			icmp(ip_hdr, eth_hdr, 3, interface);
			continue;
		}

		// check ttl
		if (ip_hdr->ttl <= 1) {

			icmp(ip_hdr, eth_hdr, 11, interface);
			continue;
		}		
		
		// update TLL and checksum
		ip_hdr->ttl--;
		ip_hdr->check = 0;
		ip_hdr->check = htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

		// update the ethernet addresses; find the destination MAC address
		struct arp_entry *nexthop_mac = get_mac_entry(best_router->next_hop);
		if (nexthop_mac == NULL)
			continue;
		
		memcpy(eth_hdr->ether_dhost, nexthop_mac->mac, 6);

		// find the destination mac address of interface.
		get_interface_mac(best_router->interface, eth_hdr->ether_shost);

		send_to_link(best_router->interface, buf, len);
	}
}
