import { test } from 'node:test';
import { strict as assert } from 'assert';
import { RpcCaller } from '../src/RpcCaller';
import { BinaryTransport } from '../src/BleClientChunker';

function s2bin(str: string): Uint8Array {
    return new TextEncoder().encode(str);
}

class MockTransport implements BinaryTransport {
    private readResponses: Uint8Array[];
    private writes: Uint8Array[];

    constructor(readResponses: Uint8Array[]) {
        this.readResponses = readResponses;
        this.writes = [];
    }

    async send(data: Uint8Array): Promise<Uint8Array> {
        this.writes.push(data);
        if (this.readResponses.length === 0) {
            throw new Error('No more data to read');
        }
        return this.readResponses.shift()!;
    }

    getWrites(): Uint8Array[] {
        return this.writes;
    }
}

test('RpcCaller should correctly send a request and receive a successful response', async () => {
    const mockResponse = s2bin('{ "ok": true, "result": 42 }');
    const transport = new MockTransport([mockResponse]);
    const rpcClient = new RpcCaller(transport);

    const result = await rpcClient.invoke('someMethod', 1, 2, 3);

    assert.strictEqual(result, 42);
});

test('RpcCaller should throw an error when the response contains an error', async () => {
    const mockResponse = s2bin('{ "ok": false, "result": "Error message" }');
    const transport = new MockTransport([mockResponse]);
    const rpcClient = new RpcCaller(transport);

    await assert.rejects(
        async () => {
            await rpcClient.invoke('someMethod', 1, 2, 3);
        },
        new Error('RPC Error: Error message')
    );
});

test('RpcCaller should correctly handle different argument types', async () => {
    const mockResponse = s2bin('{ "ok": true, "result": "success" }');
    const transport = new MockTransport([mockResponse]);
    const rpcClient = new RpcCaller(transport);

    const result = await rpcClient.invoke('anotherMethod', true, 123, 'test');

    assert.strictEqual(result, 'success');

    // Check the sent data
    const writes = transport.getWrites();
    assert.strictEqual(writes.length, 1);
    assert.deepStrictEqual(writes[0], s2bin('{"method":"anotherMethod","args":[true,123,"test"]}'));
});

test('RpcCaller should handle empty argument list', async () => {
    const mockResponse = s2bin('{ "ok": true, "result": "empty" }');
    const transport = new MockTransport([mockResponse]);
    const rpcClient = new RpcCaller(transport);

    const result = await rpcClient.invoke('methodWithoutArgs');

    assert.strictEqual(result, 'empty');

    // Check the sent data
    const writes = transport.getWrites();
    assert.strictEqual(writes.length, 1);
    assert.deepStrictEqual(writes[0], s2bin('{"method":"methodWithoutArgs","args":[]}'));
});

test('RpcCaller should handle unicode characters correctly', async () => {
    const mockResponse = s2bin('{ "ok": true, "result": "успех" }');
    const transport = new MockTransport([mockResponse]);
    const rpcClient = new RpcCaller(transport);

    const result = await rpcClient.invoke('unicodeMethod', 'тест');

    assert.strictEqual(result, 'успех');

    // Check the sent data
    const writes = transport.getWrites();
    assert.strictEqual(writes.length, 1);
    assert.deepStrictEqual(writes[0], s2bin('{"method":"unicodeMethod","args":["тест"]}'));
});
