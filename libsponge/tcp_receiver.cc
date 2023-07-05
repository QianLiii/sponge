#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    const Buffer &payload = seg.payload();
    const string &payload_stuff = payload.copy();
    WrappingInt32 seqno = header.seqno;
    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    if(_ISN == nullopt and header.syn) {
        // 如果这个segment是包含syn的，负载的seqno实际上要+1
        _ISN = optional<WrappingInt32>(seqno);
        seqno = seqno + 1u;
    }
    if(_ISN.has_value()) {
        // ATTENTION: unwrap了seqno之后是absolute seqno，还要再-1才是index
        _reassembler.push_substring(payload_stuff, unwrap(seqno, _ISN.value(), checkpoint) - 1, header.fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(_ISN.has_value()) {
        size_t _eos = _reassembler.stream_out().input_ended() ? 1 : 0;
        return wrap(_reassembler.stream_out().bytes_written()+1+_eos, _ISN.value());
    }
    else {
        return nullopt;
    }
}
