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
#include "pins_arduino.h"
#include <Arduino.h>
#include <WiFi.h>
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
int heartRateCount = 0;
#define HEART_RATE_TIME_MS 50
unsigned long lastHeartRateTime = 0;
#define HEART_RATE_SEND_TIME_MS 6000
unsigned long lastHeartRateSendTime = 0;


// Solana Configuration : see credentials.h
// Initialize Solana Library
IoTxChain solana(SOLANA_RPC_URL);
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
// prepare solana accounts
const int maxAttempts = 3;
int attempt = 0;
bool pdaSuccess = false;

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
void sendHeartRateReading(float heartRate);

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
  
  // connect to WiFi
  connectToWiFi();
  
  while (!pdaSuccess && attempt < maxAttempts) {
    pdaSuccess = prepareSolanaAccounts();
    if (!pdaSuccess) {
      Serial.println("⏳ Retrying PDA calculation... (Attempt " + String(attempt + 1) + "/" + String(maxAttempts) + ")");
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
      Serial.println("\n== ❤️  Heart Rate ❤️  ==");
      heartRateHeaderPrinted = true;
    }
    
    // Add current reading
    Serial.print(String((int)heartRateAverage) + " ... ");
    heartRateCount++;
    if (heartRateCount >= 6) {
      Serial.print("\n");
      heartRateCount = 0;
    }
  }

  // send heart rate reading
  if ((timeMs - lastHeartRateSendTime > HEART_RATE_SEND_TIME_MS) && pdaSuccess) {
    heartRateHeaderPrinted = false;
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, LOW);
    lastHeartRateSendTime = timeMs;
    sendHeartRateReading(heartRateAverage);
    Serial.print("Time to send transaction: " + String(millis()-lastHeartRateSendTime) + "ms\n");
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
      Serial.println(readableBalance, 8);
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
  Serial.println("\n=== Heartbeat Account PDA ===");
  Serial.println(vectorToHex(accountPdaPubkey.data));
  
  Serial.println("\n=== Mint Authority PDA ===");
  Serial.println(vectorToHex(mintAuthorityPdaPubkey.data));
  
  Serial.println("\n=== Associated Token Account ===");
  Serial.println(tokenAccountAddress);
}

/*****************************************************************************************
* Function: Send Heart Rate Reading
*
* Description: 
* Parameters: None
* Returns: bool - True if transaction is successful, false otherwise
*****************************************************************************************/ 
void sendHeartRateReading(float heartRate) {

  Serial.println("\n=== Sending Heart Rate Reading ===");

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
      AccountMeta{mintAuthorityPdaPubkey, false, false},
      AccountMeta::writable(mint, false),
      AccountMeta::writable(tokenAccount, false),
      AccountMeta{TOKEN_PROGRAM_ID, false, false},   // Token Program
      AccountMeta{SYSTEM_PROGRAM_ID, false, false}   // System Program
    },
    data
  );

  Transaction tx;
    tx.fee_payer = owner;
    tx.recent_blockhash = solana.getLatestBlockhash();
    if (tx.recent_blockhash.isEmpty()) {
        Serial.println("❌ Failed to get blockhash!");
        return;
    }
    tx.add(ix);
    tx.sign({signer});
    String txBase64 = tx.serializeBase64();

    String txSig;
    if (solana.sendRawTransaction(txBase64, txSig)) {
        Serial.println("✅ Anchor tx sent! Signature: " + txSig);
    } else {
        Serial.println("❌ Anchor tx failed.");
    }

}