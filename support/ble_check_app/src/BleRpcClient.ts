import { BinaryTransport } from './BleClientChunker';

type RpcArgument = boolean | number | string;
type RpcResult = boolean | number | string;

export class BleRpcClient {
    private transport: BinaryTransport;

    constructor(transport: BinaryTransport) {
        this.transport = transport;
    }

    async invoke(method: string, ...args: RpcArgument[]): Promise<RpcResult> {
        // Serialize the request
        const request = {
            method: method,
            args: args
        };

        const requestBin = new TextEncoder().encode(JSON.stringify(request));
        console.log(`Sending RPC request "${request.method}"`);

        const responseBin = await this.transport.send(requestBin);

        // Deserialize the response
        const responseObj = JSON.parse(new TextDecoder().decode(responseBin));

        // Check for errors in the response
        if (responseObj.ok !== true) {
            throw new Error(`RPC Error: ${responseObj.result}`);
        }

        // Return the result
        return responseObj.result as RpcResult;
    }
}
