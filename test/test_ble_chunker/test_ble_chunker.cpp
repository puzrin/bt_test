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
        BleChunkHead head(messageId, sequenceNumber, flags);

        BleChunk chunk;
        chunk.resize(BleChunkHead::SIZE);
        head.fillTo(chunk);
        chunk.insert(chunk.end(), data.begin(), data.end());
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

    BleChunkHead errorHead(lastResponseChunks[0]);
    EXPECT_EQ(BleChunkHead::SIZE_OVERFLOW_FLAG | BleChunkHead::FINAL_CHUNK_FLAG, errorHead.flags);
}

TEST_F(BleChunkerTest, MissedChunks) {
    BleChunk chunk1 = createChunk(1, 0, 0, {'A', 'B', 'C'});
    BleChunk chunk2 = createChunk(1, 2, BleChunkHead::FINAL_CHUNK_FLAG, {'D', 'E', 'F'});

    chunker->consumeChunk(chunk1);
    chunker->consumeChunk(chunk2);

    EXPECT_FALSE(onMessageCalled);
    EXPECT_TRUE(onRespondCalled);
    EXPECT_EQ(static_cast<size_t>(1), lastResponseChunks.size());

    BleChunkHead errorHead(lastResponseChunks[0]);
    EXPECT_EQ(BleChunkHead::MISSED_CHUNKS_FLAG | BleChunkHead::FINAL_CHUNK_FLAG, errorHead.flags);
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

TEST_F(BleChunkerTest, TooShortChunk) {
    BleChunk chunk1 = createChunk(1, 0, 0, {'A', 'B', 'C'}); // Valid chunk
    BleChunk shortChunk = {0x01, 0x00}; // Too short chunk
    BleChunk chunk2 = createChunk(1, 1, BleChunkHead::FINAL_CHUNK_FLAG, {'D', 'E', 'F'}); // Valid chunk

    chunker->consumeChunk(chunk1);    // First valid chunk
    chunker->consumeChunk(shortChunk); // Short, invalid chunk (should be ignored)
    chunker->consumeChunk(chunk2);    // Second valid chunk

    EXPECT_TRUE(onMessageCalled);
    EXPECT_EQ(static_cast<size_t>(6), lastReceivedMessage.size());
    EXPECT_EQ(std::vector<uint8_t>({'A', 'B', 'C', 'D', 'E', 'F'}), lastReceivedMessage);
    EXPECT_TRUE(onRespondCalled);
    EXPECT_EQ(static_cast<size_t>(1), lastResponseChunks.size());
}

TEST_F(BleChunkerTest, ZeroLengthMessageResponse) {
    chunker->onMessage = [](const BleMessage&) { return BleMessage(); };

    BleChunk chunk1 = createChunk(1, 0, BleChunkHead::FINAL_CHUNK_FLAG, {'A', 'B', 'C'});
    chunker->consumeChunk(chunk1);

    EXPECT_TRUE(onRespondCalled);
    EXPECT_EQ(static_cast<size_t>(1), lastResponseChunks.size());

    BleChunkHead responseHead(lastResponseChunks[0]);
    EXPECT_EQ(BleChunkHead::FINAL_CHUNK_FLAG, responseHead.flags);
    EXPECT_EQ(BleChunkHead::SIZE, lastResponseChunks[0].size()); // Only the header, no data
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
