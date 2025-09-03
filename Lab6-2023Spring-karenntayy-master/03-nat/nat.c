#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) 
    {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	//fprintf(stdout, "TODO: determine the direction of this packet.\n");
	struct iphdr *iphdr = packet_to_ip_hdr(packet);
	rt_entry_t *rt = longest_prefix_match(ntohl(iphdr->saddr));

	if (rt->iface->index == nat.internal_iface->index)
		return DIR_OUT;
	else
		return DIR_IN;

	return DIR_INVALID;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	//fprintf(stdout, "TODO: do translation for this packet.\n");
    pthread_mutex_lock(&nat.lock);

	struct iphdr *ip = packet_to_ip_hdr(packet);
	struct tcphdr *tcp = packet_to_tcp_hdr(packet);
	u32 hash_address = (dir == DIR_IN) ? ntohl(ip->saddr) : ntohl(ip->daddr);
	u8 hash_i = hash8((char *)&hash_address, 4);
	
    // fprintf(stdout,"nat mapping table of hash_value %d\n",hash_i);
	struct list_head *head = &(nat.nat_mapping_list[hash_i]);
	struct nat_mapping *mapping_entry = NULL;

	int found = 0;

	if (dir == DIR_IN)
	{
		found = 0;

		list_for_each_entry(mapping_entry, head, list)
		{
			if (mapping_entry->external_ip == ntohl(ip->daddr) && mapping_entry->external_port == ntohs(tcp->dport))
			{
				found = 1;
				break;
			}
		}

		if (!found)
		{
			mapping_entry = (struct nat_mapping *)malloc(sizeof(struct nat_mapping));
			memset(mapping_entry, 0, sizeof(struct nat_mapping));
			mapping_entry->external_ip = ntohl(ip->daddr);
			mapping_entry->external_port = ntohs(tcp->dport);

			list_add_tail(&(mapping_entry->list), head);
		}

		tcp->dport = htons(mapping_entry->internal_port);
		ip->daddr = htonl(mapping_entry->internal_ip);
		mapping_entry->conn.external_fin = (tcp->flags == TCP_FIN);
		mapping_entry->conn.external_seq_end = tcp->seq;

		if (tcp->flags == TCP_ACK)
			mapping_entry->conn.external_ack = tcp->ack;
	}
	else
	{
		found = 0;

		list_for_each_entry(mapping_entry, head, list)
		{
			if (mapping_entry->internal_ip == ntohl(ip->saddr) && mapping_entry->internal_port == ntohs(tcp->sport))
			{
				found = 1;
				break;
			}
		}

		if (!found)
		{
			mapping_entry = (struct nat_mapping *)malloc(sizeof(struct nat_mapping));
			memset(mapping_entry, 0, sizeof(struct nat_mapping));
			mapping_entry->internal_ip = ntohl(ip->saddr);
			mapping_entry->internal_port = ntohs(tcp->sport);
			mapping_entry->external_ip = nat.external_iface->ip;

			u16 i;
			for (i = NAT_PORT_MIN; i < NAT_PORT_MAX; i++)
			{
				if (!nat.assigned_ports[i])
				{
					nat.assigned_ports[i] = 1;
					break;
				}
			}
			mapping_entry->external_port = i;

			list_add_tail(&(mapping_entry->list), head);
		}

		tcp->sport = htons(mapping_entry->external_port);
		ip->saddr = htonl(mapping_entry->external_ip);
		mapping_entry->conn.internal_fin = (tcp->flags == TCP_FIN);
		mapping_entry->conn.internal_seq_end = tcp->seq;

		if (tcp->flags == TCP_ACK)
			mapping_entry->conn.internal_ack = tcp->ack;
	}

	tcp->checksum = tcp_checksum(ip, tcp);
	ip->checksum = ip_checksum(ip);
	mapping_entry->update_time = time(NULL);

	pthread_mutex_unlock(&nat.lock);

	ip_send_packet(packet, len);
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) 
    {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) 
    {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return;
	}

	do_translation(iface, packet, len, dir);
}

