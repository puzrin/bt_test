#!/usr/bin/env -S node --import=tsimp/import

import { BleConnector } from './src/BleConnector';

// Example script to scan for BLE devices
async function main() {
    const connector = new BleConnector();
    // Add scanning logic here
    console.log('Scanning for devices...');
}

main().catch(console.error);
