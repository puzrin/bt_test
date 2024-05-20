#pragma once

#include <vector>
#include <functional>
#include <cstdint>
#include <algorithm>

struct BleChunkHead {
    uint8_t messageId;
    uint16_t sequenceNumber;
    uint8_t flags;

    static constexpr uint8_t FINAL_CHUNK_FLAG = 0x01;
    static constexpr uint8_t MISSED_CHUNKS_FLAG = 0x02;
    static constexpr uint8_t SIZE_OVERFLOW_FLAG = 0x04;
};

using BleChunk = std::vector<uint8_t>;
using BleMessage = std::vector<uint8_t>;

class BleChunker {
public:
    BleChunker(size_t maxBlobSize)
        : maxBlobSize(maxBlobSize), messageSize(0), expectedSequenceNumber(0), firstMessage(true), skipTail(false) {}

    void consumeChunk(const BleChunk& chunk) {
        BleChunkHead head;
        std::copy(chunk.begin(), chunk.begin() + sizeof(BleChunkHead), reinterpret_cast<uint8_t*>(&head));

        if (skipTail && head.messageId == currentMessageId) {
            // Discard chunks until a new message ID is received
            return;
        }

        if (firstMessage || head.messageId != currentMessageId) {
            // New message, discard old data and reset state
            currentMessageId = head.messageId;
            resetState();
        }

        size_t newMessageSize = messageSize + (chunk.size() - sizeof(BleChunkHead));

        // Check for size overflow
        if (newMessageSize > maxBlobSize) {
            skipTail = true;
            sendErrorResponse(BleChunkHead::SIZE_OVERFLOW_FLAG);
            return;
        }

        // Check for missed chunks
        if (head.sequenceNumber != expectedSequenceNumber) {
            skipTail = true;
            sendErrorResponse(BleChunkHead::MISSED_CHUNKS_FLAG);
            return;
        }

        inputChunks.push_back(chunk);
        messageSize = newMessageSize;
        expectedSequenceNumber++;

        if (head.flags & BleChunkHead::FINAL_CHUNK_FLAG) {
            // Set skipTail to true to prevent processing further chunks for this message
            skipTail = true;

            // Assemble the complete message
            BleMessage assembledMessage;
            for (const auto& chunk : inputChunks) {
                assembledMessage.insert(assembledMessage.end(), chunk.begin() + sizeof(BleChunkHead), chunk.end());
            }

            // Process the complete message
            if (onMessage) {
                BleMessage response = onMessage(assembledMessage);
                std::vector<BleChunk> responseChunks = splitMessageToChunks(response);
                if (onRespond) onRespond(responseChunks);
            }
        }
    }

    std::function<BleMessage(const BleMessage& message)> onMessage;
    std::function<void(const std::vector<BleChunk>& chunks)> onRespond;

private:
    size_t maxBlobSize;
    uint8_t currentMessageId;
    size_t messageSize;
    uint16_t expectedSequenceNumber;
    bool firstMessage;
    bool skipTail;
    std::vector<BleChunk> inputChunks;

    void resetState() {
        inputChunks.clear();
        messageSize = 0;
        expectedSequenceNumber = 0;
        firstMessage = false;
        skipTail = false;
    }

    std::vector<BleChunk> splitMessageToChunks(const BleMessage& message) {
        std::vector<BleChunk> chunks;
        size_t totalSize = message.size();
        size_t chunkSize = maxBlobSize - sizeof(BleChunkHead);

        for (size_t i = 0; i < totalSize; i += chunkSize) {
            BleChunk chunk;
            size_t end = (i + chunkSize > totalSize) ? totalSize : i + chunkSize;
            chunk.insert(chunk.end(), message.begin() + i, message.begin() + end);

            insertHeader(chunk, currentMessageId, i / chunkSize, (end == totalSize) ? BleChunkHead::FINAL_CHUNK_FLAG : 0);

            chunks.push_back(chunk);
        }

        return chunks;
    }

    void insertHeader(BleChunk& chunk, uint8_t messageId, uint16_t sequenceNumber, uint8_t flags) {
        BleChunkHead head;
        head.messageId = messageId;
        head.sequenceNumber = sequenceNumber;
        head.flags = flags;

        chunk.insert(chunk.begin(), reinterpret_cast<uint8_t*>(&head), reinterpret_cast<uint8_t*>(&head) + sizeof(head));
    }

    void sendErrorResponse(uint8_t errorFlag) {
        BleMessage errorMessage;
        std::vector<BleChunk> errorChunks;

        BleChunk errorChunk;
        insertHeader(errorChunk, currentMessageId, 0, errorFlag | BleChunkHead::FINAL_CHUNK_FLAG);
        errorChunks.push_back(errorChunk);

        if (onRespond) onRespond(errorChunks);
    }
};
