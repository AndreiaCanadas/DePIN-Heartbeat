# ❤️  Heart Rate DePIN with ESP32
---

## ESP32 Architecture

### Hardware Requirements
- ESP32
- Heart rate sensor (KY039 or other similar)
- WiFi connectivity

### ESP32 Functionality
1. Initialize sensor and WiFi
2. Calibrate baseline heart rate (60 seconds)
3. Main loop:
   - Read heart rate every 20 ms (BPM)
   - Calculate average (BPM)
   - Every 1 minute: prepare transaction
   - Sign data with device private key
   - Send to devnet

### Validations and considerations:
Sanity checks: HR between 40-220 BPM
Variability check: Real hearts have natural variation

---
## Solana Protocol Architecture

1. Initialize Account
2. Log Heartbeat
    - Anti-spam rate limiting (max 1 submission per minute per device)
    - Log data in circular buffer
    - Calculate average rate
    - Reward user with SPL tokens every x minutes if average from last readings are above threshold

State account: 

```rust
pub struct HeartBeat {
    pub owner: Pubkey,
    pub heartbeat: [u32; NUMBER_OF_READINGS],   // circular buffer with most recent at index 0
    pub last_heartbeat: i64,
    pub last_minted: i64,
    pub bump: u8,
}
```

### Solana Protocol Functionality

Main logging functionality:
```rust
pub fn log_heartbeat(ctx: Context<LogHeartBeat>, reading: u32) -> Result<()> {
    ctx.accounts.log_heartbeat(reading)?;
    let average = ctx.accounts.get_average_heartbeat()?;
    if average > HEARTBEAT_THRESHOLD {
        if Clock::get()?.unix_timestamp - ctx.accounts.heartbeat.last_minted > MINT_TIME {
            ctx.accounts.mint_reward(average, &ctx.bumps)?;
        }
    }
    Ok(())
}
```

Logging Instructions:
```rust
impl<'info> LogHeartBeat<'info> {
    pub fn log_heartbeat(&mut self, reading: u32) -> Result<()> {

        // allow only 1 reading per minute
        require!(
            Clock::get()?.unix_timestamp - self.heartbeat.last_heartbeat > 60, 
            CustomError::HeartbeatPerMinute
        );

        self.heartbeat.last_heartbeat = Clock::get()?.unix_timestamp;

        // Shift all values to the right and insert the new reading at index 0 (circular buffer style)
        for i in (1..NUMBER_OF_READINGS).rev() {
            self.heartbeat.heartbeat[i] = self.heartbeat.heartbeat[i - 1];
        }
        self.heartbeat.heartbeat[0] = reading;

        Ok(())
    }

    pub fn get_average_heartbeat(&mut self) -> Result<u32> {
        let sum: u32 = self.heartbeat.heartbeat.iter().sum();
        let average = sum / NUMBER_OF_READINGS as u32;
        Ok(average)
    }

    pub fn mint_reward(&mut self, average: u32, bumps: &LogHeartBeatBumps) -> Result<()> {
        // mint rewards if time interval has elapsed
        require!(
            Clock::get()?.unix_timestamp - self.heartbeat.last_minted > MINT_TIME, 
            CustomError::MintingTimeIntervalNotElapsed
        );
        self.heartbeat.last_minted = Clock::get()?.unix_timestamp;
            
        let signer_seeds: &[&[&[u8]]] = &[&[b"authority", &[bumps.authority]]];

        let cpi_accounts = MintTo {
            mint: self.mint.to_account_info(),
            to: self.token_account.to_account_info(),
            authority: self.authority.to_account_info(),
        };
        let cpi_ctx = CpiContext::new_with_signer(self.token_program.to_account_info(), cpi_accounts, signer_seeds);
        mint_to(cpi_ctx, average as u64)?;

        Ok(())
    }
}
```

---
## Scaling and final considerations:
- Sustainable token emission
- Update account to save points and separate method to claim rewards
- Partnerships for token utility