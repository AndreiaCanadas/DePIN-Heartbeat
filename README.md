# Solana DePIN - Heartbeat Monitor

## System Overview

This project consists of three integrated components that create a decentralized physical infrastructure network (DePIN) for health monitoring and token rewards:

### System Components
1. **ESP32 Microcontroller** - Physical device with heart rate sensor and OLED display
2. **Solana Protocol** - On-chain program for data storage and reward calculation  
3. **Webpage** - User interface for account management and reward minting

### User Journey
1. User connects wallet and creates account from webpage
2. On-chain HeartBeat account is initialized with user's wallet address
3. User starts ESP32 device to begin heart rate monitoring
4. ESP32 sends heart rate data to Solana protocol every minute
5. Protocol calculates points based on heart rate above threshold (currently set to 100 BPM)
6. User views monitoring data and mints SPL token rewards on webpage

### Data Flow
- **ESP32 → Solana**: Heart rate readings (every 60 seconds)
- **Solana Protocol**: Stores data in circular buffer, calculates points, manages rewards
- **Webpage ← Solana**: Fetches user data and points for display
- **Webpage → Solana**: Allows user to mints SPL tokens based on accumulated points

---
## ESP32 Component

This repository contains the ESP32 firmware that monitors heart rate and communicates with the Solana protocol.

### Hardware Requirements
- ESP32 microcontroller
- KY-039 heart rate sensor (analog output)
- 128x64 OLED display (I2C, SDA=A4, SCL=A5)
- WiFi connectivity

### Core Functionality
1. **Initialization**
   - Initialize OLED display and heart rate sensor
   - Connect to WiFi network
   - Establish Solana connection and finds user's HeartBeat account

2. **Heart Rate Monitoring**
   - Continuous 20ms sampling to eliminate electrical noise
   - Peak detection algorithm using rising edge threshold
   - BPM calculation from beat intervals (weighted average of last 3 beats)
   - Real-time display on OLED screen

3. **Data Transmission**
   - Send heart rate data to Solana protocol every 60 seconds
   - Display transaction status on OLED
   - Handle network errors and retry logic

### Heart Rate Detection Algorithm
- **Noise Elimination**: 20ms continuous sampling removes 50Hz electrical interference
- **Signal Smoothing**: 4-sample rolling average for stable readings
- **Peak Detection**: Detects heartbeat peaks using 4 consecutive rising values threshold
- **BPM Calculation**: Time intervals between peaks converted to beats per minute
- **Range Validation**: Accepts only realistic heart rates (30-200 BPM)
- **Stability**: Weighted average of last 3 beat intervals for consistent readings

### Display System
- **Startup**: WiFi connection status and system initialization
- **Monitoring**: Real-time heart rate display with visual heartbeat pattern
- **Transactions**: Status messages for data transmission to blockchain

---
## Solana Protocol

The on-chain program manages user data and reward calculations:

### Account Structure
- **HeartBeat Account**: Stores user's wallet address, heart rate history, points, and timestamps
- **Circular Buffer**: Maintains last 10 heart rate readings for point calculation
- **Points System**: Accumulates points when heart rate stays above 100 BPM threshold

### Key Instructions
1. **Initialize**: Create new HeartBeat account for user
2. **Log Heartbeat**: Store heart rate reading (rate limited to once per minute)
3. **Mint Rewards**: Convert accumulated points to SPL tokens (every 10 minutes)

### Reward Logic
- Points earned when heart rate readings stay above 100 BPM threshold
- Point value = average heart rate of last 10 readings
- SPL tokens minted based on accumulated points
- Rate limiting prevents spam and ensures fair distribution

**Repository**: https://github.com/AndreiaCanadas/anchor-heart-beat

## Webpage

User interface for account management and reward minting:

### Features
- Wallet connection and account initialization
- Real-time heart rate monitoring dashboard
- Points balance and reward history
- SPL token minting interface

**Repository**: https://github.com/AndreiaCanadas/heartbeat-monitor-dashboard

## Future Considerations
- Off-chain data logging for improved scalability
- Sustainable token emission model
- Integration with fitness tracking platforms