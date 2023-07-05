#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(data.size() == 0) {
        // do nothing
    }
    else {
        size_t _first_unacceptable = _first_unassembled + _capacity - _output.buffer_size();
        // 丢弃index超出限制的子串
        if (index >= _first_unacceptable) {
            // do nothing
        }
        // data的全部已被提交，丢弃
        else if (index + data.size() <= _first_unassembled) {
            // do nothing
        }
        // 先暂存入_container，然后执行合并
        else {
            // 暂存部分，不会超过_first_unassembled和_first_unacceptable这个区间
            size_t start_index = _first_unassembled > index ? _first_unassembled : index;
            size_t end_index = index + data.size() > _first_unacceptable ? _first_unacceptable : index + data.size();
            size_t _added_bytes = end_index - start_index;
            bool _discard{};
            auto temp = make_pair(start_index, data.substr(start_index - index, end_index - start_index));
            // 如果有首字节重合的，选择长度长的留下
            if (_container.find(start_index) != _container.end()) {
                // 原先的长，直接丢弃新的
                if (_container.find(start_index)->second.size() >= temp.second.size()) {
                    _discard = true;
                }
                // 新的长
                else {
                    _added_bytes -= _container.find(start_index)->second.size();
                    _container.find(start_index)->second = temp.second;
                }
            }
            // 首字节不重合就可以直接暂存
            else {
                _container.insert(temp);
            }
            if (not _discard) {
                // 开始检查是否合并，要么从暂存子串开始，要么从它的前一个开始
                auto start_it = _container.find(start_index) == _container.begin() ? _container.begin()
                                                                                   : --_container.find(start_index);
                _added_bytes -= _merge(start_it);
                // 更新未提交的字符数
                _unassembled_bytes += _added_bytes;
                // 提交（可能成功，也可能什么都不做），同时会更新_unassembled_bytes和_first_unassembled
                _submit_to_stream();
            }
        }
    }
    
    // 记录流的最后一个字符的索引
    if(eof){
        _wait_eof = true;
        _eof_index = index + data.size();
    }
    // 在确实提交了所有字符后关闭输入
    if(_wait_eof and _first_unassembled == _eof_index) {
        _output.end_input();
    }
}

// 以合并的方式将暂存的子串不重叠的部分存入_container，方法参考算法题：区间合并
// 返回重叠的字符数量
// 有一个前提是所以已经存在的子串都是相互独立的（不能合并）
// 因此所有合并只会在和暂存的子串相关的地方发生，所以一旦一次（可以是多个子串）合并结束，就不用再向后寻找了
size_t StreamReassembler::_merge(std::map<uint64_t, std::string>::iterator start_it) {
    size_t overlapping_bytes{};
    bool merged = false;
    auto next_it = start_it;
    ++next_it;
    while(next_it != _container.end()){
        // 如果start_it的右边界大于等于next_it的左边界，就认为可以合并
        if(start_it->first + start_it->second.size() >= next_it->first) {
            merged = true;
            // 如果start_it的右边界大于等于next_it的右边界，next_it就可以整个删除
            if(start_it->first + start_it->second.size() >= next_it->first + next_it->second.size()) {
                overlapping_bytes += next_it->second.size();
                _container.erase(next_it++);
            }
            // 如若不然，将next_it不重叠的部分追加到start_it后并删除next_it
            else {
                size_t overlaps = start_it->first + start_it->second.size() - next_it->first;
                overlapping_bytes += overlaps;
                start_it->second.append(next_it->second.substr(overlaps));
                _container.erase(next_it++);
            }
        }
        else if(not merged) {
            ++start_it;
            ++next_it;
        }
        else {
            break;
        }
    }
    return overlapping_bytes;
}

// 每次肯定只能提交一个子串，而且必须是index为_first_unassembled的
void StreamReassembler::_submit_to_stream() {
    auto it = _container.cbegin();
    if(it->first == _first_unassembled){
        size_t write_bytes = _output.write(it->second);
        _unassembled_bytes -= write_bytes;
        _first_unassembled += write_bytes;
        _container.erase(it);
    }
}