// check whether the flow is finished according to FIN bit and sequence number
// XXX: seq_end is calculated by `tcp_seq_end` in tcp.h
static int is_flow_finished(struct nat_connection *conn)
{
    return (conn->internal_fin && conn->external_fin) && \
            (conn->internal_ack >= conn->external_seq_end) && \
            (conn->external_ack >= conn->internal_seq_end);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		fprintf(stdout, "TODO: sweep finished flows periodically.\n");
		sleep(1);
		pthread_mutex_lock(&nat.lock);
        
        for (int i = 0; i < HASH_8BITS; i++) 
        {
			struct list_head *nat_entry_list = &nat.nat_mapping_list[i];
			struct nat_mapping *nat_entry, *nat_entry_next;
			list_for_each_entry_safe(nat_entry, nat_entry_next, nat_entry_list, list) 
            {
				if (is_flow_finished(&nat_entry->conn) ||
					(time(NULL) - nat_entry->update_time > TCP_ESTABLISHED_TIMEOUT)) 
                {
					log(DEBUG, "remove map entry, port: %d\n", nat_entry->external_port);
					nat.assigned_ports[nat_entry->external_port] = 0;
					list_delete_entry(&nat_entry->list);
					free(nat_entry);
				}
			}
		}

		pthread_mutex_unlock(&nat.lock);
	}

	return NULL;
}

const int MAX_LINE_LEN = 100;

int parse_config(const char *filename)
{
	//fprintf(stdout, "TODO: parse config file, including i-iface, e-iface (and dnat-rules if existing).\n");
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		log(ERROR, "config file does not exist\n");
		return -1;
	}

	char line[MAX_LINE_LEN];
	while (fgets(line, sizeof(line), fp)) {
		char *config = line;
		char *key = strsep(&config, ":");
		if (key == NULL) {
			continue;
		}

		if (strcmp(key, "internal-iface") == 0) {
			char *iface_name = strstrip(config);
			nat.internal_iface = if_name_to_iface(iface_name);
			log(DEBUG, "internal_iface: " IP_FMT "\n", HOST_IP_FMT_STR(nat.internal_iface->ip));
		} else if (strcmp(key, "external-iface") == 0) {
			char *iface_name = strstrip(config);
			nat.external_iface = if_name_to_iface(iface_name);
			log(DEBUG, "external_iface: " IP_FMT "\n", HOST_IP_FMT_STR(nat.external_iface->ip));
		} else if (strcmp(key, "dnat-rules") == 0) {
			char *external_ip_str = strstrip(config);
			char *external_port_str = strsep(&config, ":");
			char *internal_ip_str = strstrip(config);
			char *internal_port_str = strsep(&config, ":");
			
			if (external_ip_str == NULL || external_port_str == NULL ||
				internal_ip_str == NULL || internal_port_str == NULL) {
				continue;
			}

			struct dnat_rule *new_rule = malloc(sizeof(struct dnat_rule));
			memset(new_rule, 0, sizeof(struct dnat_rule));

			new_rule->external_ip = ipv4_to_int(external_ip_str);
			new_rule->external_port = atoi(external_port_str);
			new_rule->internal_ip = ipv4_to_int(internal_ip_str);
			new_rule->internal_port = atoi(internal_port_str);

			init_list_head(&new_rule->list);
			list_add_tail(&new_rule->list, &nat.rules);

			nat.assigned_ports[new_rule->external_port] = 1;

			log(DEBUG, "dnat_rule: " IP_FMT " %d " IP_FMT " %d\n",
				HOST_IP_FMT_STR(new_rule->external_ip), new_rule->external_port,
				HOST_IP_FMT_STR(new_rule->internal_ip), new_rule->internal_port);
		}
    }

	fclose(fp);
	return 0;
}

// initialize
void nat_init(const char *config_file)
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	init_list_head(&nat.rules);

	// seems unnecessary
	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	parse_config(config_file);

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

void nat_exit()
{
	//fprintf(stdout, "TODO: release all resources allocated.\n");

	pthread_mutex_lock(&nat.lock);

	for (int i = 0; i < HASH_8BITS; i++) 
    {
		struct nat_mapping *map_entry, *map_q;
		list_for_each_entry_safe(map_entry, map_q, &(nat.nat_mapping_list[i]), list) 
        {
			list_delete_entry(&(map_entry->list));
			free(map_entry);
		}
	}

	struct dnat_rule *rule, *rule_q;
	list_for_each_entry_safe(rule, rule_q, &nat.rules, list) 
    {
		list_delete_entry(&(rule->list));
		free(rule);
	}

	pthread_mutex_unlock(&nat.lock);
}
