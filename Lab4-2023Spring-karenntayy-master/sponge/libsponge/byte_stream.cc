#include "byte_stream.hh"
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity): //{ DUMMY_CODE(capacity); }
    _data({}),
    is_end(false),
    _begin(0),
    _end(capacity),
    write_cnt(0),
    read_cnt(0),
    _capacity(capacity)

    {
        _data.reserve(capacity);
    }

size_t ByteStream::write(const string &data) {
    //DUMMY_CODE(data);
    //获取数据大小
    int write_size = min(data.size(), remaining_capacity());
    for(int i = _begin, j = 0; i < _begin + write_size; i++, j++) 
    {
        int real_index = i % _capacity;
        _data[real_index] = data[j];   
    }
    _begin += write_size;
    write_cnt += write_size;
    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    //DUMMY_CODE(len);
    string peek_res;
    int peek_size = min(buffer_size(), len);
    for(int i = _end; i < _end + peek_size; i++) 
    {
        int real_index = i % _capacity;
        peek_res += _data[real_index];
    }
    return peek_res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    //DUMMY_CODE(len);
    int pop_size = min(buffer_size(), len);
    _end += pop_size;
    read_cnt += pop_size; 
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    //DUMMY_CODE(len);
    string read_res;
    int read_size = min(buffer_size(), len);
    for(int i = _end; i < _end + read_size; i++) 
    {
        int real_index = i % _capacity;
        read_res += _data[real_index];  
    }
    read_cnt += read_size;
    _end += read_size;
    return read_res;
}

void ByteStream::end_input() { is_end = true; }

bool ByteStream::input_ended() const { return is_end; }

size_t ByteStream::buffer_size() const { return _begin - (_end - _capacity); }

bool ByteStream::buffer_empty() const { 
    //cout << "lll" << endl;
    return buffer_size() == 0;
}

bool ByteStream::eof() const { return is_end && buffer_empty(); }

size_t ByteStream::bytes_written() const { 
    //cout << "lll" << endl;
    return write_cnt; 
}

size_t ByteStream::bytes_read() const { return read_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }