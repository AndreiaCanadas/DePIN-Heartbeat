/*****************************************************************************************
 * ESP32 Heart Rate DePIN
 * 
 * Decentralized Physical Infrastructure Network (DePIN) device that monitors heart rate
 * and logs data to the Solana blockchain. Earns tokens for contributing health data.
 * 
 * Features:
 * - Real-time heart rate monitoring with KY039 sensor
 * - WiFi connectivity and Solana blockchain integration
 * - Automated data logging to Solana every 6 seconds
 * - LED status indicators for monitoring system state
 * - SPL token rewards for data contribution
 * 
 *****************************************************************************************/

#include "USBCDC.h"
#include "WString.h"
#include "esp32-hal-gpio.h"
#include "esp32-hal.h"
#include "pins_arduino.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "IoTxChain-lib.h"
#include "credentials.h"

/*****************************************************************************************  
* Global Variables
*****************************************************************************************/ 
unsigned long timeMs = millis();

// WiFi
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
#define WIFI_TIMEOUT_MS 10000

// LEDs
#define BLINK_TIME_MS 1000
unsigned long lastBlinkTime = 0;
uint8_t ledOn = 0;
int ledStatus = LOW;

// Heart Rate Sensor KY039
#define HEART_RATE_SENSOR_PIN A0
#define HEART_RATE_SAMPLE_SIZE 20
float heartRate = 0;
float heartRateAverage = 0;
bool heartRateHeaderPrinted = false;
#define HEART_RATE_TIME_MS 500
unsigned long lastHeartRateTime = 0;
#define HEART_RATE_SEND_TIME_MS 60000 // 1 minute
unsigned long lastHeartRateSendTime = 0;
bool heartRateSent = false;
int heartRateCount = 0;
bool heartRateDisplay = 0;

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA A4
#define OLED_SCL A5
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Solana Configuration
IoTxChain solana("https://api.devnet.solana.com");

#define PROGRAM_ID "2hRuCZS1QyXe5N3bYFYvWWRZZqD1t1VwJWjvogfmAM6u"
#define TOKEN_MINT "4f6b8KjU9QHeEHPczAsF4hL5RZvfWW52C5rw6QkW5XHy"
Pubkey TOKEN_PROGRAM_ID = Pubkey::fromBase58("TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
Pubkey SYSTEM_PROGRAM_ID = Pubkey::fromBase58("11111111111111111111111111111111");

// Solana Accounts
Pubkey owner;
Keypair signer;
Pubkey mint;
std::vector<uint8_t> programId;
Pubkey accountPdaPubkey;
Pubkey mintAuthorityPdaPubkey;
Pubkey tokenAccount;
String tokenAccountAddress;
const int maxAttempts = 3;
int attempt = 0;
bool pdaSuccess = false;
#define REWARDS_MINT_TIME_MS 600000 // 10 minutes
unsigned long lastRewardsMintTime = 0;
bool rewardsMinted = false;

/*****************************************************************************************
* Function Declarations
*****************************************************************************************/ 
void connectToWiFi();
void blinkRgbLed();
void readHeartRate();
void printSplTokenBalance();
bool prepareSolanaAccounts();
void printSolanaAccounts();
String vectorToHex(const std::vector<uint8_t>& data);
String generateHeartbeatPattern(float heartRate);
bool sendHeartRateReading(float heartRate);
bool mintRewards();
void initializeDisplay();
void displayHeartRate(float heartRate);
void displayWiFiStatus();
void displayMessage(String message, int line);

/*****************************************************************************************
* Function: Initial Setup
*
* Description: Initializes the setup of the board
* Parameters: None
* Returns: None
*****************************************************************************************/ 
void setup() {
  // initialize serial communication
  Serial.begin(115200);
  delay(2000);

  // initialize LEDs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, HIGH);

  // initialize heart rate sensor
  pinMode(HEART_RATE_SENSOR_PIN, INPUT);
  
  // initialize OLED display
  initializeDisplay();
  delay(2000);
  
  // connect to WiFi
  connectToWiFi();
  displayWiFiStatus();
  delay(2000);
  
  while (!pdaSuccess && attempt < maxAttempts) {
    pdaSuccess = prepareSolanaAccounts();
    if (!pdaSuccess) {
      delay(1000);
      attempt++;
    }
  }
  
  if (pdaSuccess) {
    printSolanaAccounts();
  } else {
    Serial.println("❌ Failed to calculate PDAs");
  }

  // print SPL token balance of user
  printSplTokenBalance();

}

/*****************************************************************************************
* Main Loop
*****************************************************************************************/  
void loop() {
  timeMs = millis();

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_BLUE, HIGH);

  // blink LED
  if (timeMs - lastBlinkTime > BLINK_TIME_MS) {
    lastBlinkTime = timeMs;
    // Option 1: Simple built-in LED toggle
    ledStatus = !ledStatus;
    digitalWrite(LED_BUILTIN, ledStatus);
    // Option 2: RGB LED cycling
    //blinkRgbLed();
  }

  // capture heart rate
  if (timeMs - lastHeartRateTime > HEART_RATE_TIME_MS) {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    lastHeartRateTime = timeMs;
    readHeartRate();
    heartRateAverage = (heartRateAverage + heartRate) / 2;
    
    // Print header once
    if (!heartRateHeaderPrinted) {
      Serial.println("\n== ❤️  Heart Rate Monitor ❤️  ==");
      Serial.println(); // Blank line after header
      heartRateHeaderPrinted = true;
    }

    // Add current reading with heartbeat pattern
    String pattern = generateHeartbeatPattern(heartRateAverage);
    Serial.print(pattern + " ");
    heartRateCount++;
    if (heartRateCount >= 6) {  // Show 4 patterns per line for better visibility
      heartRateCount = 0;
      Serial.print("\r");
    }
    
    // Update OLED display with heart rate
    delay(1000);
    displayHeartRate(heartRateAverage);
  }

  // send heart rate reading
  if ((timeMs - lastHeartRateSendTime > HEART_RATE_SEND_TIME_MS) && pdaSuccess) {
    Serial.println("\n\n=== Sending Heart Rate Reading ===");
    displayMessage("Sending Heart Rate...", 0);
    heartRateHeaderPrinted = false;
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, LOW);
    Serial.println("\nTime since last transaction: " + String(millis()-lastHeartRateSendTime) + "ms\n");
    lastHeartRateSendTime = timeMs;
    heartRateSent = sendHeartRateReading(heartRateAverage);
    Serial.println("Time to send transaction: " + String(millis()-lastHeartRateSendTime) + "ms\n");
    if (heartRateSent) {
      displayMessage("Tx Sent Successfully!", 2);
    } else {
      displayMessage("Failed to send Tx!", 2);
    }
  }

  // Mint rewards
  if (timeMs - lastRewardsMintTime > REWARDS_MINT_TIME_MS) {
    lastRewardsMintTime = timeMs;
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, LOW);
    displayMessage("Minting Rewards...", 0);
    rewardsMinted = mintRewards();
    if (rewardsMinted) {
      displayMessage("Minted Successfully!", 2);
    } else {
      displayMessage("Failed to mint!", 2);
    }
  }

}

/*****************************************************************************************
* Function Definitions
*****************************************************************************************/ 

/*****************************************************************************************
* Function: Blink LED
*
* Description: Blinks the RGB LEDs on the board
* Parameters: None
* Returns: None
*****************************************************************************************/ 
void blinkRgbLed() {

  if (ledOn == 0) { // turn on LED_BLUE
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
    ledOn = 1;
  } else if (ledOn == 1) { // turn on LED_GREEN
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    ledOn = 2;
  } else if (ledOn == 2) { // turn on LED_RED
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
    ledOn = 0;
  }
  
}

/*****************************************************************************************
* Function: Connect to WiFi
*
* Description: Connects to WiFi with global ssid and password
* Parameters: None
* Returns: None
*****************************************************************************************/ 
void connectToWiFi() {
  timeMs = millis();
  Serial.println("Connecting to WiFi...");
  // initialize WiFi
  WiFi.begin(ssid, password);

  // wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED && (millis() - timeMs) < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed");
  }
}

/*****************************************************************************************
* Function: Read Heart Rate
*
* Description: Reads the heart rate from the heart rate sensor and calculates the average
* Parameters: None
* Returns: None
*****************************************************************************************/ 
void readHeartRate() {
  float sum = 0;
  for (int i = 0; i < HEART_RATE_SAMPLE_SIZE; i++) {
    sum += analogRead(HEART_RATE_SENSOR_PIN);
  }
  heartRate = sum / HEART_RATE_SAMPLE_SIZE;
}


/*****************************************************************************************
* Function: Get SPL token balance
*
* Description: Gets the SPL token balance of a wallet address and prints it to the serial monitor
* Parameters: None
* Returns: None
*****************************************************************************************/ 
void printSplTokenBalance() {
  Serial.println("\n=== User SPL Token Balance ===");

  uint64_t rawBalance = 0;

  if (solana.getSplTokenBalance(PUBLIC_KEY, TOKEN_MINT, rawBalance)) {
      float readableBalance = (float)rawBalance / 1e9;
      Serial.print("Token Balance: ");
      Serial.println(readableBalance, 9);
  } else {
      Serial.println("Failed to get SPL token balance.");
  }
  Serial.print("\n");
}

/*****************************************************************************************
* Function: Prepare Solana Accounts
*
* Description: Prepares the Solana accounts and PDAs
* Parameters: None
* Returns: None
*****************************************************************************************/ 
bool prepareSolanaAccounts(){

  // Prepare accounts and signer
  owner = Pubkey::fromBase58(PUBLIC_KEY);
  signer = Keypair::fromPrivateKey(PRIVATE_KEY);
  mint = Pubkey::fromBase58(TOKEN_MINT);
  
  // Prepare program ID
  programId = base58ToPubkey(PROGRAM_ID);

  // Find Heartbeat Account PDA
  std::vector<uint8_t> accountPda;
  // Prepare seeds: 
  std::vector<std::vector<uint8_t>> seeds = {
    {'h','e','a','r','t','b','e','a','t'},
    base58ToPubkey(PUBLIC_KEY)
  };
  uint8_t bump;
  if (!solana.findProgramAddress(seeds, programId, accountPda, bump)) {
      Serial.println("❌ Failed to find Heartbeat Account PDA.");
      return false;
  }
  accountPdaPubkey.data = accountPda;

  // Find Mint Authority PDA
  std::vector<uint8_t> mintAuthorityPda;
  // Prepare seeds: 
  std::vector<std::vector<uint8_t>> seeds2 = {
    {'a','u','t','h','o','r','i','t','y'}
  };
  uint8_t bump2;
  if (!solana.findProgramAddress(seeds2, programId, mintAuthorityPda, bump2)) {
    Serial.println("❌ Failed to find program address for Mint Authority!");
    return false;
  }
  mintAuthorityPdaPubkey.data = mintAuthorityPda;

  // Find Associated Token Account
  if (!solana.findAssociatedTokenAccount(PUBLIC_KEY, TOKEN_MINT, tokenAccountAddress)) {
      Serial.println("❌ Failed to find ATA.");
      return false;
  }
  tokenAccount = Pubkey::fromBase58(tokenAccountAddress);

  return true;

}

/*****************************************************************************************
* Function: Convert vector to Hex
*
* Description: Auxiliary function to convert std::vector<uint8_t> to hex string  
* Parameters: data - vector of bytes to convert
* Returns: String - hex encoded string
*****************************************************************************************/ 
String vectorToHex(const std::vector<uint8_t>& data) {
  String result = "";
  for (int i = 0; i < data.size(); i++) {
    if (data[i] < 16) result += "0";
    result += String(data[i], HEX);
  }
  return result;
}

/*****************************************************************************************
* Function: Print Solana Accounts
*
* Description: Prints all Solana account addresses
* Parameters: None
* Returns: None
*****************************************************************************************/ 
void printSolanaAccounts() {
  Serial.println("\n=== Heartbeat Account PDA (hex) ===");
  Serial.println(vectorToHex(accountPdaPubkey.data));
  
  Serial.println("\n=== Mint Authority PDA (hex) ===");
  Serial.println(vectorToHex(mintAuthorityPdaPubkey.data));
  
  Serial.println("\n=== Associated Token Account ===");
  Serial.println(tokenAccountAddress);
}

/*****************************************************************************************
* Function: Generate Heartbeat Pattern
*
* Description: Creates visual heartbeat pattern based on heart rate value
* Parameters: heartRate - current heart rate reading
* Returns: String - visual pattern representing heartbeat intensity
*****************************************************************************************/ 
String generateHeartbeatPattern(float heartRate) {
  int bpm = (int)heartRate;
  
  // Different patterns based on heart rate ranges
  if (heartRate < 70) {
    return String(bpm) + " __/\\^/\\__";
  } else if (heartRate < 100) {
    return String(bpm) + " _/\\^^/\\_";
  } else if (heartRate < 130) {
    return String(bpm) + " /\\^^^^\\/\\";
  } else {
    return String(bpm) + " /\\^^^^^/\\";
  }
}

