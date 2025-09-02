/*****************************************************************************************
 * ESP32 DePIN Template
 * 
 * A clean template for ESP32 + Solana blockchain integration
 * 
 * Features:
 * - WiFi connection management
 * - Solana wallet integration with IoTxChain library
 * - LED status indicators (built-in + RGB)
 * - Balance checking functionality
 * - Example transaction function
 * 
 * Setup:
 * 1. Configure credentials in include/credentials.h
 * 2. Add your custom sensors/functionality
 * 3. Extend the main loop and functions as needed
 * 
 *****************************************************************************************/

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

// Solana Configuration : see credentials.h
// Initialize Solana Library
IoTxChain solana(SOLANA_RPC_URL);
Pubkey TOKEN_PROGRAM_ID = Pubkey::fromBase58("TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
Pubkey SYSTEM_PROGRAM_ID = Pubkey::fromBase58("11111111111111111111111111111111");

/*****************************************************************************************
* Function Declarations
*****************************************************************************************/ 
void connectToWiFi();
void blinkRgbLed();
void printSplTokenBalance();
bool sendHeartRateReading(float heartRate);



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
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, LOW);
  
  // connect to WiFi
  connectToWiFi();

  // print SPL token balance of user
  printSplTokenBalance();

}

/*****************************************************************************************
* Main Loop
*****************************************************************************************/  
void loop() {
  timeMs = millis();

  // blink LED
  if (timeMs - lastBlinkTime > BLINK_TIME_MS) {
    // Option 1: Simple built-in LED toggle
    ledStatus = !ledStatus;
    digitalWrite(LED_BUILTIN, ledStatus);

    // Option 2: RGB LED cycling
    blinkRgbLed();

    lastBlinkTime = timeMs;

    // send heart rate reading
    if (sendHeartRateReading(100.0)) {
      printSplTokenBalance();
      //Serial.println("✅ Heart rate reading sent successfully");
    } else {
      //Serial.println("❌ Failed to send heart rate reading");
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
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
    ledOn = 1;
  } else if (ledOn == 1) { // turn on LED_GREEN
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
    ledOn = 2;
  } else if (ledOn == 2) { // turn on LED_RED
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
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
}

/*****************************************************************************************
* Function: Send Heart Rate Reading
*
* Description: 
* Parameters: None
* Returns: bool - True if transaction is successful, false otherwise
*****************************************************************************************/ 
bool sendHeartRateReading(float heartRate) {

  Serial.println("\n=== Sending Heart Rate Reading ===");

  // Prepare accounts and signer
  Pubkey owner = Pubkey::fromBase58(PUBLIC_KEY);
  Keypair signer = Keypair::fromPrivateKey(PRIVATE_KEY);
  Pubkey mint = Pubkey::fromBase58(TOKEN_MINT);
  
  // Prepare program ID
  std::vector<uint8_t> programId = base58ToPubkey(PROGRAM_ID);

  // Find Heartbeat Account PDA
  std::vector<uint8_t> accountPda;
  // Prepare seeds: 
  std::vector<std::vector<uint8_t>> seeds = {
    {'h','e','a','r','t','b','e','a','t'},
    base58ToPubkey(PUBLIC_KEY)
  };
  uint8_t bump;
  if (!solana.findProgramAddress(seeds, programId, accountPda, bump)) {
    Serial.println("❌ Failed to find program address for Heartbeat Account!");
    return false;
  }
  Pubkey accountPdaPubkey;
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
  Pubkey mintAuthorityPdaPubkey;
  mintAuthorityPdaPubkey.data = mintAuthorityPda;

  // Find Associated Token Account
  String ata;
  if (solana.findAssociatedTokenAccount(PUBLIC_KEY, TOKEN_MINT, ata)) {
      Serial.print("Associated Token Account: ");
      Serial.println(ata);
  } else {
      Serial.println("❌ Failed to find ATA.");
  }
  Pubkey tokenAccount = Pubkey::fromBase58(ata);

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
    }

  return true;
}