00-broadcast
=============

My name: 郑凯琳 TAY KAI LIN

My Student ID: 205220025

This lab took me about 2 hours to do.

Implementation Explanation:

**`void broadcast_packet`**

<u>函数的目标：</u>

将接收到的数据包广播到除了接收到该数据包的接口之外的所有接口。

<u>函数的输入参数：</u>

`iface_info_t *iface`: 接收到数据包的接口
`const char *packet`: 要广播的数据包
`int len`: 数据包的长度

<u>函数的实现思路：</u>

1. 声明一个指针变量 `tx_iface` 用于遍历接口列表，并初始化为 `NULL`。

2. 使用 `list_for_each_entry` 宏遍历   `instance->iface_list`，该宏可以依次将列表中的每个元素赋值给 `tx_iface`。

3. 在循环中，通过比较 `tx_iface` 的 `index` 和接收到数据包的接口的 `index` 来确定是否为同一个接口。

4. 如果 `tx_iface` 的 `index` 不等于接收到数据包的接口的 `index` ，则表示 `tx_iface` 是一个不同的接口。

5. 调用 `iface_send_packet` 函数，将数据包发送到 `tx_iface` 对应的接口。

Screenshots:
2023-05-24.png
2023-05-24 (1).png

Remaining Bugs:
