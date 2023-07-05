#include "wrapping_integers.hh"
// #include <fstream>

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // ((n mod 2^32) + isn) mod 2^32
    return WrappingInt32{static_cast<uint32_t>((n & 0xffff'ffff) + isn.raw_value()) & 0xffff'ffff};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // ofstream log("log");
    // log<<"n:\t"<<n<<"\nisn:\t"<<isn<<"\ncp:\t"<<checkpoint<<endl;
    // 32位带符号转为32位无符号再转为64位无符号
    uint64_t ballpark = static_cast<uint64_t>(static_cast<uint32_t>(n - isn));
    // log<<"initial ballpark:\t"<<ballpark<<endl;
    // log<<"2^32:\t"<<0x1'0000'0000<<endl;
    uint64_t offset;
    if(ballpark < checkpoint) {
        offset = checkpoint - ballpark;
        while(offset > 0x1'0000'0000u) {
            ballpark += 0x1'0000'0000u;
            offset -= 0x1'0000'0000u;
        }
        if(ballpark <= UINT64_MAX - 0x1'0000'0000u and ballpark + 0x1'0000'0000u - checkpoint < offset) {
            ballpark += 0x1'0000'0000u;
        }
    }
    else {
        offset = ballpark - checkpoint;
        while(offset > 0x1'0000'0000u) {
            ballpark -= 0x1'0000'0000u;
            offset -= 0x1'0000'0000u;
        }
        if(ballpark >= 0x1'0000'0000u and checkpoint - ballpark + 0x1'0000'0000u < offset) {
            ballpark -= 0x1'0000'0000u;
        }
    }
    return ballpark;
}
