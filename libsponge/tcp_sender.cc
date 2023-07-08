#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// #include <iostream>

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _RTO(retx_timeout) {
        // cerr<<"function called: constructor\n"
        // <<"init_rto: "<<retx_timeout<<endl;
    }

void TCPSender::fill_window() {
    // cerr<<"function called: fill_window\n";

    // 发出SYN：唯一条件
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

        // cerr << "send segment: abs_seqno = 0, FLAG = S\n";

    }

    TCPSegment segment;
    // 可以发送数据的条件：1）SYN被确认（_newest_ackno > 0）
    // 2）窗口有空余（_newest_ackno + max(_window_size, static_cast<uint16_t>(1u)) > _next_seqno）画图就明白，还处理了通告窗口为0的情况
    // 3）未发送过FIN（_next_seqno != _stream.bytes_written() + 2）
    while (_newest_ackno > 0 and _newest_ackno + max(_window_size, static_cast<uint16_t>(1u)) > _next_seqno and _next_seqno != _stream.bytes_written() + 2) {
        // char fool;
        // cin>>fool;
        // 一开始各种陷入死循环就是这里理解错了，lab实现的是单线程的，如果没有要发的数据就应该直接退出，而不是在while里等待
        if(_stream.buffer_empty() and not _stream.eof()) {
            // cerr<<"nothing to be sent\n";
            break;
        }

        segment.header().seqno = wrap(_next_seqno, _isn);
        // 可用窗口大小，至少是1
        size_t available_win = _newest_ackno + max(_window_size, static_cast<uint16_t>(1u)) - _next_seqno;
        // 实际传输的长度
        size_t length = min({_stream.buffer_size(), available_win, TCPConfig::MAX_PAYLOAD_SIZE});
        segment.payload() = _stream.read(length);
        // 加上FIN标记的条件：
        // 1）流结束（_stream.eof()）
        // 2）还没发送过FIN（_next_seqno < _stream.bytes_written() + 2）
        // 3）可用窗口中放得下（length < available_win）
        if(_stream.eof() and _next_seqno < _stream.bytes_written() + 2 and length < available_win) {
            segment.header().fin = true;
        }
        _segments_out.push(segment);
        _retx_queue.push(segment);
        _next_seqno += segment.length_in_sequence_space();
        _bytes_in_flight += segment.length_in_sequence_space();

        // cerr << "send segment: asb_seqno = " << unwrap(segment.header().seqno, _isn, _newest_ackno)
            //  << "\nlength = " << length << "\nnext abs_seqno = " << _next_seqno <<"\tFIN = "<<segment.header().fin<< endl;
        if (not _timer.is_start()) {
            _timer.start(_alive_time, _RTO);
            // cerr << "timer on\n";
        }

    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _newest_ackno);

    // cerr<<"function called: ack_received\n"
    // <<"abs_ackno: "<<abs_ackno<<endl;

    // 根据测试用例知道，总是会更新窗口大小，不管确认号是否有意义
    _window_size = window_size;
    if(abs_ackno > _newest_ackno and  abs_ackno <= _next_seqno) {
            // cerr<<"update winsize: "<<window_size<<endl;
        // 更新最新的确认号
        _newest_ackno = abs_ackno;
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

        // cerr<<"calls fill_window\n";
        fill_window();
    }
    // do nothing if the ackno isn't up-to-date
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // cerr<<"function called: tick\n"<<ms_since_last_tick<<" ms passed\n";
    // 更新时间
    _alive_time += ms_since_last_tick;
    // 计时器超时
    if(_timer.is_start() and _timer.expired(_alive_time)) {
        _segments_out.push(_retx_queue.front());

        // cerr<<"retransmit segment: abs_seqno = "<<unwrap(_retx_queue.front().header().seqno, _isn, _newest_ackno)<<endl;

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