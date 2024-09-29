import { BleClientChunker, isLastChunk, IO } from './BleClientChunker';
import { RpcCaller } from './RpcCaller';
import { AuthStorage } from './AuthStorage';

type RpcArgument = boolean | number | string;
type RpcResult = boolean | number | string;

interface AuthInfo {
    id: string;
    hmac_msg: string;
    pairable: boolean;
}
  
class NotPairedError extends Error {
    constructor(message: string) {
        super(message);
        this.name = 'NotPairedError';
    }
}

type BleEvent = 'disconnected' | 'connected' | 'ready' | 'need_pairing';

export class BleRpcClient {
    private device: BluetoothDevice | null = null;
    private gattServer: BluetoothRemoteGATTServer | null = null;

    private rpcIO = new BleCharacteristicIO();
    private authIO = new BleCharacteristicIO();

    private rpcCaller = new RpcCaller(new BleClientChunker(this.rpcIO));
    private authCaller = new RpcCaller(new BleClientChunker(this.authIO));

    private authStorage = new AuthStorage();

    private emitter = new EventEmitter<BleEvent>();

    private isConnectedFlag = false;
    private isAuthenticatedFlag = false;
    private needPairingFlag = false;
    private boundedDisconnectHandler = this.handleDisconnection.bind(this);

    // UUIDs
    private static readonly SERVICE_UUID = '5f524546-4c4f-575f-5250-435f5356435f'; // _REFLOW_RPC_SVC_
    private static readonly RPC_CHARACTERISTIC_UUID = '5f524546-4c4f-575f-5250-435f494f5f5f'; // _REFLOW_RPC_IO__
    private static readonly AUTH_CHARACTERISTIC_UUID = '5f524546-4c4f-575f-5250-435f41555448'; // _REFLOW_RPC_AUTH

    constructor() {
        this.loop().catch(console.error);
    }

    isConnected() { return this.isConnectedFlag && this.gattServer?.connected === true; }
    isAuthenticated() { return this.isAuthenticatedFlag; }
    isReady() { return this.isConnected() && this.isAuthenticated(); }
    needPairing() { return this.needPairingFlag; }
    isDeviceSelected() { return this.device !== null; }

    on(event: BleEvent, listener: (data?: any) => void) { this.emitter.on(event, listener); }
    off(event: BleEvent, listener: (data?: any) => void) { this.emitter.off(event, listener); }
    once(event: BleEvent, listener: (data?: any) => void) { this.emitter.once(event, listener); }

    async invoke(method: string, ...args: RpcArgument[]): Promise<RpcResult> {
        if (!this.isReady()) throw new Error('DisconnectedError: Device is not ready');
        return await this.rpcCaller.invoke(method, ...args);
    }

    async selectDevice(once = false) {
        if (once && this.device) {
            console.log('Device already found.', this.device);
            return;
        }

        const device = await navigator.bluetooth.requestDevice({
            filters: [{ services: [BleRpcClient.SERVICE_UUID] }],
        });

        if (this.device) {
            // Cleanup old device
            this.device?.removeEventListener('gattserverdisconnected', this.boundedDisconnectHandler);
            this.cleanup();
        }

        this.device = device;
        this.device.addEventListener('gattserverdisconnected', this.boundedDisconnectHandler);
    }

    private lastConnectedTime = 0;
    private lastAuthenticatedTime = 0;
    private readonly CONNECT_DEBOUNCE_PERIOD = 5000; // in milliseconds
    private readonly AUTH_DEBOUNCE_PERIOD = 1000; // in milliseconds

    private async loop() {
        while (true) {
            if (!this.isConnectedFlag && this.device &&
                (Date.now() - this.lastConnectedTime > this.CONNECT_DEBOUNCE_PERIOD)) {
                try {
                    this.lastConnectedTime = Date.now();

                    const [rpcChar, authChar] = await this.connect();

                    if (this.device?.gatt?.connected) {
                        this.rpcIO.setCharacteristic(rpcChar);
                        this.authIO.setCharacteristic(authChar);

                        // Init flags to "just connected" state
                        this.isConnectedFlag = true;
                        this.needPairingFlag = false;
                        this.isAuthenticatedFlag = false;

                        // Reset debounce delays after successful connection
                        this.lastConnectedTime = 0;
                        this.lastAuthenticatedTime = 0;

                        this.emitter.emit('connected');
                    }
                } catch (error) {
                    console.error('Error:', error);
                }
            }

            if (this.isConnectedFlag && !this.isAuthenticatedFlag &&
                (Date.now() - this.lastAuthenticatedTime > this.AUTH_DEBOUNCE_PERIOD)) {
                try {
                    this.lastAuthenticatedTime = Date.now();
                    const successful = await this.authenticate();

                    if (successful) {
                        this.isAuthenticatedFlag = true;
                        this.needPairingFlag = false;
                        this.emitter.emit('ready');
                    } else {
                        this.needPairingFlag = true;
                        this.emitter.emit('need_pairing');
                    }
                } catch (error) {
                    console.error('Error:', error);
                }
            }

            await this.delay(200);
        }
    }

