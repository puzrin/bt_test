import { BleConnector } from './BleConnector';
import { BleClientChunker } from './BleClientChunker';
import { BleRpcClient } from './BleRpcClient';

class BleManager {
  private connector: BleConnector;

  constructor() {
    this.connector = new BleConnector(); // Use the BleConnector instance
  }

  async scanForDevices(): Promise<void> {
    if (!navigator.bluetooth) {
      alert('Web Bluetooth API is not available in this browser.');
      return;
    }

    try {
      console.log('Requesting Bluetooth Device...');
      await this.connector.connect(); // Connect using BleConnector

      // Create the chunker for managing data transfer over BLE
      const chunker = new BleClientChunker(this.connector);

      // Create the RPC client for invoking RPC methods
      const rpcClient = new BleRpcClient(chunker);

      try {
        // Invoke echo with a short string
        const shortMessage = 'Hello, BLE!';
        const shortResponse = await rpcClient.invoke('echo', shortMessage);
        console.log(`Echo response (short): ${shortResponse}`);

        // Invoke echo with a long string (~2000 characters)
        const longMessage = 'A'.repeat(2000);
        const longResponse = await rpcClient.invoke('echo', longMessage);
        console.log(`Echo response (long): ${longResponse}`);

        // Invoke ping
        const ping_output = await rpcClient.invoke('ping');
        console.log(`Ping response: ${ping_output}`);

        // Invoke sum
        const sum_output = await rpcClient.invoke('sum', 4, 5);
        console.log(`Sum response: ${sum_output}`);
      } catch (error) {
        console.error('Error invoking RPC:', error);
      }
    } catch (error) {
      console.error('Error during scanning or connection:', error);
    }
  }
}

const bleManager = new BleManager();

document.getElementById('scanButton')?.addEventListener('click', () => {
  bleManager.scanForDevices();
});
