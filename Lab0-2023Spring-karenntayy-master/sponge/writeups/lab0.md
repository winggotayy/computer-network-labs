Lab 0 Writeup
=============

My name: 郑凯琳 TAY KAI LIN

My Student number : 205220025

This lab took me about 6 hours to do. I did attend the lab session.

My secret code from section 2.1 was: [code here]

#### 1. Program Structure and Design:

(1) 建立tcp连接

    TCPSocket sock;
    sock.connect(Address(host, "http"));

(2) 发送(write)字节流：请求报文

    sock.write("GET " + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");

(3) 接收(read)字节流：打印回复报文的内容
    报文内容可能不止一个，需通过检查EOF标志位来判断是否接收完毕

    string str;
    while (!sock.eof()) 
    {
        sock.read(str);
        cout << str;
    }

(4) 关闭tcp连接

    sock.close();

#### 2. Implementation Challenges:

问题：环境配置 - VSC无法远程接入实验环境
解决方法：创建共享文件
在主机创建share文件，在虚拟机创建share文件，设置其二share文件为共享文件
2023-03-17-044716_1024x768_scrot.png9(1).png

#### 3. Remaining Bugs:

...

...

*More details and requirements of sections above can be found in `lab0_tutorials.pdf/4.submit`*





