#include "mac.h"
#include "log.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

mac_port_map_t mac_port_map;

// initialize mac_port table
void init_mac_port_table()
{
	bzero(&mac_port_map, sizeof(mac_port_map_t));

	for (int i = 0; i < HASH_8BITS; i++) {
		init_list_head(&mac_port_map.hash_table[i]);
	}

	pthread_mutex_init(&mac_port_map.lock, NULL);

	pthread_create(&mac_port_map.thread, NULL, sweeping_mac_port_thread, NULL);
}

// destroy mac_port table
void destory_mac_port_table()
{
	pthread_mutex_lock(&mac_port_map.lock);
	mac_port_entry_t *entry, *q;
	for (int i = 0; i < HASH_8BITS; i++) {
		list_for_each_entry_safe(entry, q, &mac_port_map.hash_table[i], list) {
			list_delete_entry(&entry->list);
			free(entry);
		}
	}
	pthread_mutex_unlock(&mac_port_map.lock);
}

// lookup the mac address in mac_port table
iface_info_t *lookup_port(u8 mac[ETH_ALEN])
{
	// TODO: implement the lookup process here
	fprintf(stdout, "TODO: implement the lookup process here.\n");
	pthread_mutex_lock(&mac_port_map.lock);
	for (int i = 0; i < HASH_8BITS; i++) 
	{
		mac_port_entry_t *entry;
		list_for_each_entry(entry, &mac_port_map.hash_table[i], list) 
		{
			if (memcmp(entry->mac, mac, ETH_ALEN) == 0) 
			{
				pthread_mutex_unlock(&mac_port_map.lock);
				return entry->iface;
			}
		}
	}
	pthread_mutex_unlock(&mac_port_map.lock);
	return NULL;
}

// 计算MAC地址的哈希值
int calculate_hash(u8 mac[ETH_ALEN])
{
	int hash = 0;
	for (int i = 0; i < ETH_ALEN; i++) {
		hash += mac[i]; // 将MAC地址的每个字节累加到哈希值中
	}
	hash %= HASH_8BITS; // 取模操作，确保哈希值在0到HASH_8BITS-1之间
	return hash;
}

// insert the mac -> iface mapping into mac_port table
void insert_mac_port(u8 mac[ETH_ALEN], iface_info_t *iface)
{
	// TODO: implement the insertion process here
	fprintf(stdout, "TODO: implement the insertion process here.\n");

	pthread_mutex_lock(&mac_port_map.lock);

	mac_port_entry_t *entry = malloc(sizeof(mac_port_entry_t));
	if (entry == NULL) 
	{
		// fprintf(stderr, "Failed to allocate memory for mac_port_entry_t.\n");
		pthread_mutex_unlock(&mac_port_map.lock);
		return;
	}
	memset(entry, 0, sizeof(mac_port_entry_t));
	
	time_t now = time(NULL);
	entry->visited = now;
	memcpy(entry->mac, mac, ETH_ALEN);
	entry->iface = iface;

	int hash = calculate_hash(mac);
	list_add_tail(&entry->list, &mac_port_map.hash_table[hash]);

	pthread_mutex_unlock(&mac_port_map.lock);
}

// dumping mac_port table
void dump_mac_port_table()
{
	mac_port_entry_t *entry = NULL;
	time_t now = time(NULL);

	fprintf(stdout, "dumping the mac_port table:\n");
	pthread_mutex_lock(&mac_port_map.lock);
	for (int i = 0; i < HASH_8BITS; i++) {
		list_for_each_entry(entry, &mac_port_map.hash_table[i], list) {
			fprintf(stdout, ETHER_STRING " -> %s, %d\n", ETHER_FMT(entry->mac), \
					entry->iface->name, (int)(now - entry->visited));
		}
	}

	pthread_mutex_unlock(&mac_port_map.lock);
}

// sweeping mac_port table, remove the entry which has not been visited in the
// last 30 seconds.
int sweep_aged_mac_port_entry()
{
	// TODO: implement the sweeping process here
	fprintf(stdout, "TODO: implement the sweeping process here.\n");
	int num = 0;
	
	mac_port_entry_t *entry = NULL, *q = NULL;
	time_t now = time(NULL);
	
	pthread_mutex_lock(&mac_port_map.lock);
	
	for (int i = 0; i < HASH_8BITS; i++) 
	{
		list_for_each_entry_safe(entry, q, &mac_port_map.hash_table[i], list) 
		{
			if((int)(now - entry->visited) > MAC_PORT_TIMEOUT)
			{
				list_delete_entry(&entry->list);
				free(entry);
				num ++;
			}
		}
	}
	
	pthread_mutex_unlock(&mac_port_map.lock);
	
	return num;
	//return 0;
}

// sweeping mac_port table periodically, by calling sweep_aged_mac_port_entry
void *sweeping_mac_port_thread(void *nil)
{
	while (1) {
		sleep(1);
		int n = sweep_aged_mac_port_entry();

		if (n > 0)
			log(DEBUG, "%d aged entries in mac_port table are removed.", n);
	}

	return NULL;
}
