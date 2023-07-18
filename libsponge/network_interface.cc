#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    EthernetFrame frame{};
    frame.header().src = _ethernet_address;
    // 不知道目标以太网地址
    if(mapping.find(next_hop_ip) == mapping.end()) {
        _wait_queue.push({dgram, next_hop});
        // 需要发送ARP请求
        if (_request_ip != next_hop_ip or
        (_request_ip == next_hop_ip and _time_wait_for_reply > 5000ul)) {
            _request_ip = next_hop_ip;
            _time_wait_for_reply = 0ul;
            frame.header().dst = ETHERNET_BROADCAST;
            frame.header().type = EthernetHeader::TYPE_ARP;
            ARPMessage message{};
            message.opcode = ARPMessage::OPCODE_REQUEST;
            message.sender_ethernet_address = _ethernet_address;
            message.sender_ip_address = _ip_address.ipv4_numeric();
            message.target_ip_address = next_hop_ip;
            frame.payload() = message.serialize();
            _frames_out.push(frame);
        }
    }
    else {
        frame.header().dst = mapping.find(next_hop_ip)->second.first;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(not(frame.header().dst == _ethernet_address or frame.header().dst == ETHERNET_BROADCAST)) {
        return nullopt;
    }
    // 收到ip报文
    if(frame.header().type == EthernetHeader::TYPE_IPv4) {
        IPv4Datagram datagram{};
        if(datagram.parse(frame.payload()) == ParseResult::NoError) {
            return datagram;
        }
    }
    // 收到arp报文
    else {
        ARPMessage message{};
        if(message.parse(frame.payload()) == ParseResult::NoError) {
            if(message.opcode == ARPMessage::OPCODE_REQUEST) {
                mapping.insert({message.sender_ip_address, Addr_Time{message.sender_ethernet_address, 30000ul}});
                // 需要发送ARP回复
                if(message.target_ip_address == _ip_address.ipv4_numeric()) {
                    ARPMessage reply{};
                    reply.opcode = ARPMessage::OPCODE_REPLY;
                    reply.sender_ethernet_address = _ethernet_address;
                    reply.sender_ip_address = _ip_address.ipv4_numeric();
                    reply.target_ethernet_address = message.sender_ethernet_address;
                    reply.target_ip_address = message.sender_ip_address;
                    EthernetFrame eth_reply{};
                    eth_reply.header().src = _ethernet_address;
                    eth_reply.header().dst = message.sender_ethernet_address;
                    eth_reply.header().type = EthernetHeader::TYPE_ARP;
                    eth_reply.payload() = reply.serialize();
                    _frames_out.push(eth_reply);
                }
            }
            else {
                mapping.insert({message.sender_ip_address, Addr_Time{message.sender_ethernet_address, 30000ul}});
            }
            while(not _wait_queue.empty()) {
                auto first = _wait_queue.front();
                if(mapping.find(first.second.ipv4_numeric()) != mapping.end()) {
                    _wait_queue.pop();
                    EthernetFrame ip_frame{};
                    ip_frame.header().src = _ethernet_address;
                    ip_frame.header().dst = mapping.find(first.second.ipv4_numeric())->second.first;
                    ip_frame.header().type = EthernetHeader::TYPE_IPv4;
                    ip_frame.payload() = first.first.serialize();
                    _frames_out.push(ip_frame);
                }
                else {
                    break;
                }
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time_wait_for_reply += ms_since_last_tick;
    for(auto it = mapping.begin();it != mapping.end();) {
        if(it->second.second > ms_since_last_tick) {
            it->second.second -= ms_since_last_tick;
            ++it;
        }
        else {
            // 只能在这里it++（或it = mapping.erase(it)），而不能写在循环条建里
            mapping.erase(it++);
        }
    }
}
