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

export class BleRpcClient {
    private device: BluetoothDevice | null = null;
    private gattServer: BluetoothRemoteGATTServer | null = null;

    private rpcIO: BleCharacteristicIO;
    private authIO: BleCharacteristicIO;

    private rpcCaller: RpcCaller;
    private authCaller: RpcCaller;

    private authStorage = new AuthStorage();

    private isConnectedFlag = false;
    private isAuthenticatedFlag = false;
    private isReconnecting = false;

    private reconnectAttempts = 0;
    private maxReconnectAttempts = 5;
    private reconnectInterval = 5000; // in milliseconds

    private boundedDisconnectHandler: () => void;

    // UUIDs
    private static readonly SERVICE_UUID = '5f524546-4c4f-575f-5250-435f5356435f'; // _REFLOW_RPC_SVC_
    private static readonly RPC_CHARACTERISTIC_UUID = '5f524546-4c4f-575f-5250-435f494f5f5f'; // _REFLOW_RPC_IO__
    private static readonly AUTH_CHARACTERISTIC_UUID = '5f524546-4c4f-575f-5250-435f41555448'; // _REFLOW_RPC_AUTH

    constructor() {
        // Initialize IO objects without characteristics
        this.rpcIO = new BleCharacteristicIO();
        this.authIO = new BleCharacteristicIO();

        // Initialize BleClientChunkers with the IO objects
        const rpcChunker = new BleClientChunker(this.rpcIO);
        const authChunker = new BleClientChunker(this.authIO);

        // Initialize RpcCallers with the chunkers
        this.rpcCaller = new RpcCaller(rpcChunker);
        this.authCaller = new RpcCaller(authChunker);

        // Bind disconnect handler
        this.boundedDisconnectHandler = this.handleDisconnection.bind(this);
    }

    /**
     * Establishes a BLE connection and performs authentication.
     */
    async connect(): Promise<void> {
        try {
            if (this.isConnected()) {
                if (!this.isAuthenticated()) {
                    await this.authenticate();
                }
                return;
            }

            // Request device if not previously connected
            if (!this.device) {
                this.device = await navigator.bluetooth.requestDevice({
                    filters: [{ services: [BleRpcClient.SERVICE_UUID] }],
                });
            }
            if (!this.device) throw new Error('No device found during scan.');

            console.log(`Connecting to GATT Server on ${this.device.name}...`);

            this.gattServer = (await this.device.gatt?.connect()) ?? null;
            if (!this.gattServer) {
                throw new Error('Failed to connect to GATT server.');
            }
            this.isConnectedFlag = true;

            // Get primary service
            console.log('Getting primary service...');

            const services = await this.gattServer.getPrimaryServices();
            if (services?.length !== 1) {
                throw new Error(`Bad amount of services (${services.length}).`);
            }
            const service = services[0];

            // Get characteristics
            console.log('Getting characteristic...');
            
            let rpcCharacteristic = await service.getCharacteristic(BleRpcClient.RPC_CHARACTERISTIC_UUID)
            let authCharacteristic = await service.getCharacteristic(BleRpcClient.AUTH_CHARACTERISTIC_UUID)
            if (!rpcCharacteristic || !authCharacteristic) {
                throw new Error('Failed to get characteristics.');
            }

            // Attach characteristics in IO objects
            this.rpcIO.setCharacteristic(rpcCharacteristic);
            this.authIO.setCharacteristic(authCharacteristic);

            // Perform authentication
            await this.authenticate();

            // Set up disconnect listener
            this.device.removeEventListener('gattserverdisconnected', this.boundedDisconnectHandler); // Ensure only one listener
            this.device.addEventListener('gattserverdisconnected', this.boundedDisconnectHandler);

            console.log('BLE connection and initialization complete');
        } catch (error) {
            this.isConnectedFlag = false;
            throw error;
        }
    }

    /**
     * Checks if the device is connected.
     */
    isConnected() { return this.isConnectedFlag && this.gattServer?.connected === true; }

    /**
     * Checks if the client is authenticated with the device.
     */
    isAuthenticated() { return this.isAuthenticatedFlag; }

    /**
     * Checks if the device is ready for RPC communication.
     */
    isReady() { return this.isConnected() && this.isAuthenticated(); }

    /**
     * Sends an RPC request to the device.
     */
    async invoke(method: string, ...args: RpcArgument[]): Promise<RpcResult> {
        if (!this.isReady()) {
            throw new Error('DisconnectedError: Device is not ready');
        }

        return await this.rpcCaller.invoke(method, ...args);
    }

    /**
     * Performs authentication with the device.
     */
    private async authenticate(recursive_cnt = 0): Promise<void> {
        try {
            const auth_info : AuthInfo = JSON.parse(await this.authCaller.invoke('auth_info') as string);

            const device_id = auth_info.id;
            const client_id = this.authStorage.getClientId();
            const secret = this.authStorage.getSecret(device_id);

            if (!secret) {
                if (recursive_cnt > 1) throw new Error('Failed to pair with device.');

                if (!auth_info.pairable) {
                    throw new NotPairedError('Enable pairing mode on device and repeat connection!');
                }

                console.log('Pairing...');

                const new_secret  = await this.authCaller.invoke('pair', client_id) as string;
                this.authStorage.setSecret(device_id, new_secret);

                console.log('Retry authentication...');

                return await this.authenticate(++recursive_cnt);
            }

            console.log('Authenticating...');

            const signature = await this.authStorage.calculateHMAC(auth_info.hmac_msg, secret);            
            const authenticated = await this.authCaller.invoke('authenticate', client_id, signature, Date.now()) as boolean;

            if (!authenticated) {
                if (recursive_cnt > 1) throw new Error('Failed to pair with device.');

                // Wrong key. Clear and retry.
                console.log('Authentication failed. Clearing secret and retrying...');

                this.authStorage.setSecret(device_id, '');
                return await this.authenticate(++recursive_cnt);
            }

            console.log('Ready!');
            this.isAuthenticatedFlag = true;
        } catch (error) {
            this.isAuthenticatedFlag = false;
            throw error;
        }
    }

    /**
     * Handles disconnection events and initiates reconnection attempts.
     */
    private async handleDisconnection(): Promise<void> {
        console.log('Disconnected from GATT server.');

        this.isConnectedFlag = false;
        this.isAuthenticatedFlag = false;

        // Clear characteristics in IO objects
        this.rpcIO.setCharacteristic(null);
        this.authIO.setCharacteristic(null);

        if (this.isReconnecting) return;

        this.isReconnecting = true;
        this.reconnectAttempts = 0;

        while (this.reconnectAttempts < this.maxReconnectAttempts) {
            try {
                await this.connect();
                this.isReconnecting = false;
                break;
            } catch (error) {
                this.reconnectAttempts++;
                console.warn(`Reconnect attempt ${this.reconnectAttempts} failed:`, error);
                await this.delay(this.reconnectInterval);
            }
        }

        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.error('Failed to reconnect after maximum attempts.');
            this.isReconnecting = false;
        }
    }

    /**
     * Utility function for delaying execution.
     */
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
