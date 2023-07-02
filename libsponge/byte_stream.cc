#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(const size_t capacity) :
_capacity(capacity) {
    
}

size_t ByteStream::write(const string &data) {
    size_t len = remaining_capacity() > data.size()?
        data.size() : remaining_capacity();
    _container.insert(_container.end(), data.begin(), data.begin()+len);
    _bytes_written += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret{};
    ret.append(_container.begin(), _container.begin()+len);
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    _container.erase(_container.begin(), _container.begin()+len);
    _bytes_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string &&ret = peek_output(len);
    pop_output(len);
    return ret;
}

