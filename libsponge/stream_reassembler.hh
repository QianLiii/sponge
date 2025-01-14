#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <map>
#include <vector>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  friend class TCPReceriver;
  private:
    // Your code here -- add private members as necessary.
    std::map<uint64_t, std::string> _container{};
    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    size_t _unassembled_bytes{};
    size_t _first_unassembled{};
    // 当一个push_substring收到了eof，将reassembler设为等待eof状态，并记录下当次请求的data的最后一个字节的index
    // 当所有index都被组装并提交到stream后，才真的eof
    bool _wait_eof{};
    size_t _eof_index{};

    // 将push_substring收到的子串的适当部分合并入_container
    size_t _merge(std::map<uint64_t, std::string>::iterator start_it);
    // 将可提交部分提交给stream
    void _submit_to_stream();
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const {return _unassembled_bytes;}

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const {return _container.empty();}
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
