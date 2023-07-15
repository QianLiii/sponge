#include "tcp_connection.hh"

#include <iostream>

using namespace std;

void TCPConnection::_pop_and_send() {
    while (not _sender.segments_out().empty()) {
        TCPSegment &&first = std::move(_sender.segments_out().front());
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            first.header().ack = true;
            first.header().ackno = _receiver.ackno().value();
        }
        first.header().win = _receiver.window_size();
        _segments_out.push(first);
    }
    // cerr<<"segment sent with : A = "<<first.header().ack
    // <<"\tR = "<<first.header().rst<<"\tS = "<<first.header().syn
    // <<"\tF = "<<first.header().fin<<"\nseqno = "<<first.header().seqno
    // <<"\tackno = "<<first.header().ackno<<"\npayload size = "
    // <<first.payload().size()<<endl;
}

void TCPConnection::_send_rst() {
    if (_sender.stream_in().buffer_empty()) {
        _sender.send_empty_segment();
    } else {
        while (not _sender.stream_in().buffer_empty()) {
            _sender.fill_window();
        }
    }
    _sender.segments_out().back().header().rst = true;
    _pop_and_send();
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 非active状态下，什么都不做
    if (active()) {
        // cerr<<"segment received with : A = "<<seg.header().ack
        // <<"\tR = "<<seg.header().rst<<"\tS = "<<seg.header().syn
        // <<"\tF = "<<seg.header().fin<<"\nseqno = "<<seg.header().seqno
        // <<"\tackno = "<<seg.header().ackno<<"\npayload size = "
        // <<seg.payload().size()<<endl;

        _last_seg_received_time = _alive_time;
        auto &header = seg.header();
        _newest_received_ackno = header.ackno;
        // 如果收到RST包，进入unclean shutdown（只需要把sender和receiver设置成error状态）
        if (header.rst) {
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
        }
        // 收到的是非RST包
        else {
            // 收到的segment递交给receiver处理（拼装，完成后关闭流）
            _receiver.segment_received(seg);
            // 如果输入流先于输出流结束，设置不需要linger
            if (_receiver.stream_out().input_ended() and not _sender.stream_in().eof()) {
                _linger_after_streams_finish = false;
            }
        }
        // 如果包含ACK，收到的确认号和窗口递交给sender
        if (header.ack) {
            _sender.ack_received(header.ackno, header.win);
        }
        // 收到SYN时调用fill_window：
        // 如果是对方发起连接，fill_window方法会发送一个SYN
        // 如果是自己发起连接，fill_window方法什么都不会做，而会在下面发送确认部分发出第三次握手
        if (header.syn) {
            _sender.fill_window();
        }
        // 需要发出确认
        if (seg.length_in_sequence_space() > 0) {
            if (_sender.segments_out().empty()) {
                _sender.send_empty_segment();
            }
        }
        // 发出keep-alive确认
        else if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
                 header.seqno == _receiver.ackno().value() - 1) {
            _sender.send_empty_segment();
        }
        _pop_and_send();
    }
}

bool TCPConnection::active() const {
    if(_receiver.stream_out().error() and _sender.stream_in().error()) {
        return false;
    }
    // prereq#1～#3
    if(_receiver.stream_out().eof() and _sender.stream_in().eof()
    and (_sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2)
    and (_sender.bytes_in_flight() == 0)) {
        if(_linger_after_streams_finish == false) {
            // not active
            // cerr<<"clean shutdown without linger\n";
            return false;
        }
        else if(_alive_time - _last_seg_received_time >= 10*_cfg.rt_timeout){
            // linger time expired
            // cerr<<"clean shutdown with linger\n";
            return false;
        }
    }
    // cerr<<"still active\n";
    return true;
}

size_t TCPConnection::write(const string &data) {
    size_t bytes_written = _sender.stream_in().write(data);
    if(not _sender.stream_in().buffer_empty()) {
        _sender.fill_window();
        _pop_and_send();
    }
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // cerr<<ms_since_last_tick<<" ms passed\n";
    _alive_time += ms_since_last_tick;
    // 得到更新时间后，先检查是否需要发出RST
    if(_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        _send_rst();
    }
    // 如果连接还可用，通知sender时间
    // 在sender重传了segment后不会立即检查是否达到最大重传次数，而是下一次时间更新时检查
    if(active()) {
       _sender.tick(ms_since_last_tick);
        _pop_and_send();
    }
}

// 主动建立连接，发送SYN
void TCPConnection::connect() {
    _sender.fill_window();
    _pop_and_send();
}

// 关闭输出流，发送FIN
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _pop_and_send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
