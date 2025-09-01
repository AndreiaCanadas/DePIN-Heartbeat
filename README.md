# ESP32 DePIN Template

A simple template for building Solana DePIN projects using an ESP32

## Features

- 🌐 **WiFi connectivity**
- 🔗 **Solana blockchain integration**
- 💡 **LED status indicators**
- 💰 **Wallet balance checking**
- 🛠️ **Ready for custom sensors**

## Hardware

- **Arduino Nano ESP32**

## Setup

### 1. Configure Credentials
```bash
cp include/credentials_template.h include/credentials.h
```

### 2. Edit Your Credentials
Open `include/credentials.h` and add:
- WiFi SSID and password
- Solana wallet private/public keys
- Recipient wallet address

### 3. Build and Upload
```bash
pio run -t upload
```

## Usage

The device will:
1. Connect to WiFi
2. Check Solana wallet balance
3. Blink LEDs

