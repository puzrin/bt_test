#include <unity.h>
#include "ble_chunker.hpp"

BleChunker* chunker;

bool onMessageCalled = false;
bool onRespondCalled = false;
BleMessage lastReceivedMessage;
std::vector<BleChunk> lastResponseChunks;

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

void setUp(void) {
    // Create a new instance of BleChunker for each test
    chunker = new BleChunker(512);

    // Set up the handlers
    chunker->onMessage = onMessageHandler;
    chunker->onRespond = onRespondHandler;

    // Reset flags
    onMessageCalled = false;
    onRespondCalled = false;
    lastReceivedMessage.clear();
    lastResponseChunks.clear();
}

void tearDown(void) {
    // Clean up after each test
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

void test_chunk_assembly(void) {
    // Create chunks
    BleChunk chunk1 = createChunk(1, 0, 0, {'A', 'B', 'C'});
    BleChunk chunk2 = createChunk(1, 1, BleChunkHead::FINAL_CHUNK_FLAG, {'D', 'E', 'F'});

    // Consume chunks
    chunker->consumeChunk(chunk1);
    chunker->consumeChunk(chunk2);

    // Verify
    TEST_ASSERT_TRUE(onMessageCalled);
    TEST_ASSERT_EQUAL(6, lastReceivedMessage.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY("ABCDEF", lastReceivedMessage.data(), 6);
    TEST_ASSERT_TRUE(onRespondCalled);
    TEST_ASSERT_EQUAL(1, lastResponseChunks.size());
}

void test_message_size_overflow(void) {
    // Create a chunk larger than maxBlobSize
    BleChunk largeChunk = createChunk(1, 0, 0, std::vector<uint8_t>(600, 0));

    // Consume chunk
    chunker->consumeChunk(largeChunk);

    // Verify
    TEST_ASSERT_FALSE(onMessageCalled);
    TEST_ASSERT_TRUE(onRespondCalled);
    TEST_ASSERT_EQUAL(1, lastResponseChunks.size());
    if (!lastResponseChunks.empty()) {
        BleChunkHead errorHead;
        std::copy(lastResponseChunks[0].begin(), lastResponseChunks[0].begin() + sizeof(errorHead), reinterpret_cast<uint8_t*>(&errorHead));
        TEST_ASSERT_EQUAL(BleChunkHead::SIZE_OVERFLOW_FLAG | BleChunkHead::FINAL_CHUNK_FLAG, errorHead.flags);
    }
}

void test_missed_chunks(void) {
    // Create chunks with a gap in sequence numbers
    BleChunk chunk1 = createChunk(1, 0, 0, {'A', 'B', 'C'});
    BleChunk chunk2 = createChunk(1, 2, BleChunkHead::FINAL_CHUNK_FLAG, {'D', 'E', 'F'});

    // Consume chunks
    chunker->consumeChunk(chunk1);
    chunker->consumeChunk(chunk2);

    // Verify
    TEST_ASSERT_FALSE(onMessageCalled);
    TEST_ASSERT_TRUE(onRespondCalled);
    TEST_ASSERT_EQUAL(1, lastResponseChunks.size());
    if (!lastResponseChunks.empty()) {
        BleChunkHead errorHead;
        std::copy(lastResponseChunks[0].begin(), lastResponseChunks[0].begin() + sizeof(errorHead), reinterpret_cast<uint8_t*>(&errorHead));
        TEST_ASSERT_EQUAL(BleChunkHead::MISSED_CHUNKS_FLAG | BleChunkHead::FINAL_CHUNK_FLAG, errorHead.flags);
    }
}

void test_multiple_messages(void) {
    // Create and process the first message
    BleChunk chunk1a = createChunk(1, 0, 0, {'M', 'S', 'G'});
    BleChunk chunk1b = createChunk(1, 1, BleChunkHead::FINAL_CHUNK_FLAG, {'1', 'S', 'T'});

    chunker->consumeChunk(chunk1a);
    chunker->consumeChunk(chunk1b);

    // Verify the first message
    TEST_ASSERT_TRUE(onMessageCalled);
    TEST_ASSERT_EQUAL(6, lastReceivedMessage.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY("MSG1ST", lastReceivedMessage.data(), 6);
    TEST_ASSERT_TRUE(onRespondCalled);
    TEST_ASSERT_EQUAL(1, lastResponseChunks.size());

    // Reset flags for the next message
    onMessageCalled = false;
    onRespondCalled = false;
    lastReceivedMessage.clear();
    lastResponseChunks.clear();

    // Create and process the second message
    BleChunk chunk2a = createChunk(2, 0, 0, {'2', 'N', 'D'});
    BleChunk chunk2b = createChunk(2, 1, BleChunkHead::FINAL_CHUNK_FLAG, {'M', 'S', 'G'});

    chunker->consumeChunk(chunk2a);
    chunker->consumeChunk(chunk2b);

    // Verify the second message
    TEST_ASSERT_TRUE(onMessageCalled);
    TEST_ASSERT_EQUAL(6, lastReceivedMessage.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY("2NDMSG", lastReceivedMessage.data(), 6);
    TEST_ASSERT_TRUE(onRespondCalled);
    TEST_ASSERT_EQUAL(1, lastResponseChunks.size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_chunk_assembly);
    RUN_TEST(test_message_size_overflow);
    RUN_TEST(test_missed_chunks);
    RUN_TEST(test_multiple_messages);
    return UNITY_END();
}
