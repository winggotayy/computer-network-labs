Lab 4 Writeup
=============

My name: 郑凯琳 TAY KAI LIN

My Student number : 205220025

This lab took me about 12 hours to do. I did attend the lab session.

#### 1. Program Structure and Design:

核心函数 -- 思路 & 代码：

**（1）发起连接**
- 将 `sender` 中的 `segment` push到 `connection` 中
- 尽量设置 `ackno` 和 `window_size`

Code:

<u>void TCPConnection::_send_data()</u>

    while(!_sender.segments_out().empty()) 
    {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
            
        if(_receiver.ackno().has_value()) 
        {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }

**（1.1）发起连接**
- 调用 `TCPSender::fill_window` ，发送 `SYN`

Code:

    void TCPConnection::connect() 
    {
        _sender.fill_window();
        _send_data();
    }

**（2）关闭连接**
- 发送完毕：
    - `sender` 的 `stream_in` 已经 `eof`
    - `receiver` 的 `stream_out` 已经 `input_ended`

Code:

<u>void TCPConnection::_send_data()</u>

    if(_receiver.stream_out().input_ended()) 
    {
        if(!_sender.stream_in().eof())
            _linger_after_streams_finish = false;
        
        else if(_sender.bytes_in_flight() == 0) 
        {
            if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) 
                active_ = false;
        }
    }

**（3）写入数据 & 发送数据包**
- 将数据写入 `TCPSender` 的 `ByteStream` 中
- 填充窗口，发送

Code:

    size_t TCPConnection::write(const string &data) {
        //DUMMY_CODE(data);

        if(data.empty()) 
            return 0;

        // 在sender中写入数据并发送
        size_t size = _sender.stream_in().write(data);
        _sender.fill_window();
        _send_data();
        return size;
    }

**（4）接收数据包**

TCP 连接状态

image.png

TCP 三次握手

image.png

<u>CLOSED / LISTEN</u>
- CLOSED：初始状态，表示TCP连接是 “关闭着” 或 “未打开” 的
- LISTEN：表示服务器端的某个SOCKET处于监听状态，可以接受客户端的连接
- 等待来自远程TCP应用程序的请求

- 收到 `SYN` ：说明TCP连接由对方启动，进入Syn-Revd状态
- 因为还没有ACK，所以 `sender` 不需要 `ack_received`
- 主动发送一个 `SYN`

Code:

    else if(_sender.next_seqno_absolute() == 0)
    {
        // 收到SYN：TCP连接由对方启动，进入SYN-REVD状态
        if(seg.header().syn)
        {
            // 还没有ACK，sender不需要ack_received
            _receiver.segment_received(seg);
            // 主动发送一个SYN
            connect();
        }
    }

<u>SYN-SENT</u>
- 发送连接请求后等待来自远程端点的确认
- TCP第一次握手后客户端所处的状态

- 收到 `SYN` 和 `ACK`：说明由对方主动开启连接，进入ESTABLISHED状态
    - 通过一个空包来发送 `ACK`
- 只收到了 `SYN`：说明由双方同时开启连接，进入SYN-REVD状态
    - 没有接收到对方的 `ACK`，主动发一个

Code:
    
    else if(_sender.next_seqno_absolute() == _sender.bytes_in_flight() && !_receiver.ackno().has_value())
    {
        // 收到SYN和ACK：由对方主动开启连接，进入ESTABLISHED状态
        if(seg.header().syn && seg.header().ack)
        {
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _receiver.segment_received(seg);
            // 通过空包发送一个ACK
            _sender.send_empty_segment();
            _send_data();
        }
        // 只收到SYN：由双方同时开启连接，进入SYN-REVD状态
        else if(seg.header().syn && !seg.header().ack)
        {
            // 没有接收到对方的ACK
            _receiver.segment_received(seg);
            // 主动发送一个ACK
            _sender.send_empty_segment();
            _send_data();
        }
    }

<u>SYN-REVD</u>
- 该端点已经接收到连接请求并发送确认
- 该端点正在等待最终确认
- TCP第二次握手后服务端所处的状态

- 输入没有结束
- 接收 `ACK`，进入ESTABLISHED状态

Code:

    else if(_sender.next_seqno_absolute() == _sender.bytes_in_flight() && _receiver.ackno().has_value() && !_receiver.stream_out().input_ended())
    {
        // 接收ACK，进入ESTABLISHED状态
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
    }

<u>ESTABLISHED</u>
- 代表连接已经建立起来
- 连接数据传输阶段的正常状态

- 发送数据
- 如果接到数据，则更新 `ACK`

Code:

    else if(_sender.next_seqno_absolute() > _sender.bytes_in_flight() && !_sender.stream_in().eof())
    {
        // 发送数据：如果接到数据，则更新ACK
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);

        if(seg.length_in_sequence_space() > 0) 
            _sender.send_empty_segment();
        _sender.fill_window();
        _send_data();
    }

TCP 四次挥手

image.png

<u>FIN-WAIT-1</u>
- 等待来自远程TCP的终止连接请求或终止请求的确认

- 收到 `FIN`：发送新 `ACK`，进入CLOSING/TIME-WAIT
- 收到 `ACK`：进入FIN-WAIT-2

Code:

    else if(_sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && _sender.bytes_in_flight() > 0 
    && _sender.stream_in().eof() && !_receiver.stream_out().input_ended()) 
    {
        if(seg.header().fin) 
        {
            // 收到FIN：发送新ACK，进入CLOSING/TIME-WAIT
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _receiver.segment_received(seg);
            _sender.send_empty_segment();
            _send_data();
        } 
        else if(seg.header().ack) 
        {
            // 收到ACK：进入FIN-WAIT-2
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _receiver.segment_received(seg);
            _send_data();
        }
    }

<u>FIN-WAIT-2</u>
- 在此端点发送终止连接请求
- 等待来自远程TCP的连接终止请求

Code:

    else if(_sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && _sender.bytes_in_flight() == 0 
    && _sender.stream_in().eof() && !_receiver.stream_out().input_ended()) 
    {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        _sender.send_empty_segment();
        _send_data();
    }

<u>TIME-WAIT</u>
- 等待足够的时间来确保远程TCP接收到其连接终止请求的确认

- 收到FIN：保持TIME-WAIT状态
    - 可靠地实现TCP的全双工连接终止
    - 允许旧的重复数据段在网络中过期

Code:

    else if(_sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && _sender.bytes_in_flight() == 0 
    && _sender.stream_in().eof() && _receiver.stream_out().input_ended()) 
    {
        if(seg.header().fin) 
        {
           // 收到FIN：保持TIME-WAIT状态
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _receiver.segment_received(seg);
            _sender.send_empty_segment();
            _send_data();
        }
    }

<u>其他</u>

Code:

    else 
    {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        _sender.fill_window();
        _send_data();
    }

#### 2. Implementation Challenges:

...

...

#### 3. Remaining Bugs:

...

...

*More details and requirements of sections above can be found in `lab5_tutorials.pdf/10.submit`*
