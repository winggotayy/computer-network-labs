#include "stream_reassembler.hh"

#include <iostream>
#include <vector>
#include <map>
#include <iterator>

// Dummy implementation of a stream reassembler.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    assemble_idx(0),            
    eof_idx(-1),
    _segments({}),
    _output(capacity), 
    _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    //DUMMY_CODE(data, index, eof);
    if(eof)
        eof_idx = data.size() + index;
    
    //Non-expected segement: Cache
    if(index > assemble_idx) 
    {
        merge_segments(index, data);
        return;
    } 

    //Expected segment: Write to ByteStream
    int _begin = assemble_idx - index;
    int write_cnt = data.size() - _begin;
    //no enough space
    if(write_cnt < 0)
        return;

    assemble_idx += _output.write(data.substr(_begin, write_cnt));

    //Find next segment
    vector<size_t> pop_list;
    for(auto segment : _segments) 
    {
        //processed or empty string
        if(segment.first + segment.second.size() <= assemble_idx || segment.second.size() == 0) 
        {
            pop_list.push_back(segment.first);
            continue;
        } 

        //not yet
        if(assemble_idx < segment.first) 
            continue;

        _begin = assemble_idx - segment.first;
        write_cnt = segment.second.size() - _begin;
        assemble_idx += _output.write(segment.second.substr(_begin, write_cnt));
        pop_list.push_back(segment.first);
    }

    //Remove useless segment
    for(auto segment_id : pop_list) 
        _segments.erase(segment_id);

    if(empty() && assemble_idx == eof_idx) 
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { 
    size_t ub = 0;
    for(auto segment : _segments)
        ub += segment.second.size();
    return ub;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }

void StreamReassembler::merge_segments(size_t index, const string& data)
{
    size_t data_lft = index;
    size_t data_rgt = index + data.size();
    string data_cpy = data;
    vector<size_t> remove_list;
    bool cached = true;

    for(auto segment : _segments)
    {
        size_t seg_lft = segment.first;
        size_t seg_rgt = segment.first + segment.second.size();

        if(data_lft <= seg_lft && data_rgt >= seg_lft)
        {
            if(data_rgt >= seg_rgt)
            {
                remove_list.push_back(segment.first);
                continue;
            }
            else if(data_rgt < seg_rgt)
            {
                data_cpy = data_cpy.substr(0, seg_lft - data_lft) + segment.second;
                data_rgt = data_lft + data_cpy.size();
                remove_list.push_back(segment.first);
            }
        }

        if(data_lft > seg_lft && data_lft <= seg_rgt)
        {
            if(data_rgt <= seg_rgt)
                cached = false;
            else if(data_rgt > seg_rgt)
            {
                data_cpy = segment.second.substr(0, data_lft - seg_lft) + data_cpy;
                data_lft = seg_lft;
                data_rgt = data_lft + data_cpy.size();
                remove_list.push_back(segment.first);
            }
        }
    }
    
    //Remove overlapping data
    for(auto remove_idx : remove_list) 
        _segments.erase(remove_idx);

    if(cached)
        _segments[data_lft] = data_cpy;
}