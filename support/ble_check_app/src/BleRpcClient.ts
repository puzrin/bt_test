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
        console.log(`Sending RPC request: ${JSON.stringify(request)}`);

        // Send the request and receive the response
        const responseBin = await this.transport.send(requestBin);
        console.log(`Received response: ${new TextDecoder().decode(responseBin)}`);

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
