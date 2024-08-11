#!/usr/bin/env -S node --import=tsx

import { BleConnector } from './src/BleConnector.ts';

// Example script to scan for BLE devices
async function main() {
    const connector = new BleConnector();
    // Add scanning logic here
    console.log('Scanning for devices...');
}

main().catch(console.error);
