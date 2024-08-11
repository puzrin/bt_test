export class BleClientChunker {
    private readFn: () => Promise<Uint8Array>;
    private writeFn: (data: Uint8Array) => Promise<void>;

    constructor(readFn: () => Promise<Uint8Array>, writeFn: (data: Uint8Array) => Promise<void>) {
        this.readFn = readFn;
        this.writeFn = writeFn;
    }

    async send(data: Uint8Array): Promise<Uint8Array> {
        await this.writeFn(data);
        return await this.readFn();
    }

    async enqueueRequest(data: Uint8Array): Promise<Uint8Array> {
        return this.send(data);
    }
}
