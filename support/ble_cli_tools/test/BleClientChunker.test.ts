import { test } from 'node:test';
import { strict as assert } from 'assert';
import { BleClientChunker } from '../src/BleClientChunker';

test('BleClientChunker should send and receive data', async () => {
    const mockRead = async (): Promise<Uint8Array> => new Uint8Array([1, 2, 3, 4]);
    const mockWrite = async (data: Uint8Array): Promise<void> => {
        assert.deepStrictEqual(data, new Uint8Array([116, 101, 115, 116]), 'Sent data should match');
    };

    const chunker = new BleClientChunker(mockRead, mockWrite);
    const response = await chunker.enqueueRequest(new Uint8Array([116, 101, 115, 116])); // 'test' in Uint8Array

    assert.deepStrictEqual(response, new Uint8Array([1, 2, 3, 4]), 'Response should match expected data');
});
