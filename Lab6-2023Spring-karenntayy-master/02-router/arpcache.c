#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "icmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy()
{
	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
		struct cached_pkt *pkt_entry = NULL, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry->packet);
			free(pkt_entry);
		}

		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}

	pthread_kill(arpcache.thread, SIGTERM);

	pthread_mutex_unlock(&arpcache.lock);
}

// lookup the IP->mac mapping
//
// traverse the table to find whether there is an entry with the same IP
// and mac address with the given arguments
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
	//fprintf(stderr, "TODO: lookup ip address in arp cache.\n");
	pthread_mutex_lock(&arpcache.lock);
	
	for (int i = 0; i < MAX_ARP_SIZE; i++) 
	{
		if (arpcache.entries[i].ip4 && arpcache.entries[i].valid == ip4) 
		{
			memcpy(mac, arpcache.entries[i].mac, ETH_ALEN);
			pthread_mutex_unlock(&arpcache.lock);
			return 1;
		}
	}

	pthread_mutex_unlock(&arpcache.lock);

	return 0;
}

// append the packet to arpcache
//
// Lookup in the list which stores pending packets, if there is already an
// entry with the same IP address and iface (which means the corresponding arp
// request has been sent out), just append this packet at the tail of that entry
// (the entry may contain more than one packet); otherwise, malloc a new entry
// with the given IP address and iface, append the packet, and send arp request.
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	//fprintf(stderr, "TODO: append the ip address if lookup failed, and send arp request if necessary.\n");
	pthread_mutex_lock(&arpcache.lock);

    // Check if there is an existing entry with the same IP address and iface
    struct arp_req *entry = NULL;
    list_for_each_entry(entry, &arpcache.req_list, list) {
        if (entry->ip4 == ip4 && entry->iface == iface) {
            // Create a new packet entry and append it to the existing entry
            struct cached_pkt *pkt = malloc(sizeof(struct cached_pkt));
            pkt->len = len;
            pkt->packet = packet;
            list_add_tail(&pkt->list, &entry->cached_packets);

            pthread_mutex_unlock(&arpcache.lock);
            return;
        }
    }

    // No existing entry found, create a new one and send ARP request
    struct arp_req *new_entry = malloc(sizeof(struct arp_req));
    new_entry->iface = iface;
    new_entry->ip4 = ip4;
    new_entry->sent = time(NULL);
    new_entry->retries = 0;
    init_list_head(&new_entry->cached_packets);

    // Create a new packet entry and append it to the new entry
    struct cached_pkt *pkt = malloc(sizeof(struct cached_pkt));
    pkt->len = len;
    pkt->packet = packet;
    list_add_tail(&pkt->list, &new_entry->cached_packets);

    // Add the new entry to the arpcache request list
    list_add_tail(&new_entry->list, &arpcache.req_list);

    pthread_mutex_unlock(&arpcache.lock);

    // Send ARP request
    arp_send_request(iface, ip4);
}

