export class BleChunkHead {
    messageId: number;
    sequenceNumber: number;
    flags: number;

    static readonly FINAL_CHUNK_FLAG = 0x01;
    static readonly MISSED_CHUNKS_FLAG = 0x02;
    static readonly SIZE_OVERFLOW_FLAG = 0x04;

    constructor(messageId: number, sequenceNumber: number, flags: number) {
        this.messageId = messageId;
        this.sequenceNumber = sequenceNumber;
        this.flags = flags;
    }

    static fromBuffer(buffer: Uint8Array): BleChunkHead {
        return new BleChunkHead(buffer[0], (buffer[1] << 8) | buffer[2], buffer[3]);
    }

    toBuffer(): Uint8Array {
        const buffer = new Uint8Array(4);
        buffer[0] = this.messageId;
        buffer[1] = (this.sequenceNumber >> 8) & 0xff;
        buffer[2] = this.sequenceNumber & 0xff;
        buffer[3] = this.flags;
        return buffer;
    }
}

export type BleChunk = Uint8Array;
export type BleMessage = Uint8Array;

export interface IO {
    read: () => Promise<Uint8Array>;
    write: (data: Uint8Array) => Promise<void>;
}

export function mergeUint8Arrays(arrays: Uint8Array[]): Uint8Array {
    const result = new Uint8Array(arrays.reduce((acc, arr) => acc + arr.length, 0));
    arrays.reduce((offset, arr) => (result.set(arr, offset), offset + arr.length), 0);
    return result;
}

export class BleClientChunker {
    private io: IO;
    private maxBlobSize: number;
    private messageIdCounter: number;
    private queue: Promise<BleMessage>;

    constructor(io: IO, maxBlobSize = 500) {
        this.io = io;
        this.maxBlobSize = maxBlobSize;
        this.messageIdCounter = 0;
        this.queue = Promise.resolve(new Uint8Array(0));  // Initialize the queue with a resolved promise
    }

    private splitIntoChunks(message: BleMessage): BleChunk[] {
        const chunks: BleChunk[] = [];
        const totalSize = message.length;
        const chunkSize = this.maxBlobSize - 4;

        // Increment the messageId counter, wrapping around at 255
        this.messageIdCounter = (this.messageIdCounter + 1) & 0xff;

        for (let i = 0; i < totalSize; i += chunkSize) {
            const end = Math.min(i + chunkSize, totalSize);
            const chunk = message.slice(i, end);
            const head = new BleChunkHead(this.messageIdCounter, i / chunkSize, (end === totalSize) ? BleChunkHead.FINAL_CHUNK_FLAG : 0);
            chunks.push(mergeUint8Arrays([head.toBuffer(), chunk]));
        }

        return chunks;
    }

    async sendOne(data: BleMessage): Promise<BleMessage> {
        while (true) {
            const chunks = this.splitIntoChunks(data);

            // Send all chunks
            for (const chunk of chunks) {
                await this.io.write(chunk);
            }

            // Read the first chunk of the response
            let responseChunk = await this.io.read();
            const head = BleChunkHead.fromBuffer(responseChunk);

            // Check for errors
            if (head.flags & BleChunkHead.SIZE_OVERFLOW_FLAG) {
                throw new Error('Protocol error: chunk size overflow');
            }

            if (head.flags & BleChunkHead.MISSED_CHUNKS_FLAG) {
                // If missed chunks, retry the entire process
                continue;
            }

            // If the response is empty, retry after 100ms
            if (responseChunk.length === head.toBuffer().length) {
                await new Promise(resolve => setTimeout(resolve, 100));  // Wait for 100ms
                responseChunk = await this.io.read();

                if (responseChunk.length === head.toBuffer().length) {
                    throw new Error('Protocol error: received empty response twice');
                }
            }

            // Read remaining chunks if necessary
            const responseChunks: BleChunk[] = [responseChunk];

            while (!isLastChunk(responseChunk)) {
                responseChunk = await this.io.read();
                responseChunks.push(responseChunk);
            }

            // Merge the response chunks into a single BleMessage and return
            return mergeUint8Arrays(responseChunks.map(chunk => chunk.slice(4)));
        }
    }

    async send(data: BleMessage): Promise<BleMessage> {
        // Add the sendOne operation to the queue, suppressing any errors from previous operations
        this.queue = this.queue.catch(() => {}).then(() => this.sendOne(data));
        return this.queue;
    }
}

export function isLastChunk(chunk: BleChunk): boolean {
    const head = BleChunkHead.fromBuffer(chunk);
    return !!(head.flags & BleChunkHead.FINAL_CHUNK_FLAG);
}
