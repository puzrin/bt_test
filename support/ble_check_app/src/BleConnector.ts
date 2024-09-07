import { IO, isLastChunk } from './BleClientChunker';

export class BleConnector implements IO {
    private device: BluetoothDevice | null = null;
    private gatt: BluetoothRemoteGATTServer | null = null;
    private characteristic: BluetoothRemoteGATTCharacteristic | null = null;

    static readonly SERVICE_UUID = '5f524546-4c4f-575f-5250-435f5356435f';
    static readonly CHARACTERISTIC_UUID = '5f524546-4c4f-575f-5250-435f494f5f5f';

    async connect(): Promise<void> {
        try {
            console.log('Scanning for devices...');

            this.device = await navigator.bluetooth.requestDevice({
                filters: [{ services: [BleConnector.SERVICE_UUID] }]
            });
            if (!this.device) throw new Error('No device found during scan.');

            console.log(`Connecting to GATT Server on ${this.device.name}...`);

            this.gatt = (await this.device.gatt?.connect()) || null;
            if (!this.gatt) {
                throw new Error('Failed to connect to GATT server.');
            }

            console.log('Getting primary service...');

            const services = await this.gatt.getPrimaryServices();
            if (services.length !== 1) throw new Error(`Bad amount of services (${services.length}).`);
            const service = services[0];

            console.log('Getting characteristic...');

            this.characteristic = await service.getCharacteristic(BleConnector.CHARACTERISTIC_UUID);
            if (!this.characteristic) {
                throw new Error('Failed to get characteristic.');
            }

            console.log('BLE connection and initialization complete');
        } catch (error) {
            console.error('Connection failed:', error);
            throw error;
        }
    }

    async write(data: Uint8Array): Promise<void> {
        if (!this.characteristic) {
            throw new Error('No characteristic found, cannot write.');
        }

        if (isLastChunk(data)) {
            await this.characteristic.writeValue(data);
        } else {
            await this.characteristic.writeValueWithoutResponse(data);
        }
    }

    async read(): Promise<Uint8Array> {
        if (!this.characteristic) {
            throw new Error('No characteristic found, cannot read.');
        }
        const value = await this.characteristic.readValue();
        return new Uint8Array(value.buffer);
    }

    async disconnect(): Promise<void> {
        try {
            if (this.gatt && this.gatt.connected) {
                console.log(`Disconnecting from GATT server on ${this.device?.name}...`);
                this.gatt.disconnect();
            } else {
                console.log('No connected GATT server to disconnect from.');
            }
        } catch (error) {
            console.error('Disconnection failed:', error);
        } finally {
            this.device = null;
            this.gatt = null;
            this.characteristic = null;
            console.log('Disconnected and resources cleared.');
        }
    }
}
