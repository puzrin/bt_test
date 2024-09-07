import { BleConnector } from './BleConnector';
import { BleClientChunker } from './BleClientChunker';
import { BleRpcClient } from './BleRpcClient';

if (!navigator.bluetooth) {
    alert('Web Bluetooth API is not available in this browser.');
}

window.addEventListener('unhandledrejection', function(event) {
    console.error('Unhandled rejection:', event.reason);
});

const connector = new BleConnector();
const chunker = new BleClientChunker(connector);
const rpcClient = new BleRpcClient(chunker);

document.getElementById('connectButton')?.addEventListener('click', async () => {
    await connector.connect();
});

document.getElementById('disconnectButton')?.addEventListener('click', async () => {
    connector.disconnect();
});

document.getElementById('simpleCommandsButton')?.addEventListener('click', async () => {
    try {
        console.log(`Echo response: ${await rpcClient.invoke('echo', 'Hello, BLE!')}`);

        console.log(`Ping response: ${await rpcClient.invoke('ping')}`);

        console.log(`Sum response: ${await rpcClient.invoke('sum', 4, 5)}`);
    } catch (error) {
        console.error('RPC error:', error);
    }
});

document.getElementById('bigUploadButton')?.addEventListener('click', async () => {
    const total_size = 1024 * 1024;
    const block_size = 16 * 1024;

    for (let i = 0; i < total_size; i += block_size) {
        const block = 'A'.repeat(block_size);
        await rpcClient.invoke('devnull', block);
        console.log(`${new Date().toLocaleTimeString()} Sent ${i + block_size} bytes`);
    }

    console.log('Done');
});