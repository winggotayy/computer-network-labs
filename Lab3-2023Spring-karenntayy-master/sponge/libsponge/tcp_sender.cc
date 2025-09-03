#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

#include <iostream>
#include <vector>
#include <iterator>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()})),
      _initial_retransmission_timeout{retx_timeout},
      _stream(capacity) 
{
    default_initial_retransmission_timeout_ = retx_timeout;
}

uint64_t TCPSender::bytes_in_flight() const 
{ 
    return bytes_in_flight_; 
}

void TCPSender::fill_window() 
{
    //CLOSED
    if(next_seqno_absolute() == 0) 
    {
        //Send the syn
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = _isn;
        
        _segments_out.push(seg);

        _next_seqno = _next_seqno + seg.length_in_sequence_space();
        bytes_in_flight_ = bytes_in_flight_ + seg.length_in_sequence_space();
        
        //Cache the segments
        _cache_segment(next_seqno_absolute() , seg);
    }

    //SYN_ACKED
    if(!stream_in().eof() && next_seqno_absolute() > bytes_in_flight()) 
    {
        size_t seg_maxsz_ = TCPConfig::MAX_PAYLOAD_SIZE;
        
        size_t wdws_remsz_ = peer_windows_size_ ? peer_windows_size_ : 1;
        size_t flight_size_ = bytes_in_flight();
        if(wdws_remsz_ < flight_size_) 
            return;

        wdws_remsz_ = wdws_remsz_ - flight_size_;
        string read_stream_ = _stream.read(min(seg_maxsz_, wdws_remsz_));
        
        while(!read_stream_.empty()) 
        {
            TCPSegment seg;
            seg.header().seqno = next_seqno();
            seg.payload() = Buffer(std::move(read_stream_));

            wdws_remsz_ = wdws_remsz_ - seg.length_in_sequence_space();
            
            if(stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2 && wdws_remsz_ >= 1) 
            {
                seg.header().fin = true;
                wdws_remsz_ = wdws_remsz_ - 1;
            }

            _next_seqno = _next_seqno + seg.length_in_sequence_space();
            bytes_in_flight_ = bytes_in_flight_ + seg.length_in_sequence_space();
            _segments_out.push(seg);

            //Cache the seg
            _cache_segment(next_seqno_absolute(), seg);
            read_stream_ = _stream.read(min(seg_maxsz_, wdws_remsz_));
        }
    }

    //SYN_ACKED
    //stream has reached EOF, but FIN flag hasn't been sent yet
    if(stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2) 
    {
        //Send the fin
        size_t wdws_remsz_ = peer_windows_size_ ? peer_windows_size_ : 1;
        size_t flight_size_ = bytes_in_flight();
        if(wdws_remsz_ <= flight_size_) 
            return;

        TCPSegment seg;
        seg.header().seqno = next_seqno();
        seg.header().fin = true;

        _next_seqno = _next_seqno + seg.length_in_sequence_space();
        bytes_in_flight_ = bytes_in_flight_ + seg.length_in_sequence_space();
        windows_size_ = windows_size_ - seg.length_in_sequence_space();

        _segments_out.push(seg);
        
        //Cache the segments
        _cache_segment(next_seqno_absolute(), seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) 
{ 
    int n_bytes_ = next_seqno() - ackno;
    if(n_bytes_ < 0)
        return;
    
    //Pop all the cache segments
    vector<uint64_t> acknos_pop_;

    for(auto & seg : segments_cache_)
    {
        uint64_t abs_ackno_ = unwrap(ackno, _isn, seg.first);
        if(abs_ackno_ >= seg.first) 
            acknos_pop_.push_back(seg.first);
    }

    for(auto & ackno_pop_ : acknos_pop_)
        segments_cache_.erase(ackno_pop_); 
    
    //Extend window size
    if(window_size > peer_windows_size_) 
    {    
        windows_size_ += window_size - peer_windows_size_;
    }
    peer_windows_size_ = window_size;

    //Valid ackno
    if(!acknos_pop_.empty()) 
    {
        windows_size_ = max(window_size, uint16_t(1));
        bytes_in_flight_ = n_bytes_;
        
        time_ = 0;
        retx_times_ = 0;
        
        _initial_retransmission_timeout = default_initial_retransmission_timeout_;

        fill_window();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) 
{ 
    if(segments_cache_.empty())
        return;

    time_ = time_ + ms_since_last_tick;

    if(time_ >= _initial_retransmission_timeout && retx_times_ <= TCPConfig::MAX_RETX_ATTEMPTS) 
    {
        _segments_out.push(segments_cache_.begin()->second);
        time_ = 0;
        if(peer_windows_size_ != 0) 
        {
            _initial_retransmission_timeout = _initial_retransmission_timeout * 2;
            retx_times_ += 1;
        }
     }
}

unsigned int TCPSender::consecutive_retransmissions() const 
{ 
    return retx_times_; 
}

void TCPSender::send_empty_segment() 
{
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}

void TCPSender::_cache_segment(uint64_t ackno, TCPSegment& seg) 
{
    segments_cache_[ackno] = seg;
}
