#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const 
{ 
    return _sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const 
{ 
    return _sender.bytes_in_flight(); 
}

size_t TCPConnection::unassembled_bytes() const 
{ 
    return _receiver.unassembled_bytes(); 
}

size_t TCPConnection::time_since_last_segment_received() const 
{ 
    return time_since_last_segment_received_; 
}

void TCPConnection::segment_received(const TCPSegment &seg) { 
    //DUMMY_CODE(seg);

    // 非启动：不接收
    if(!active_)
        return;

    time_since_last_segment_received_ = 0;

    // RST标志：关闭连接
    if(seg.header().rst)
    {
        // 在出站流中标记错误
        _receiver.stream_out().set_error();
        // 在入站流中标记错误
        _sender.stream_in().set_error();
        // 使active返回false
        active_ = false;
    }

    // 状态：CLOSED / LISTEN
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

     // 状态：SYN-SENT
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

    // 状态：SYN-REVD，并且输入没有结束
    else if(_sender.next_seqno_absolute() == _sender.bytes_in_flight() && _receiver.ackno().has_value() && !_receiver.stream_out().input_ended())
    {
        // 接收ACK，进入ESTABLISHED状态
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
    }

    // 状态：ESTABLISHED，连接已建立
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

    // 状态：FIN-WAIT-1
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

   // 状态：FIN-WAIT-2
    else if(_sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && _sender.bytes_in_flight() == 0 
    && _sender.stream_in().eof() && !_receiver.stream_out().input_ended()) 
    {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        _sender.send_empty_segment();
        _send_data();
    }

    // 状态：TIME-WAIT
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
    
    // 状态：其他
    else 
    {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        _sender.fill_window();
        _send_data();
    }
}

bool TCPConnection::active() const 
{ 
    return active_; 
}

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

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) 
{ 
    //DUMMY_CODE(ms_since_last_tick);

    // 非启动：不接收
    if(!active_)
       return;

    // 保存时间，并通知sender
    time_since_last_segment_received_ += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    // 超时：重置连接
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) 
    {
       _reset_connection();
       return;
    }
    _send_data(); 
}

void TCPConnection::end_input_stream() 
{
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_data();
}

void TCPConnection::connect() 
{
    // 主动启动：fill_window方法 - 发送SYN
    _sender.fill_window();
    _send_data();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_send_data() 
{
    // 将sender中的数据保存到connection中
    while(!_sender.segments_out().empty()) 
    {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        // 尽量设置ackno和window_size
        if(_receiver.ackno().has_value()) 
        {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
    // 发送完毕：结束连接
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
}

void TCPConnection::_reset_connection() 
{
    // 发送RST标志
    TCPSegment seg;
    seg.header().rst = true;
    _segments_out.push(seg);

    // 在出站流中标记错误
    _receiver.stream_out().set_error();
    // 在入站流中标记错误
    _sender.stream_in().set_error();
    // 使active返回false
    active_ = false;
}
