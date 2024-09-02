// src/index.ts
class BleManager {
  private serviceUuid: string;
  
  constructor(serviceUuid: string) {
    this.serviceUuid = serviceUuid;
  }

  async scanForDevices(): Promise<void> {
    try {
      console.log('Requesting Bluetooth Device...');
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [this.serviceUuid] }],
      });

      console.log(`> Name: ${device.name}`);
      console.log(`> Id: ${device.id}`);

      // Here you can add further logic to connect to the device, read/write characteristics, etc.
    } catch (error) {
      console.error('Error:', error);
    }
  }
}

const bleManager = new BleManager('5f524546-4c4f-575f-5250-435f5356435f');

document.getElementById('scanButton')?.addEventListener('click', () => {
  bleManager.scanForDevices();
});
  