#include "base.h"
#include <stdio.h>

// XXX ifaces are stored in instace->iface_list
extern ustack_t *instance;

extern void iface_send_packet(iface_info_t *iface, const char *packet, int len);

void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
	// TODO: broadcast packet 
	fprintf(stdout, "TODO: broadcast packet.\n");
	iface_info_t* tx_iface = NULL; 

 	// 遍历所有的interface，参考该语句学习该部分实验中给出的数据结构list的使⽤⽅法 
 	list_for_each_entry(tx_iface, &instance->iface_list, list) 
 	{ 
		if (tx_iface->index != iface->index) 
		{ 
			// 向指定端⼝发包的接⼝函数
			iface_send_packet(tx_iface, packet, len); 
		} 
 	} 
}
