import { BleClientChunker } from './BleClientChunker';

export class BleRpcClient {
    private chunker: BleClientChunker;

    constructor(chunker: BleClientChunker) {
        this.chunker = chunker;
    }

    async invoke(method: string, ...args: any[]): Promise<any> {
        const request = new TextEncoder().encode(JSON.stringify({ method, args }));
        const response = await this.chunker.enqueueRequest(request);
        return JSON.parse(new TextDecoder().decode(response));
    }
}
