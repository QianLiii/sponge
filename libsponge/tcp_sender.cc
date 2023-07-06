#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>


using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _RTO(retx_timeout) {}

void TCPSender::fill_window() {
    if (not _stream.eof()) {
        // 发出SYN
        if (_next_seqno == 0) {
            TCPSegment first_seg;
            first_seg.header().seqno = wrap(_next_seqno, _isn);
            first_seg.header().syn = true;
            _segments_out.push(first_seg);
            _retx_queue.push(first_seg);
            ++_next_seqno;
            ++_bytes_in_flight;
            if (not _timer.is_start()) {
                _timer.start(_alive_time, _RTO);
            }
        }
        TCPSegment segment;
        while (not _stream.buffer_empty()) {
            segment.header().seqno = wrap(_next_seqno, _isn);
            size_t length =
                min({_stream.buffer_size(), _newest_ackno + _window_size - _next_seqno, TCPConfig::MAX_PAYLOAD_SIZE});
            segment.payload() = _stream.read(length);
            _segments_out.push(segment);
            _retx_queue.push(segment);
            _next_seqno += segment.length_in_sequence_space();
            _bytes_in_flight += segment.length_in_sequence_space();
            if (not _timer.is_start()) {
                _timer.start(_alive_time, _RTO);
            }
        }
    }
    // 条件用来判断有没有发送过FIN
    else if(_next_seqno < _stream.bytes_written() + 2) {
        TCPSegment last_seg;
        last_seg.header().seqno = wrap(_next_seqno, _isn);
        last_seg.header().fin = true;
        _segments_out.push(last_seg);
        _retx_queue.push(last_seg);
        ++_next_seqno;
        ++_bytes_in_flight;
        if(not _timer.is_start()) {
            _timer.start(_alive_time, _RTO);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _newest_ackno);
    if(abs_ackno > _newest_ackno and  abs_ackno <= _next_seqno) {
        // 更新窗口
        _newest_ackno = abs_ackno;
        _window_size = window_size;
        // 从重传队列中移除已被确认的
        while(not _retx_queue.empty()) {
            auto first = _retx_queue.front();
            if(unwrap(first.header().seqno, _isn, _newest_ackno) + first.length_in_sequence_space() <= abs_ackno) {
                _bytes_in_flight -= first.length_in_sequence_space();
                _retx_queue.pop();
            }
            else {
                break;
            }
        }
        // 重置RTO
        _RTO = _initial_retransmission_timeout;
        // 重启重传计时器
        if(not _retx_queue.empty()) {
            _timer.start(_alive_time, _RTO);
        }
        // 没有需要重传的就关闭
        else {
            _timer.stop();
        }
        // 重置连续重传次数
        _consecutive_retxs = 0;
        // 填充窗口
        fill_window();
    }
    // do nothing if the ackno isn't up-to-date
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 更新时间
    _alive_time += ms_since_last_tick;
    // 计时器超时
    if(_timer.is_start() and _timer.expired(_alive_time)) {
        _segments_out.push(_retx_queue.front());
        if(_window_size != 0) {
            ++_consecutive_retxs;
            _RTO *= 2;
        }
        _timer.start(_alive_time, _RTO);
    }
}

// this function interests in none of the fields in a segment but seqno
void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
    // don't need retransmission
}

void RetxTimer::start(size_t current_time, uint32_t RTO) {
    _started = true;
    _start_time = current_time;
    _RTO = RTO;
}