    private async connect(): Promise<Array<BluetoothRemoteGATTCharacteristic>> {

        console.log(`Connecting to GATT Server on ${this.device?.name}...`);

        this.gattServer = (await this.device?.gatt?.connect()) ?? null;
        if (!this.gattServer) {
            throw new Error('Failed to connect to GATT server.');
        }

        // Get primary service
        const services = await this.gattServer.getPrimaryServices();
        if (services?.length !== 1) {
            throw new Error(`Bad amount of services (${services.length}).`);
        }
        const service = services[0];

        // Get characteristics
        let rpcCharacteristic = await service.getCharacteristic(BleRpcClient.RPC_CHARACTERISTIC_UUID)
        let authCharacteristic = await service.getCharacteristic(BleRpcClient.AUTH_CHARACTERISTIC_UUID)
        if (!rpcCharacteristic || !authCharacteristic) {
            throw new Error('Failed to get characteristics.');
        }

        console.log('BLE connection and initialization complete');
        return [rpcCharacteristic, authCharacteristic];
    }

    private async authenticate(): Promise<boolean> {
        try {
            const client_id = this.authStorage.getClientId();

            let auth_info : AuthInfo = JSON.parse(await this.authCaller.invoke('auth_info') as string);
            const device_id = auth_info.id;
            let secret = this.authStorage.getSecret(device_id);

            if (!secret) {
                if (!auth_info.pairable) return false;

                console.log('Try to pair...');

                const new_secret  = await this.authCaller.invoke('pair', client_id) as string;
                this.authStorage.setSecret(device_id, new_secret);
                secret = new_secret;
                // Re-fetch new hmac message value
                auth_info = JSON.parse(await this.authCaller.invoke('auth_info') as string);

                console.log('Paired!');
            }

            console.log('Authenticating...');

            const signature = await this.authStorage.calculateHMAC(auth_info.hmac_msg, secret);            
            const authenticated = await this.authCaller.invoke('authenticate', client_id, signature, Date.now()) as boolean;

            if (!authenticated) {
                // Wrong key. Drop it.
                console.log('Wrong auth key. Clearing...');

                this.authStorage.setSecret(device_id, '');
                return false;
            }

            console.log('Ready!');
            return true;
        } catch (error) {
            console.error('Error:', error);
        }

        return false;
    }

    private async cleanup() {
        this.isConnectedFlag = false;
        this.isAuthenticatedFlag = false;

        // Clear characteristics in IO objects
        this.rpcIO.setCharacteristic(null);
        this.authIO.setCharacteristic(null);
    }

    private async handleDisconnection(): Promise<void> {
        console.log('Disconnected from GATT server.');
        this.cleanup();
        this.emitter.emit('disconnected');
    }

    private delay(ms: number): Promise<void> {
        return new Promise((resolve) => setTimeout(resolve, ms));
    }
}

/**
 * Helper class to adapt a BluetoothRemoteGATTCharacteristic to the IO interface.
 */
class BleCharacteristicIO implements IO {
    private characteristic: BluetoothRemoteGATTCharacteristic | null = null;

    constructor(characteristic?: BluetoothRemoteGATTCharacteristic | null) {
        this.characteristic = characteristic || null;
    }

    setCharacteristic(characteristic: BluetoothRemoteGATTCharacteristic | null): void {
        this.characteristic = characteristic;
    }

    async write(data: Uint8Array): Promise<void> {
        if (!this.characteristic) {
            throw new Error('DisconnectedError: No characteristic available for writing');
        }
        if (isLastChunk(data)) {
            // Last chunk delivery is guaranteed.
            await this.characteristic.writeValueWithResponse(data);
        } else {
            // Intermediate chunk delivery is best-effort.
            await this.characteristic.writeValueWithoutResponse(data);
        }
    }

    async read(): Promise<Uint8Array> {
        if (!this.characteristic) {
            throw new Error('DisconnectedError: No characteristic available for reading');
        }
        const value = await this.characteristic.readValue();
        return new Uint8Array(value.buffer);
    }
}

class EventEmitter<T extends string> {
    private events: { [key in T]?: Array<(data?: any) => void> } = {};

    on(event: T, listener: (data?: any) => void): void {
        if (!this.events[event]) this.events[event] = [];
        this.events[event]?.push(listener);
    }

    off(event: T, listenerToRemove: (data?: any) => void): void {
        if (!this.events[event]) return;
        this.events[event] = this.events[event]?.filter(listener => listener !== listenerToRemove);
    }

    emit(event: T, data?: any): void {
        if (!this.events[event]) return;
        this.events[event]?.forEach(listener => listener(data));
    }

    once(event: T, listener: (data?: any) => void): void {
        const onceListener = (data?: any) => {
            this.off(event, onceListener);
            listener(data);
        };
        this.on(event, onceListener);
    }
}