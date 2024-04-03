if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP) {
			// extract ip header
			struct iphdr *ip_hdr = (struct iphdr *)(m.payload + sizeof(struct ether_header));

			// check if packet is for this router
			if (inet_addr(get_interface_ip(m.interface)) == ip_hdr->daddr) {
				if (ip_hdr->protocol == 1) {
					// received an icmp packet
					// extract icmp header
					struct icmphdr *icmp_hdr = (struct icmphdr *)(m.payload + sizeof(struct ether_header) + sizeof(struct iphdr));

					if (icmp_hdr->type == ICMP_ECHO) {
						// send icmp
						send_icmp(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost, eth_hdr->ether_shost, ICMP_ECHOREPLY, ICMP_ECHOREPLY, m.interface, icmp_hdr->un.echo.id, icmp_hdr->un.echo.sequence);
						continue;
					}
					continue;
				}
				continue;
			}

			if (ip_checksum((void *)ip_hdr, sizeof(struct iphdr))) {
				// drop packet
				continue;
			}

			if (ip_hdr->ttl <= 1) {
				send_icmp_error(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost, eth_hdr->ether_shost, ICMP_TIME_EXCEEDED, 0, m.interface);
				continue;
			}

			// if the packet is not for the router, find next hop
			struct route_table_entry *best_match = get_best_route(ip_hdr->daddr);

			if (!best_match) {
				// unreachable destination
				send_icmp_error(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost, eth_hdr->ether_shost, ICMP_DEST_UNREACH, 0, m.interface);
				continue;
			}

			// update checksum after ttl decrementation
			update_checksum(ip_hdr);




 void send_icmp(uint32_t daddr, uint32_t saddr, uint8_t *sha, uint8_t *dha, u_int8_t type, u_int8_t code, int interface, int id, int seq)
{

	struct ether_header eth_hdr;
	struct iphdr ip_hdr;
	struct icmphdr icmp_hdr = {
		.type = type,
		.code = code,
		.checksum = 0,
		.un.echo = {
			.id = id,
			.sequence = seq,
		}};
	packet packet;
	void *payload;

	build_ethhdr(&eth_hdr, sha, dha, htons(ETHERTYPE_IP));
	/* No options */
	ip_hdr.version = 4;
	ip_hdr.ihl = 5;
	ip_hdr.tos = 0;
	ip_hdr.protocol = IPPROTO_ICMP;
	ip_hdr.tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
	ip_hdr.id = htons(1);
	ip_hdr.frag_off = 0;
	ip_hdr.ttl = 64;
	ip_hdr.check = 0;
	ip_hdr.daddr = daddr;
	ip_hdr.saddr = saddr;
	ip_hdr.check = ip_checksum((void *)&ip_hdr, sizeof(struct iphdr));

	icmp_hdr.checksum = icmp_checksum((uint16_t *)&icmp_hdr, sizeof(struct icmphdr));

	payload = packet.payload;
	memcpy(payload, &eth_hdr, sizeof(struct ether_header));
	payload += sizeof(struct ether_header);
	memcpy(payload, &ip_hdr, sizeof(struct iphdr));
	payload += sizeof(struct iphdr);
	memcpy(payload, &icmp_hdr, sizeof(struct icmphdr));
	packet.len = sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr);
	
	packet.interface = interface;

	send_packet(&packet);
}

void send_icmp_error(uint32_t daddr, uint32_t saddr, uint8_t *sha, uint8_t *dha, u_int8_t type, u_int8_t code, int interface)
{

	struct ether_header eth_hdr;
	struct iphdr ip_hdr;
	struct icmphdr icmp_hdr = {
		.type = type,
		.code = code,
		.checksum = 0,
	};
	packet packet;
	void *payload;

	build_ethhdr(&eth_hdr, sha, dha, htons(ETHERTYPE_IP));
	/* No options */
	ip_hdr.version = 4;
	ip_hdr.ihl = 5;
	ip_hdr.tos = 0;
	ip_hdr.protocol = IPPROTO_ICMP;
	ip_hdr.tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
	ip_hdr.id = htons(1);
	ip_hdr.frag_off = 0;
	ip_hdr.ttl = 64;
	ip_hdr.check = 0;
	ip_hdr.daddr = daddr;
	ip_hdr.saddr = saddr;
	ip_hdr.check = ip_checksum((void *)&ip_hdr, sizeof(struct iphdr));

	icmp_hdr.checksum = icmp_checksum((uint16_t *)&icmp_hdr, sizeof(struct icmphdr));

	payload = packet.payload;
	memcpy(payload, &eth_hdr, sizeof(struct ether_header));
	payload += sizeof(struct ether_header);
	memcpy(payload, &ip_hdr, sizeof(struct iphdr));
	payload += sizeof(struct iphdr);
	memcpy(payload, &icmp_hdr, sizeof(struct icmphdr));
	packet.len = sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr);

	packet.interface = interface;

	send_packet(&packet);
}

void build_ethhdr(struct ether_header *eth_hdr, uint8_t *sha, uint8_t *dha, unsigned short type)
{
	memcpy(eth_hdr->ether_dhost, dha, ETH_ALEN);
	memcpy(eth_hdr->ether_shost, sha, ETH_ALEN);
	eth_hdr->ether_type = type;
}

uint16_t ip_checksum(uint8_t *data, size_t size)
{
	// Initialise the accumulator.
	uint64_t acc = 0xffff;

	// Handle any partial block at the start of the data.
	unsigned int offset = ((uintptr_t)data) &3;
	if (offset) {
		size_t count = 4 - offset;
		if (count > size)
			count = size;
		uint32_t word = 0;
		memcpy(offset + (char *)&word, data, count);
		acc += ntohl(word);
		data += count;
		size -= count;
	}

	// Handle any complete 32-bit blocks.
	char* data_end = data + (size & ~3);
	while (data!=data_end) {
		uint32_t word;
		memcpy(&word, data, 4);
		acc += ntohl(word);
		data += 4;
	}

	size &= 3;

	// Handle any partial block at the end of the data.
	if (size) {
		uint32_t word = 0;
		memcpy(&word, data, size);
		acc += ntohl(word);
	}

	// Handle deferred carries.
	acc = (acc & 0xffffffff) + (acc >> 32);
	while (acc >> 16) {
		acc = (acc & 0xffff) + (acc >> 16);
	}

	// If the data began at an odd byte address
	// then reverse the byte order to compensate.
	if (offset & 1) {
		acc = ((acc & 0xff00) >> 8) | ((acc & 0x00ff) << 8);
	}

	// Return the checksum in network byte order.
	return htons(~acc);
} 


void update_checksum(struct iphdr *ip_hdr) {
	uint16_t old_checksum = ip_hdr->check;
	// cast ttl value from uint8_t to uint16_t
	uint16_t old_ttl = (ip_hdr->ttl & 0xFFFF);

	// update ttl and recompute checksum
	--ip_hdr->ttl;
	uint16_t new_ttl = (ip_hdr->ttl & 0xFFFF);	
	ip_hdr->check = ~(~old_checksum + ~old_ttl + new_ttl) - 1;
}