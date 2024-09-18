#pragma once

#include <vector>
#include <functional>
#include <cstdint>
#include <algorithm>

#ifndef TEST
#include "logger.hpp"
#else
#define DEBUG(...)
#endif

class BleChunkHead {
public:
    uint8_t messageId;
    uint16_t sequenceNumber;
    uint8_t flags;

    static constexpr uint8_t FINAL_CHUNK_FLAG = 0x01;
    static constexpr uint8_t MISSED_CHUNKS_FLAG = 0x02;
    static constexpr uint8_t SIZE_OVERFLOW_FLAG = 0x04;
    static constexpr size_t SIZE = 4;

    BleChunkHead(uint8_t messageId, uint16_t sequenceNumber, uint8_t flags)
        : messageId(messageId), sequenceNumber(sequenceNumber), flags(flags) {}

    BleChunkHead(const uint8_t* chunk) {
        messageId = chunk[0];
        sequenceNumber = static_cast<uint16_t>(chunk[1] | (chunk[2] << 8));
        flags = chunk[3];
    }

    BleChunkHead(const std::vector<uint8_t>& chunk) : BleChunkHead(chunk.data()) {}

    void fillTo(uint8_t* chunk) const {
        chunk[0] = messageId;
        chunk[1] = static_cast<uint8_t>(sequenceNumber & 0xFF);
        chunk[2] = static_cast<uint8_t>((sequenceNumber >> 8) & 0xFF);
        chunk[3] = flags;
    }

    void fillTo(std::vector<uint8_t>& chunk) const { fillTo(chunk.data()); }
};

class BleChunker {
public:
    // Max BLE MTU size usually 512 or 517 bytes. Set default transfer size a bit less.
    BleChunker(size_t maxChunkSize = 500, size_t maxMessageSize = 65536)
            : maxChunkSize(maxChunkSize), maxMessageSize(maxMessageSize),
            messageSize(0), expectedSequenceNumber(0), firstMessage(true), skipTail(false) {
        assembledMessage.reserve(maxMessageSize);
    }

    void consumeChunk(const std::vector<uint8_t>& chunk) {
        if (chunk.size() < BleChunkHead::SIZE) {
            //DEBUG("BLE Chunker: received chunk is too small, ignoring");
            return;
        }

        BleChunkHead head(chunk);

        if (skipTail && head.messageId == currentMessageId) {
            // Discard chunks until a new message ID is received
            //DEBUG("BLE Chunker: chunk discarded");
            return;
        }

        if (firstMessage || head.messageId != currentMessageId) {
            // New message, discard old data and reset state
            //DEBUG("BLE Chunker: new message (id = {}), reset state to initial", head.messageId);
            currentMessageId = head.messageId;
            resetState();
        }

        size_t newMessageSize = messageSize + (chunk.size() - BleChunkHead::SIZE);

        // Check message size overflow
        if (newMessageSize > maxMessageSize) {
            //DEBUG("BLE Chunker: size overflow");
            skipTail = true;
            sendErrorResponse(BleChunkHead::SIZE_OVERFLOW_FLAG);
            return;
        }

        // Check for missed chunks
        if (head.sequenceNumber != expectedSequenceNumber) {
            //DEBUG("BLE Chunker: bad sequence number, expected {}, got {}", expectedSequenceNumber, head.sequenceNumber);
            skipTail = true;
            sendErrorResponse(BleChunkHead::MISSED_CHUNKS_FLAG);
            return;
        }

        assembledMessage.insert(assembledMessage.end(), chunk.begin() + BleChunkHead::SIZE, chunk.end());

        messageSize = newMessageSize;
        expectedSequenceNumber++;

        if (head.flags & BleChunkHead::FINAL_CHUNK_FLAG) {
            //DEBUG("BLE Chunker: got final chunk");
            // Set skipTail to true to prevent processing further chunks for this message
            skipTail = true;

            // Process the complete message
            if (onMessage) {
                response = splitMessageToChunks(onMessage(assembledMessage));
            }
        }
    }

    std::function<std::vector<uint8_t>(const std::vector<uint8_t>& message)> onMessage;
    std::vector<std::vector<uint8_t>> response;  // Store response chunks here

private:
    size_t maxChunkSize;
    size_t maxMessageSize;
    uint8_t currentMessageId;
    size_t messageSize;
    uint16_t expectedSequenceNumber;
    bool firstMessage;
    bool skipTail;
    std::vector<uint8_t> assembledMessage;

    void resetState() {
        assembledMessage.clear();
        response.clear();
        messageSize = 0;
        expectedSequenceNumber = 0;
        firstMessage = false;
        skipTail = false;
    }

    std::vector<std::vector<uint8_t>> splitMessageToChunks(const std::vector<uint8_t>& message) {
        std::vector<std::vector<uint8_t>> chunks;
        size_t totalSize = message.size();
        size_t chunkSize = maxChunkSize - BleChunkHead::SIZE;

        if (totalSize == 0) {
            // Handle case when message is empty, send only the header with FINAL_CHUNK_FLAG
            std::vector<uint8_t> emptyChunk(BleChunkHead::SIZE);
            BleChunkHead head(currentMessageId, 0, BleChunkHead::FINAL_CHUNK_FLAG);
            head.fillTo(emptyChunk);
            chunks.push_back(emptyChunk);
            return chunks;
        }

        for (size_t i = 0; i < totalSize; i += chunkSize) {
            size_t end = (i + chunkSize > totalSize) ? totalSize : i + chunkSize;

            // Reserve space for the header and data
            std::vector<uint8_t> chunk;
            chunk.reserve(BleChunkHead::SIZE + (end - i));

            // Insert the header
            BleChunkHead head(currentMessageId, i / chunkSize, (end == totalSize) ? BleChunkHead::FINAL_CHUNK_FLAG : 0);
            chunk.resize(BleChunkHead::SIZE);
            head.fillTo(chunk);

            // Insert the data
            chunk.insert(chunk.end(), message.begin() + i, message.begin() + end);

            chunks.push_back(chunk);
        }

        return chunks;
    }

    void sendErrorResponse(uint8_t errorFlag) {
        std::vector<uint8_t> errorChunk(BleChunkHead::SIZE);
        BleChunkHead errorHead(currentMessageId, 0, errorFlag | BleChunkHead::FINAL_CHUNK_FLAG);
        errorHead.fillTo(errorChunk);

        response = {errorChunk};
    }
};
