#include <gtest/gtest.h>
#include "ble_chunker.hpp"

class BleChunkerTest : public ::testing::Test {
protected:
    BleChunker* chunker;

    bool onMessageCalled;
    bool onRespondCalled;
    BleMessage lastReceivedMessage;
    std::vector<BleChunk> lastResponseChunks;

    BleChunkerTest() : onMessageCalled(false), onRespondCalled(false) {}

    BleMessage onMessageHandler(const BleMessage& message) {
        onMessageCalled = true;
        lastReceivedMessage = message;
        BleMessage response = {'O', 'K'};
        return response;
    }

    void onRespondHandler(const std::vector<BleChunk>& chunks) {
        onRespondCalled = true;
        lastResponseChunks = chunks;
    }

    virtual void SetUp() override {
        chunker = new BleChunker(512);

        chunker->onMessage = [this](const BleMessage& message) { return onMessageHandler(message); };
        chunker->onRespond = [this](const std::vector<BleChunk>& chunks) { onRespondHandler(chunks); };

        onMessageCalled = false;
        onRespondCalled = false;
        lastReceivedMessage.clear();
        lastResponseChunks.clear();
    }

    virtual void TearDown() override {
        delete chunker;
    }

    BleChunk createChunk(uint8_t messageId, uint16_t sequenceNumber, uint8_t flags, const std::vector<uint8_t>& data) {
        BleChunkHead head;
        head.messageId = messageId;
        head.sequenceNumber = sequenceNumber;
        head.flags = flags;

        BleChunk chunk = data;
        chunk.insert(chunk.begin(), reinterpret_cast<uint8_t*>(&head), reinterpret_cast<uint8_t*>(&head) + sizeof(head));
        return chunk;
    }
};

TEST_F(BleChunkerTest, ChunkAssembly) {
    BleChunk chunk1 = createChunk(1, 0, 0, {'A', 'B', 'C'});
    BleChunk chunk2 = createChunk(1, 1, BleChunkHead::FINAL_CHUNK_FLAG, {'D', 'E', 'F'});

    chunker->consumeChunk(chunk1);
    chunker->consumeChunk(chunk2);

    EXPECT_TRUE(onMessageCalled);
    EXPECT_EQ(static_cast<size_t>(6), lastReceivedMessage.size());
    EXPECT_EQ(std::vector<uint8_t>({'A', 'B', 'C', 'D', 'E', 'F'}), lastReceivedMessage);
    EXPECT_TRUE(onRespondCalled);
    EXPECT_EQ(static_cast<size_t>(1), lastResponseChunks.size());
}

TEST_F(BleChunkerTest, MessageSizeOverflow) {
    BleChunk largeChunk = createChunk(1, 0, 0, std::vector<uint8_t>(600, 0));

    chunker->consumeChunk(largeChunk);

    EXPECT_FALSE(onMessageCalled);
    EXPECT_TRUE(onRespondCalled);
    EXPECT_EQ(static_cast<size_t>(1), lastResponseChunks.size());
    if (!lastResponseChunks.empty()) {
        BleChunkHead errorHead;
        std::copy(lastResponseChunks[0].begin(), lastResponseChunks[0].begin() + sizeof(errorHead), reinterpret_cast<uint8_t*>(&errorHead));
        EXPECT_EQ(BleChunkHead::SIZE_OVERFLOW_FLAG | BleChunkHead::FINAL_CHUNK_FLAG, errorHead.flags);
    }
}

TEST_F(BleChunkerTest, MissedChunks) {
    BleChunk chunk1 = createChunk(1, 0, 0, {'A', 'B', 'C'});
    BleChunk chunk2 = createChunk(1, 2, BleChunkHead::FINAL_CHUNK_FLAG, {'D', 'E', 'F'});

    chunker->consumeChunk(chunk1);
    chunker->consumeChunk(chunk2);

    EXPECT_FALSE(onMessageCalled);
    EXPECT_TRUE(onRespondCalled);
    EXPECT_EQ(static_cast<size_t>(1), lastResponseChunks.size());
    if (!lastResponseChunks.empty()) {
        BleChunkHead errorHead;
        std::copy(lastResponseChunks[0].begin(), lastResponseChunks[0].begin() + sizeof(errorHead), reinterpret_cast<uint8_t*>(&errorHead));
        EXPECT_EQ(BleChunkHead::MISSED_CHUNKS_FLAG | BleChunkHead::FINAL_CHUNK_FLAG, errorHead.flags);
    }
}

TEST_F(BleChunkerTest, MultipleMessages) {
    BleChunk chunk1a = createChunk(1, 0, 0, {'M', 'S', 'G'});
    BleChunk chunk1b = createChunk(1, 1, BleChunkHead::FINAL_CHUNK_FLAG, {'1', 'S', 'T'});

    chunker->consumeChunk(chunk1a);
    chunker->consumeChunk(chunk1b);

    EXPECT_TRUE(onMessageCalled);
    EXPECT_EQ(static_cast<size_t>(6), lastReceivedMessage.size());
    EXPECT_EQ(std::vector<uint8_t>({'M', 'S', 'G', '1', 'S', 'T'}), lastReceivedMessage);
    EXPECT_TRUE(onRespondCalled);
    EXPECT_EQ(static_cast<size_t>(1), lastResponseChunks.size());

    onMessageCalled = false;
    onRespondCalled = false;
    lastReceivedMessage.clear();
    lastResponseChunks.clear();

    BleChunk chunk2a = createChunk(2, 0, 0, {'2', 'N', 'D'});
    BleChunk chunk2b = createChunk(2, 1, BleChunkHead::FINAL_CHUNK_FLAG, {'M', 'S', 'G'});

    chunker->consumeChunk(chunk2a);
    chunker->consumeChunk(chunk2b);

    EXPECT_TRUE(onMessageCalled);
    EXPECT_EQ(static_cast<size_t>(6), lastReceivedMessage.size());
    EXPECT_EQ(std::vector<uint8_t>({'2', 'N', 'D', 'M', 'S', 'G'}), lastReceivedMessage);
    EXPECT_TRUE(onRespondCalled);
    EXPECT_EQ(static_cast<size_t>(1), lastResponseChunks.size());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