/*****************************************************************************************
* Function: Send Heart Rate Reading
*
* Description: 
* Parameters: heartRate - current heart rate reading (float)
* Returns: bool - True if transaction is successful, false otherwise
*****************************************************************************************/ 
bool sendHeartRateReading(float heartRate) {

  // Prepare instruction
  std::vector<uint8_t> discriminator = solana.calculateDiscriminator("log_heartbeat");
  std::vector<uint8_t> data = discriminator;

  // Prepare payload (heart rate as float32 little-endian)
  std::vector<uint8_t> payload(4);
  memcpy(payload.data(), &heartRate, sizeof(float));
  data.insert(data.end(), payload.begin(), payload.end());

  Instruction ix(
    Pubkey{programId},
    std::vector<AccountMeta>{
      AccountMeta::signer(owner),
      AccountMeta::writable(accountPdaPubkey, false),
      AccountMeta{SYSTEM_PROGRAM_ID, false, false}   // System Program
    },
    data
  );

  Transaction tx;
    tx.fee_payer = owner;
    tx.recent_blockhash = solana.getLatestBlockhash();
    if (tx.recent_blockhash.isEmpty()) {
        Serial.println("❌ Failed to get blockhash!");
        return false;
    }
    tx.add(ix);
    tx.sign({signer});
    String txBase64 = tx.serializeBase64();

    String txSig;
    if (solana.sendRawTransaction(txBase64, txSig)) {
        Serial.println("✅ Anchor tx sent! Signature: " + txSig);
    } else {
        Serial.println("❌ Anchor tx failed.");
        return false;
    }
    return true;
}

/*****************************************************************************************
* Function: Mint Rewards
*
* Description: 
* Parameters: None
* Returns: bool - True if transaction is successful, false otherwise
*****************************************************************************************/ 
bool mintRewards() {

  // Prepare instruction
  std::vector<uint8_t> discriminator = solana.calculateDiscriminator("mint_reward");

  // No payload (data = discriminator)

  Instruction ix(
    Pubkey{programId},
    std::vector<AccountMeta>{
      AccountMeta::signer(owner),
      AccountMeta::writable(accountPdaPubkey, false),
      AccountMeta{mintAuthorityPdaPubkey, false, false},
      AccountMeta::writable(mint, false),
      AccountMeta::writable(tokenAccount, false),
      AccountMeta{TOKEN_PROGRAM_ID, false, false},   // Token Program
      AccountMeta{SYSTEM_PROGRAM_ID, false, false}   // System Program
    },
    discriminator
  );

  Transaction tx;
    tx.fee_payer = owner;
    tx.recent_blockhash = solana.getLatestBlockhash();
    if (tx.recent_blockhash.isEmpty()) {
        Serial.println("❌ Failed to get blockhash!");
        return false;
    }
    tx.add(ix);
    tx.sign({signer});
    String txBase64 = tx.serializeBase64();

    String txSig;
    if (solana.sendRawTransaction(txBase64, txSig)) {
        Serial.println("✅ Anchor tx sent! Signature: " + txSig);
    } else {
        Serial.println("❌ Anchor tx failed.");
        return false;
    }
    return true;
}

/*****************************************************************************************
* Function: Initialize Display
*
* Description: Initializes the OLED display with I2C communication
* Parameters: None
* Returns: None
*****************************************************************************************/ 
void initializeDisplay() {
  // Initialize I2C
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("\n❌ OLED Display initialization failed!\n");
    return;
  }
  
  // Clear the display buffer
  display.clearDisplay();
  
  // Display welcome message
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("DePIN");
  display.println("Heartbeat");
  display.setTextSize(1);
  display.println("\nInitializing...");
  display.display();
  
  Serial.println("\n✅ OLED Display initialized successfully!\n");
}

/*****************************************************************************************
* Function: Display Heart Rate
*
* Description: Updates the OLED display with current heart rate data
* Parameters: heartRate - current heart rate value
* Returns: None
*****************************************************************************************/ 
void displayHeartRate(float heartRate) {
  display.clearDisplay();
  
  // Title
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Heartbeat Monitor");
  
  // Heart rate value - large text
  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print((int)heartRate);
  display.println(" BPM");
  
  // Heart pattern
  display.setTextSize(1);
  display.setCursor(30, 40);

  if (!heartRateDisplay) {
    display.print("__/\\  __");
    display.setCursor(30, 45);
    display.print("   \\/");
    display.display();
    heartRateDisplay = 1;
  } else {
    display.print(" ");
    heartRateDisplay = 0;
  }
  
  display.display();
}

/*****************************************************************************************
* Function: Display WiFi Status
*
* Description: Shows WiFi connection status on OLED
* Parameters: None
* Returns: None
*****************************************************************************************/ 
void displayWiFiStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("WiFi Status");
  display.println("");
  
  if (WiFi.status() == WL_CONNECTED) {
    display.println("Status: Connected");
    display.print("IP: ");
    display.println(WiFi.localIP());
  } else {
    display.println("Status: Connecting...");
  }
  
  display.display();
}

/*****************************************************************************************
* Function: Display Message
*
* Description: Displays any string message on the OLED display
* Parameters: message - string to display on OLED
* Returns: None
*****************************************************************************************/ 
void displayMessage(String message, int line) {
  if (line < 0) {
    line = 0;
  }
  if (line > 5) {
    line = 5;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, line*10);
  display.println(message);
  display.display();
}