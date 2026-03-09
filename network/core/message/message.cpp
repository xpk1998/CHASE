/**
 * @file message.cpp
 * @brief Network message implementation for Parachain
 * @author Parachain Team
 * @date 2025
 */

#include "message.h"
#include <cstring>
#include <stdexcept>

namespace parachain {
namespace network {

bool Message::encode(std::vector<uint8_t>& buffer) const {
    try {
        // Calculate total size needed
        size_t totalSize = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t);
        
        // Add UUID size if present
        if (m_uuid) {
            totalSize += sizeof(uint32_t) + m_uuid->size();
        } else {
            totalSize += sizeof(uint32_t); // Size field for empty UUID
        }
        
        // Add payload size
        totalSize += sizeof(uint32_t) + m_payload.size();
        
        // Resize buffer
        buffer.resize(totalSize);
        
        size_t offset = 0;
        
        // Encode moduleID
        std::memcpy(buffer.data() + offset, &m_moduleID, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        
        // Encode ext
        std::memcpy(buffer.data() + offset, &m_ext, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        
        // Encode seq
        std::memcpy(buffer.data() + offset, &m_seq, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Encode UUID
        uint32_t uuidSize = m_uuid ? static_cast<uint32_t>(m_uuid->size()) : 0;
        std::memcpy(buffer.data() + offset, &uuidSize, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (m_uuid && uuidSize > 0) {
            std::memcpy(buffer.data() + offset, m_uuid->data(), uuidSize);
            offset += uuidSize;
        }
        
        // Encode payload
        uint32_t payloadSize = static_cast<uint32_t>(m_payload.size());
        std::memcpy(buffer.data() + offset, &payloadSize, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (payloadSize > 0) {
            std::memcpy(buffer.data() + offset, m_payload.data(), payloadSize);
            offset += payloadSize;
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

ssize_t Message::decode(const std::vector<uint8_t>& buffer) {
    try {
        if (buffer.size() < sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t)) {
            return -1; // Buffer too small
        }
        
        size_t offset = 0;
        
        // Decode moduleID
        std::memcpy(&m_moduleID, buffer.data() + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        
        // Decode ext
        std::memcpy(&m_ext, buffer.data() + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        
        // Decode seq
        std::memcpy(&m_seq, buffer.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        // Decode UUID
        uint32_t uuidSize = 0;
        std::memcpy(&uuidSize, buffer.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (offset + uuidSize > buffer.size()) {
            return -1; // Invalid UUID size
        }
        
        if (uuidSize > 0) {
            m_uuid = std::make_shared<std::vector<uint8_t>>(uuidSize);
            std::memcpy(m_uuid->data(), buffer.data() + offset, uuidSize);
            offset += uuidSize;
        } else {
            m_uuid.reset();
        }
        
        // Decode payload
        uint32_t payloadSize = 0;
        std::memcpy(&payloadSize, buffer.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (offset + payloadSize > buffer.size()) {
            return -1; // Invalid payload size
        }
        
        if (payloadSize > 0) {
            m_payload.resize(payloadSize);
            std::memcpy(m_payload.data(), buffer.data() + offset, payloadSize);
            offset += payloadSize;
        } else {
            m_payload.clear();
        }
        
        return static_cast<ssize_t>(offset);
    } catch (const std::exception& e) {
        return -1;
    }
}

uint16_t Message::tryDecodeModuleID(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < sizeof(uint16_t)) {
        return 0;
    }
    
    uint16_t moduleID = 0;
    std::memcpy(&moduleID, buffer.data(), sizeof(uint16_t));
    return moduleID;
}

} // namespace network
} // namespace parachain