// insert the IP->mac mapping into arpcache, if there are pending packets
// waiting for this mapping, fill the ethernet header for each of them, and send
// them out
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
	//printf(stderr, "TODO: insert ip->mac entry, and send all the pending packets.\n");
	pthread_mutex_lock(&arpcache.lock);

    // 查找现有的条目并更新
    for (int i = 0; i < MAX_ARP_SIZE; i++) 
	{
        if (arpcache.entries[i].valid && arpcache.entries[i].ip4 == ip4) 
		{
            memcpy(arpcache.entries[i].mac, mac, ETH_ALEN);
            arpcache.entries[i].added = time(NULL);

            // 发送等待的数据包
            struct arp_req *req_entry, *req_tmp;
            list_for_each_entry_safe(req_entry, req_tmp, &(arpcache.req_list), list) 
			{
                if (req_entry->ip4 == ip4) 
				{
                    struct cached_pkt *pkt_entry, *pkt_tmp;
                    list_for_each_entry_safe(pkt_entry, pkt_tmp, &(req_entry->cached_packets), list) 
					{
                        struct ether_header *eth = (struct ether_header *)pkt_entry->packet;
                        memcpy(eth->ether_dhost, mac, ETH_ALEN);
                        iface_send_packet(req_entry->iface, pkt_entry->packet, pkt_entry->len);
                        list_delete_entry(&(pkt_entry->list));
                        free(pkt_entry);
                    }
                    list_delete_entry(&(req_entry->list));
                    free(req_entry);
                }
            }

            pthread_mutex_unlock(&arpcache.lock);
            return;
        }
    }

    // 寻找空闲条目或最早添加的条目
    int empty_index = -1;
    time_t oldest_time = time(NULL) + 1;
    for (int i = 0; i < MAX_ARP_SIZE; i++) 
	{
        if (!arpcache.entries[i].valid) 
		{
            empty_index = i;
            break;
        }
        if (arpcache.entries[i].added < oldest_time) 
		{
            oldest_time = arpcache.entries[i].added;
            empty_index = i;
        }
    }

    // 更新或替换条目
    arpcache.entries[empty_index].ip4 = ip4;
    memcpy(arpcache.entries[empty_index].mac, mac, ETH_ALEN);
    arpcache.entries[empty_index].added = time(NULL);
    arpcache.entries[empty_index].valid = 1;

    // 发送等待的数据包
    struct arp_req *req_entry, *req_tmp;
    list_for_each_entry_safe(req_entry, req_tmp, &(arpcache.req_list), list) 
	{
        if (req_entry->ip4 == ip4) 
		{
            struct cached_pkt *pkt_entry, *pkt_tmp;
            list_for_each_entry_safe(pkt_entry, pkt_tmp, &(req_entry->cached_packets), list) 
			{
                struct ether_header *eth = (struct ether_header *)pkt_entry->packet;
                memcpy(eth->ether_dhost, mac, ETH_ALEN);
                iface_send_packet(req_entry->iface, pkt_entry->packet, pkt_entry->len);
                list_delete_entry(&(pkt_entry->list));
                free(pkt_entry);
            }
            list_delete_entry(&(req_entry->list));
            free(req_entry);
        }
    }

    pthread_mutex_unlock(&arpcache.lock);
}

// sweep arpcache periodically
//
// For the IP->mac entry, if the entry has been in the table for more than 15
// seconds, remove it from the table.
// For the pending packets, if the arp request is sent out 1 second ago, while 
// the reply has not been received, retransmit the arp request. If the arp
// request has been sent 5 times without receiving arp reply, for each
// pending packet, send icmp packet (DEST_HOST_UNREACHABLE), and drop these
// packets.
void *arpcache_sweep(void *arg) 
{
	while (1) 
    {
		sleep(1);
		//fprintf(stderr, "TODO: sweep arpcache periodically: remove old entries, resend arp requests .\n");
		pthread_mutex_lock(&arpcache.lock);
		time_t now = time(NULL);
		// Remove old entries from the ARP cache
	    for (int i = 0; i < MAX_ARP_SIZE; i++) 
        {
		    if (arpcache.entries[i].valid && (now - arpcache.entries[i].added > ARP_ENTRY_TIMEOUT)) 
            {
			    arpcache.entries[i].valid = 0;
		    }
	    }

	    // Process pending ARP requests
	    struct arp_req *req_entry = NULL, *req_q;
	    list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) 
        {
		    // If the ARP request has been sent 5 times without receiving a reply
		    if (req_entry->retries > ARP_REQUEST_MAX_RETRIES) 
            {
			    struct cached_pkt *pkt_entry, *pkt_q;
			    list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) 
                {
				    // Send ICMP Destination Host Unreachable packet for each pending packet
				    icmp_send_packet(pkt_entry->packet, pkt_entry->len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);

				    list_delete_entry(&(pkt_entry->list));
				    free(pkt_entry->packet);
				    free(pkt_entry);
    			}

			    list_delete_entry(&(req_entry->list));
			    free(req_entry);
		    } 
		    else if ((now - req_entry->sent) > 1) 
            {
			    // If the ARP request has been sent 1 second ago without receiving a reply, retransmit the request
			    arp_send_request(req_entry->iface, req_entry->ip4);
			    req_entry->retries++;
			    req_entry->sent = now;
		    }
	    }

	    pthread_mutex_unlock(&arpcache.lock);
    }

    return NULL;
}
