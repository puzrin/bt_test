#!/usr/bin/env -S node --import=tsimp/import

import { BleRpcClient } from './src/BleRpcClient';
import { BleClientChunker } from './src/BleClientChunker';
import { BleConnector } from './src/BleConnector';

async function main() {
    const [method, ...args] = process.argv.slice(2);

    if (!method) {
        console.error('Usage: call.ts <method> [args...]');
        process.exit(1);
    }

    const connector = new BleConnector();
    await connector.connect();

    const chunker = new BleClientChunker(
        () => connector.read(),
        (data) => connector.write(data)
    );
    const rpcClient = new BleRpcClient(chunker);

    try {
        const result = await rpcClient.invoke(method, ...args);
        console.log('Result:', result);
    } catch (error) {
        console.error('Error invoking method:', error);
    }
}

main().catch(console.error);
