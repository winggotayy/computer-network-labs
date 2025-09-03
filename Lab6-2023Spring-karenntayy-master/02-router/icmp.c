#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	//fprintf(stderr, "TODO: malloc and send icmp packet.\n");
	// Allocate memory for the outgoing packet
	int packet_len = 0;
	if (type == ICMP_ECHOREPLY) 
	{
		packet_len = len;
	} 
	else 
	{
		struct iphdr *in_pkt_iphdr = packet_to_ip_hdr(in_pkt);
		packet_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + ICMP_HDR_SIZE + IP_HDR_SIZE(in_pkt_iphdr) + ICMP_COPIED_DATA_LEN;
	}
	char *packet = (char *)malloc(packet_len);
	
	// Copy Ethernet header from the incoming packet to the outgoing packet
	struct ether_header *in_pkt_ethhdr = (struct ether_header *)in_pkt;
	struct ether_header *out_pkt_ethhdr = (struct ether_header *)packet;
	memcpy(out_pkt_ethhdr->ether_dhost, in_pkt_ethhdr->ether_dhost, ETH_ALEN);
	memcpy(out_pkt_ethhdr->ether_shost, in_pkt_ethhdr->ether_shost, ETH_ALEN);
	out_pkt_ethhdr->ether_type = htons(ETH_P_IP);

	// Copy IP header from the incoming packet to the outgoing packet
	struct iphdr *in_pkt_iphdr = packet_to_ip_hdr(in_pkt);
	struct iphdr *out_pkt_iphdr = packet_to_ip_hdr(packet);
	rt_entry_t *rt_entry = longest_prefix_match(ntohl(in_pkt_iphdr->saddr));
	ip_init_hdr(out_pkt_iphdr, rt_entry->iface->ip, ntohl(in_pkt_iphdr->saddr), packet_len - ETHER_HDR_SIZE, 1);

	// Copy ICMP header and data to the outgoing packet
	struct icmphdr *out_pkt_icmphdr = (struct icmphdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	out_pkt_icmphdr->type = type;
	out_pkt_icmphdr->code = code;
	if (type == ICMP_ECHOREPLY) 
	{
		memcpy(out_pkt_icmphdr, in_pkt + ETHER_HDR_SIZE + IP_HDR_SIZE(in_pkt_iphdr), len - ETHER_HDR_SIZE - IP_HDR_SIZE(in_pkt_iphdr));
	} 
	else 
	{
		memset(out_pkt_icmphdr + 1, 0, ICMP_COPIED_DATA_LEN);
		memcpy(out_pkt_icmphdr + 1, in_pkt + ETHER_HDR_SIZE, IP_HDR_SIZE(in_pkt_iphdr) + ICMP_COPIED_DATA_LEN);
	}

	// Calculate and set ICMP checksum
	out_pkt_icmphdr->checksum = icmp_checksum(out_pkt_icmphdr, packet_len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE);

	// Send the packet
	ip_send_packet(packet, packet_len);

	// Free the allocated memory
	free(packet);
}
