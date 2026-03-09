/**
 * @file message.h
 * @brief Network message implementation for Parachain
 * @author Parachain Team
 * @date 2025
 */

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace parachain {
namespace network {

// Message type definitions
enum MessageType : uint16_t {
    Heartbeat = 0x1,
    Handshake = 0x2,
    RequestNodeStatus = 0x3,
    ResponseNodeStatus = 0x4,
    PeerToPeerMessage = 0x5,
    BroadcastMessage = 0x6,
    AMOPMessageType = 0x7,
    WSMessageType = 0x8,
    SyncNodeSeq = 0x9,
    RouterTableSyncSeq = 0xa,
    RouterTableResponse = 0xb,
    RouterTableRequest = 0xc,
    ForwardMessage = 0xd,
};

class Message {
public:
    using Ptr = std::shared_ptr<Message>;
    
    Message() = default;
    virtual ~Message() = default;
    
    // Getters and setters
    virtual uint16_t moduleID() const { return m_moduleID; }
    virtual void setModuleID(uint16_t moduleID) { m_moduleID = moduleID; }
    
    virtual uint16_t ext() const { return m_ext; }
    virtual void setExt(uint16_t ext) { m_ext = ext; }
    
    virtual uint32_t seq() const { return m_seq; }
    virtual void setSeq(uint32_t seq) { m_seq = seq; }
    
    virtual std::shared_ptr<std::vector<uint8_t>> uuid() const { return m_uuid; }
    virtual void setUuid(std::shared_ptr<std::vector<uint8_t>> uuid) { m_uuid = uuid; }
    
    virtual const std::vector<uint8_t>& payload() const { return m_payload; }
    virtual void setPayload(const std::vector<uint8_t>& payload) { m_payload = payload; }
    
    virtual void setResponse() { m_ext |= 0x0001; }
    virtual bool isResponse() const { return m_ext & 0x0001; }
    
    // Serialization methods
    virtual bool encode(std::vector<uint8_t>& buffer) const;
    virtual ssize_t decode(const std::vector<uint8_t>& buffer);
    
    static uint16_t tryDecodeModuleID(const std::vector<uint8_t>& buffer);

protected:
    uint16_t m_moduleID = 0;
    uint16_t m_ext = 0;
    uint32_t m_seq = 0;
    std::shared_ptr<std::vector<uint8_t>> m_uuid;
    std::vector<uint8_t> m_payload;
};

class MessageFactory {
public:
    using Ptr = std::shared_ptr<MessageFactory>;
    
    virtual ~MessageFactory() = default;
    
    virtual Message::Ptr buildMessage() {
        auto message = std::make_shared<Message>();
        return message;
    }
};

} // namespace network
} // namespace parachain