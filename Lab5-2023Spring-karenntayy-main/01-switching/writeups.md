01-switching
=============

My name: 郑凯琳 TAY KAI LIN

My Student ID: 205220025

This lab took me about 5 hours to do.

Implementation Explanation:

**(一) `iface_info_t *lookup_port(u8 mac[ETH_ALEN])`**

<u>功能：</u>
在 `mac_port` 表中查找给定MAC地址的接口信息

<u>思路：互斥锁 & 遍历哈希桶和链表</u>
1. 获取 `mac_port_map` 的互斥锁，以确保在查找过程中表的一致性。
2. 循环遍历 `mac_port_map` 中的所有哈希桶（0到HASH_8BITS-1）。
3. 在每个哈希桶中，使用 `list_for_each_entry` 宏遍历链表中的每个条目。
4. 对于每个条目，使用 `memcmp` 函数比较条目的MAC地址和给定的MAC地址。
5. 如果找到匹配的条目，则释放互斥锁并返回条目对应的接口信息。
6. 如果在整个表中未找到匹配的条目，则释放互斥锁并返回 `NULL`，表示未找到匹配的接口信息。

**(二) `void insert_mac_port(u8 mac[ETH_ALEN], iface_info_t *iface)`**

<u>功能：</u>
将MAC地址和接口信息插入到 `mac_port` 表中

<u>思路：互斥锁 & 哈希表</u>
1. 获取 `mac_port_map` 的互斥锁，以确保在插入过程中表的一致性。
2. 分配一个新的 `mac_port_entry_t` 结构体，并进行初始化。
3. 获取当前时间，并将其设置为新条目的 `visited` 字段。
4. 使用 `memcpy` 函数将给定的MAC地址复制到新条目的 `mac` 字段。
5. 使用 `calculate_hash` 函数计算给定MAC地址的哈希值。
6. 使用 `list_add_tail` 函数将新条目插入到对应哈希桶的链表末尾。
7. 释放互斥锁，完成插入操作。

**(三) `int sweep_aged_mac_port_entry()`**

<u>功能：</u>
清理 `mac_port` 表中那些在过去30秒内没有访问过的条目

<u>思路：互斥锁 & 遍历链表</u>
1. 获取 `mac_port_map` 的互斥锁，以确保在清理过程中表的一致性。
2. 初始化一个变量 `num` 用于计算被清理的条目数量。
3. 获取当前时间。
4. 对于每个哈希桶，使用 `list_for_each_entry_safe` 宏遍历链表中的条目。
5. 检查每个条目的 `visited` 字段与当前时间的差值是否大于 `MAC_PORT_TIMEOUT` ，即是否超过了30秒。
6. 如果是超过30秒未访问的条目，则从链表中删除该条目，释放其内存，并将 `num` 计数加一。
7. 释放互斥锁，完成清理操作。
8. 返回被清理的条目数量。

**(四) `void handle_packet(iface_info_t *iface, char *packet, int len)`**

<u>功能：</u>
处理接收到的数据包

<u>思路：查找目标MAC地址并进行转发或广播 & 更新源MAC地址和接口的映射关系</u>
1. 解析数据包的以太网头部，获取目标MAC地址。
2. 使用 `lookup_port` 函数在 `mac_port` 表中查找目标MAC地址对应的接口。
3. 如果找到了目标接口(`tx_iface`)，则使用该接口将数据包转发出去，调用`iface_send_packet`函数。
4. 如果没有找到目标接口，则表示该数据包需要进行广播，调用 `broadcast_packet` 函数将数据包从接收接口广播出去。
5. 检查源MAC地址是否存在于 `mac_port` 表中。
6. 如果源MAC地址不存在于表中，则将源MAC地址和接口信息插入到 `mac_port` 表中，调用`insert_mac_port` 函数。
7. 记录日志，输出目标MAC地址信息。
8. 释放数据包的内存。

Screenshots:
2023-05-25.png
2023-05-25 (2).png

Remaining Bugs:

