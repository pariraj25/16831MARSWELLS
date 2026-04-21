// rf69_client.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messageing client
// with the RH_RF69 class. RH_RF69 class does not provide for addressing or
// reliability, so you should only use RH_RF69  if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example rf69_server.
// Demonstrates the use of AES encryption, setting the frequency and modem 
// configuration
// Tested on Moteino with RFM69 http://lowpowerlab.com/moteino/
// Tested on miniWireless with RFM69 www.anarduino.com/miniwireless
// Tested on Teensy 3.1 with RF69 on PJRC breakout board
// Tested on RFM69HCW (Adafruit) and Arduino Nano 33 BLE
// Modified 2026.03.24 for MIT 16.831 by Alex Choi

#include <SPI.h>
#include <Wire.h>
#include <RH_RF69.h>

// Choose compatible frequency based on hardware
// DON'T USE THIS ONE FOR THE CLASS ---------
//#define RF69_FREQ 433.0  // https://www.adafruit.com/product/3071
// ------------------------------------------
// #define RF69_FREQ 868.0  // https://www.adafruit.com/product/3072
#define RF69_FREQ 915.0  // https://www.adafruit.com/product/3072
#define RF69_PWR 20 // valid values between 14 dBi (lowest power) and 20 dBi (highest power) inclusive
#define RF69_TIMEOUT_MS 500 // 500ms timeout waiting for reply from server
#define SMBUS_MAX_BYTES 32 // Maximum number of bytes than can be used per packet by Unix

#define RFM69_CS    4  // Arduino D4
#define RFM69_INT   3  // Arduino D3
#define RFM69_RST   2  // Arduino D2

// // ---- Addressing (ADD THESE LINES) -------------------------------- // part 10
#define PREAMBLE    0xAA  // Magic byte — marks a valid packet
#define HEADER_LEN  3     // preamble(1) + dest_addr(1) + src_addr(1)
#define MY_ADDR     0x0A  // <<< SET THIS to your address from the table
#define DEST_ADDR   0x09  // <<< SET THIS to the other radio's address
// ----------------------------------------------

char dbglog[256];
uint8_t uplinkBuf[SMBUS_MAX_BYTES];  // 32 bytes
uint8_t dnlinkBuf[SMBUS_MAX_BYTES];  // 32 bytes
uint8_t pktBuf[HEADER_LEN + SMBUS_MAX_BYTES]; // RF packet = header + payload  // part 10

bool RFSend = false;
bool UseBLE = false;
bool GotMsg = false;
bool LED_on = false;

// Singleton instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT); 
//RH_RF69 rf69;
//RH_RF69 rf69(15, 16); // For RF69 on PJRC breakout board with Teensy 3.1
//RH_RF69 rf69(4, 2); // For MoteinoMEGA https://lowpowerlab.com/shop/moteinomega
//RH_RF69 rf69(8, 7); // Adafruit Feather 32u4

// I2C configuration
#define CLI_I2C_ADDR 0x42 // 0x42 is the address, aka decimal 66
#define CLI_GOT_MSG 10 // Arduino D10 = Message ready pin --- HI = Message Available, LO = No Message
// RPI equivalent for input is GPIO 4

void setup() 
{
  Serial.begin(9600);
  while (!Serial) delay(1); // Wait for Serial Console (comment out if no computer)
  Serial.println("#################");

  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);
  Serial.println("RFM69 PinMode Set");
  
  // manual reset
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);
  Serial.println("RFM69 Pins Reset");

  if (!rf69.init())
    Serial.println("init failed");
  Serial.println("RFM69 Init Successful!");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
  // No encryption
  if (!rf69.setFrequency(RF69_FREQ))
    Serial.println("setFrequency failed");
  snprintf(dbglog, sizeof(dbglog), "RFM69 Set Frequency to %.1f MHz Sucessful!", RF69_FREQ);
  Serial.println(dbglog);

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(RF69_PWR, true);
  snprintf(dbglog, sizeof(dbglog), "RFM69 Set Power to %d dBi Successful!", RF69_PWR);
  Serial.println(dbglog);

  // The encryption key has to be the same as the one in the server
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  rf69.setEncryptionKey(key);
  Serial.println("RFM69 Set Encryption Key");
  Serial.println("RFM69 Client Setup Complete!");

  pinMode(CLI_GOT_MSG, OUTPUT);
  Wire.begin(CLI_I2C_ADDR);
  snprintf(dbglog, sizeof(dbglog), "RFM69 Client I2C Target Address set as %d", CLI_I2C_ADDR);
  Serial.println(dbglog);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.println("Client I2C setup complete!");

  Serial.println("#################");
}

void loop()
{
  if (GotMsg) digitalWrite(CLI_GOT_MSG, HIGH); // pull pin HI so Pi knows to poll because received RF message
  else digitalWrite(CLI_GOT_MSG, LOW); // pull pin LOW so Pi doesn't poll because no RF message

  if (RFSend && !UseBLE){
    Serial.println("-----------------");
    Serial.println("Sending to rf69_server");
    // Send a message to rf69_server
    Serial.println("uplinkBuf contains: ");
    Serial.print("-> ");
    for (int j = 0; j < sizeof(uplinkBuf); j++){
      Serial.print(uplinkBuf[j], HEX);
      Serial.print(",");
    }
    Serial.println();
    // rf69.send(uplinkBuf, sizeof(uplinkBuf));  // hide for part 10
    // Serial.println("Data sending...");        // hide for part 10
    // rf69.waitPacketSent();                    // hide for part 10
    // NEW — build packet with header, then send:                             // add for part 10
    pktBuf[0] = PREAMBLE;               // magic byte                         // add for part 10
    pktBuf[1] = DEST_ADDR;              // who this is for (the server)       // add for part 10
    pktBuf[2] = MY_ADDR;                // who sent it (us, the client)       // add for part 10
    memcpy(pktBuf + HEADER_LEN, uplinkBuf, sizeof(uplinkBuf));                // add for part 10
    rf69.send(pktBuf, sizeof(pktBuf));                                        // add for part 10
    Serial.println("Data sending...");                                        // add for part 10
    rf69.send(txBuf, sizeof(txBuf));
    rf69.waitPacketSent();                                                    // add for part 10
    Serial.println("Data sent");
    RFSend = false;
    // Now wait for a reply
    uint8_t len = sizeof(dnlinkBuf);

    if (rf69.waitAvailableTimeout(RF69_TIMEOUT_MS)){ 
      // Should be a reply message for us now   
      // if (rf69.recv(dnlinkBuf, &len)){ // overwrite the dnlinkBuf     // hide this if for part 10
      //   Serial.println("got reply: ");
      //   Serial.print("<- ");
      //   for (int i = 0; i < len; i++){
      //     Serial.print(dnlinkBuf[i], HEX);
      //     Serial.print(",");
      //   }
      //   Serial.print("\n");
      //   GotMsg = true;
      // }
      // NEW — receive into pktBuf, then check header:
      uint8_t pktLen = sizeof(pktBuf);                                // add for part 10
      // if (rf69.recv(pktBuf, &pktLen)){                                // add this if for part 10
      //     if (pktBuf[0] != PREAMBLE || pktBuf[1] != MY_ADDR){
      //         // Header check failed — not our packet, discard
      //         Serial.println("Packet not addressed to us -- discarding");
      //         GotMsg = false;
      //     } else {
      //         // Valid! Strip header, copy payload into dnlinkBuf
      //         memcpy(dnlinkBuf, pktBuf + HEADER_LEN, pktLen - HEADER_LEN);
      //         Serial.println("Valid reply received!");
      //         GotMsg = true;
      //     }
      // }
      // else{
      //   Serial.println("recv failed");
      //   GotMsg = false;
      // }
      // ── Receive into a 36-byte buffer (35 header+payload + 1 checksum) ─────
      uint8_t rxBuf[sizeof(pktBuf) + 1];
      uint8_t rxLen = sizeof(rxBuf);
      if (rf69.recv(rxBuf, &rxLen)) {
        // Step 1: verify XOR checksum (all 36 bytes XOR'd must equal 0)
        uint8_t verify = 0;
        for (int i = 0; i < rxLen; i++) {
          verify ^= rxBuf[i];
        }
        if (verify != 0) {
          Serial.println("BAD CHECKSUM — corrupted packet, discarding");
          // Treated same as lost packet — do nothing, loop continues
        }
        // Step 2: address check (from Part 10)
        else if (rxBuf[0] != PREAMBLE || rxBuf[1] != MY_ADDR) {
          Serial.println("Not our packet — discarding");
        }
        // Step 3: valid! strip header and checksum, copy payload
        else {
          memcpy(dnlinkBuf, rxBuf + HEADER_LEN, SMBUS_MAX_BYTES);
          GotMsg = true;
          Serial.println("Valid packet received");
        }
      }
      // ─────────────────────────────────────────────────────────────────────
    }
    else{
      Serial.println("No reply, is rf69_server running?");
      GotMsg = false;
    }
    Serial.println("-----------------");
  }

  if (RFSend && UseBLE){  // TODO, send via BLE

  }
  delay(400);
}


void receiveEvent(int howMany){
  // Controller sending data to Target
  // Zero out the uplink buffer
  Serial.println("*******");
  Serial.println("I2C Receive Called!");
  memset(uplinkBuf, 0, sizeof(uplinkBuf));
  // Put data in uplinkBuf
  int numUplinkBytes = Wire.available();
  Wire.read();
  numUplinkBytes--;
  snprintf(dbglog, sizeof(dbglog), "Num bytes received %d", numUplinkBytes);
  Serial.println(dbglog);
  if (numUplinkBytes > SMBUS_MAX_BYTES) return; // eject with error
  Serial.print("I2C Msg reads as: ");
  for (int i = 0; i < numUplinkBytes; i++){
    int dataRx = Wire.read(); // Wire.read returns -1 if nothing to read
    Serial.print(dataRx, HEX);
    Serial.print(",");
    if (dataRx < 0){
      Serial.print("Wire Read Error!!");
      return; // eject if error
    } 
    // TODO add unique key (NOT 0x00) that determines
    // if you use BLE or RFM69
    /*
    if (...){
      UseBLE = true;
      continue;
    } 
    else if (...){
      UseBLE = false;
      continue;
    } 
    */
    // subsequent bytes are all data to send
    uplinkBuf[i] = (uint8_t)dataRx;
  }
  Serial.println();
  // Set RFSend flag True
  RFSend = true;
  // Let the loop do the rest
  Serial.println("*******");
}

void requestEvent(){
  // Controller requesting data from Target
  // Set GotMsg to False
  Serial.println("*******");
  Serial.println("I2C Request Called!");
  GotMsg = false;
  Serial.print("dnlinkBuf contains: ");
  for (int i = 0; i < sizeof(dnlinkBuf); i++){
    Serial.print(dnlinkBuf[i], HEX);
    Serial.print(",");
  }
  Serial.println();
  // Read data from dnlinkBuf & send requested num bytes to the Controller
  Wire.write(dnlinkBuf, sizeof(dnlinkBuf));
  // DO NOT clear the dnlinkBuf (so data isn't destroyed if duplicate needed)
  Serial.println("*******");
}
