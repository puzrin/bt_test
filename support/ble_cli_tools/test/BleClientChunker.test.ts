import { test } from 'node:test';
import { strict as assert } from 'assert';
import { BleClientChunker } from '../src/BleClientChunker';

test('BleClientChunker basic initialization', () => {
    const mockRead = async (): Promise<Uint8Array> => new Uint8Array();
    const mockWrite = async (data: Uint8Array): Promise<void> => {};

    const chunker = new BleClientChunker({ read: mockRead, write: mockWrite });

    assert.ok(chunker, 'BleClientChunker should be initialized');
});
