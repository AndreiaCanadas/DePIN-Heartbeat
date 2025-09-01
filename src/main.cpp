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

/*****************************************************************************************
* Function Declarations
*****************************************************************************************/ 
void connectToWiFi();
void blinkRgbLed();
void getSolanaBalance(const char* publicKey);


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

  // get Solana balance
  getSolanaBalance(PUBLIC_KEY);

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
* Function: Get Solana balance
*
* Description: Gets the Solana balance of a wallet address and prints it to the serial monitor
* Parameters: 
*   publicKey - Wallet public key to get the balance of
* Returns: None
*****************************************************************************************/ 
void getSolanaBalance(const char* publicKey) {

  Serial.println("\nGetting Solana balance...");

  uint64_t lamports = 0;
  if (solana.getSolBalance(publicKey, lamports)) {
    Serial.print("SOL Balance (SOL): ");
    Serial.println((float)lamports / 1e9, 9);
    Serial.println();
  } else {
    Serial.println("Failed to get Solana balance\n");
  }

}

/*****************************************************************************************
* Add Your Custom Functions Here
*****************************************************************************************/ 

