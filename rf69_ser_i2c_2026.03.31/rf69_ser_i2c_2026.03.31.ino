// rf69_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple messageing server
// with the RH_RF69 class. RH_RF69 class does not provide for addressing or
// reliability, so you should only use RH_RF69  if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example rf69_client
// Demonstrates the use of AES encryption, setting the frequency and modem
// configuration.
// Tested on Moteino with RFM69 http://lowpowerlab.com/moteino/
// Tested on miniWireless with RFM69 www.anarduino.com/miniwireless
// Tested on Teensy 3.1 with RF69 on PJRC breakout board
// Tested on RFM69HCW (Adafruit) and Arduino Nano 33 BLE
// Modified 2026.03.24 for MIT 16.831 by Alex CHoi

#include <SPI.h>
#include <Wire.h>
#include <RH_RF69.h>

// Choose compatible frequency based on hardware
// DON'T USE THIS ONE FOR THE CLASS -------------
//#define RF69_FREQ 433.0  // https://www.adafruit.com/product/3071
// ----------------------------------------------
//#define RF69_FREQ 868.0  // https://www.adafruit.com/product/3072
#define RF69_FREQ 915.0  // https://www.adafruit.com/product/3072
#define RF69_PWR 20 // valid values between 14 dBi (lowest power) and 20 dBi (hightest power) inclusive

#define RFM69_CS    4  // Arduino D4
#define RFM69_INT   3  // Arduino D3
#define RFM69_RST   2  // Arduino D2
#define SMBUS_MAX_BYTES 32 // Maximum number of bytes per SMBus Packet in Unix

// // ---- Addressing (ADD THESE LINES) -------------------------------- // part 10
#define PREAMBLE    0xAA  // Magic byte — marks a valid packet
#define HEADER_LEN  3     // preamble(1) + dest_addr(1) + src_addr(1)
#define MY_ADDR     0x09  // <<< SET THIS to your address from the table
#define DEST_ADDR   0x0A  // <<< SET THIS to the other radio's address
// ----------------------------------------------

char dbglog[256];
uint8_t uplinkBuf[SMBUS_MAX_BYTES];  // 32 bytes
uint8_t dnlinkBuf[SMBUS_MAX_BYTES];  // 32 bytes
uint8_t pktBuf[HEADER_LEN + SMBUS_MAX_BYTES]; // RF packet = header + payload  // part 10

bool RFSend = false;
bool GotMsg = false;
bool UseBLE = false;

// Singleton instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT);
//RH_RF68 rf69;
//RH_RF69 rf69(15, 16); // For RF69 on PJRC breakout board with Teensy 3.1
//RH_RF69 rf69(4, 2); // For MoteinoMEGA https://lowpowerlab.com/shop/moteinomega
//RH_RF69 rf69(8, 7); // Adafruit Feather 32u4

// I2C configuration
#define SER_I2C_ADDR 0x43 // 0x43 is the address, aka decimal 67
#define SER_GOT_MSG 10 // Arduino D10 = Message ready pin --- HI = Message Available, LO = No Message
// RPi equivalent for input GPIO 4

void setup() 
{
  Serial.begin(9600);
  while (!Serial) delay(1); // Wait for Serial Console (comment out line if no computer)
  Serial.println("##################");

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
  Serial.println("RFM69 Init Successful");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
  // No encryption
  if (!rf69.setFrequency(RF69_FREQ))
    Serial.println("setFrequency failed");
  snprintf(dbglog, sizeof(dbglog), "RFM69 Set Frequency to %.1f MHz Sucessful!", RF69_FREQ);
  Serial.println(dbglog);


  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:  values can be between 14 and 20 inclusive
  rf69.setTxPower(RF69_PWR, true);
  snprintf(dbglog, sizeof(dbglog), "RFM69 Set Power to %d dBi Successful!", RF69_PWR);
  Serial.println(dbglog);

  // The encryption key has to be the same as the one in the client
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  rf69.setEncryptionKey(key);
  Serial.println("RFM69 Set Encryption Key");

/*
#if 0
  // For compat with RFM69 Struct_send
  rf69.setModemConfig(RH_RF69::GFSK_Rb250Fd250);
  rf69.setPreambleLength(3);
  uint8_t syncwords[] = { 0x2d, 0x64 };
  rf69.setSyncWords(syncwords, sizeof(syncwords));
  rf69.setEncryptionKey((uint8_t*)"thisIsEncryptKey");
#endif
*/

  Serial.println("RFM69 Server Setup Complete!");

  pinMode(SER_GOT_MSG, OUTPUT);
  Wire.begin(SER_I2C_ADDR);
  snprintf(dbglog, sizeof(dbglog), "RFM69 Server I2C Target Address set as %d", SER_I2C_ADDR);
  Serial.println(dbglog);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.println("Server I2C setup complete!");

  Serial.println("##################");
}


void loop()
{
  if (GotMsg) digitalWrite(SER_GOT_MSG, HIGH); // pull pin HI so Pi knows to poll 
  else digitalWrite(SER_GOT_MSG, LOW); // pull pin LO so Pi doesn't poll

  if (rf69.available()){  // received message via RFM69
    Serial.println("-----------------");
    Serial.println("RFM69 Got a message from Client!");
    // Should be a message for us now   
    // uint8_t len = sizeof(uplinkBuf); // hide part 10
    // if (rf69.recv(uplinkBuf, &len)){ // hide part 10
    //   Serial.print("<- "); 
    //   for (int i = 0; i < len; i++){
    //     Serial.print(uplinkBuf[i], HEX);
    //     Serial.print(",");
    //   }
    //   Serial.println();
    //   GotMsg = true;
    // }
    // NEW — receive into pktBuf, then check header: // add part 10
    uint8_t pktLen = sizeof(pktBuf);
    if (rf69.recv(pktBuf, &pktLen)){
        if (pktBuf[0] != PREAMBLE || pktBuf[1] != MY_ADDR){
            // Header check failed — not our packet, discard silently
            Serial.println("Not our packet -- discarding");
            // GotMsg stays false — loop continues
        } else {
            // Valid! Strip header, copy payload into uplinkBuf
            memcpy(uplinkBuf, pktBuf + HEADER_LEN, pktLen - HEADER_LEN);
            Serial.println("Valid packet received!");
            GotMsg = true;
        }
    }
    else Serial.println("recv failed");
  }

  // TODO add BLE receive if available (the send response via BLE too)

  if (RFSend && !UseBLE){  // RFM69 send response
    Serial.println("Sending a reply to Client via RFM69...");
    Serial.print("-> ");
    for (int i = 0; i < sizeof(dnlinkBuf); i++){
      Serial.print(dnlinkBuf[i], HEX);
      Serial.print(",");
    }
    Serial.println();
    // rf69.send(dnlinkBuf, sizeof(dnlinkBuf)); // hide part 10
    // rf69.waitPacketSent(); // hide part 10
    // NEW — build reply packet with header, then send: // add part 10
    pktBuf[0] = PREAMBLE;               // magic byte
    pktBuf[1] = DEST_ADDR;              // reply goes back to client
    pktBuf[2] = MY_ADDR;                // from us (the server)
    memcpy(pktBuf + HEADER_LEN, dnlinkBuf, sizeof(dnlinkBuf));
    rf69.send(pktBuf, sizeof(pktBuf));
    rf69.waitPacketSent();
    // ^ hide part 10
    Serial.println("Sent a reply");
    RFSend = false;
    Serial.println("-----------------");
  }

  if (RFSend && UseBLE){  // BLE send response

    RFSend = false;
    Serial.println("-----------------");
  }
}

void receiveEvent(int howMany){
  // Controller sending data to Target
  // Set GotMsg to false again for safety
  GotMsg = false;
  // Zero out the dnlink buffer
  Serial.println("*******");
  Serial.println("I2C Recevie Called!");
  memset(dnlinkBuf , 0, sizeof(dnlinkBuf));
  // put data in dnlinkBuf
  int numDnlinkBytes = Wire.available();
  Wire.read();
  numDnlinkBytes--;
  snprintf(dbglog, sizeof(dbglog), "Num bytes received %d", numDnlinkBytes);
  Serial.println(dbglog);
  if (numDnlinkBytes > SMBUS_MAX_BYTES) return; // eject with error
  Serial.print("I2C Message reads as: ");
  for (int i = 0; i < numDnlinkBytes; i++){
    int dataRx = Wire.read(); // Wire.read returns -1 if nothing to read
    Serial.print(dataRx, HEX);
    if (dataRx < 0) return; // eject if error
    Serial.print(",");
    dnlinkBuf[i] = dataRx;
  }
  Serial.println();
  RFSend = true;
  Serial.println("*******");
}

void requestEvent(){
  // Controller requesting data from Target
  // Set GotMsg to False
  Serial.println("*******");
  Serial.println("I2C Request Called!");
  GotMsg = false;
  Serial.println("uplinkBuf contains: ");
  for (int i = 0; i < sizeof(uplinkBuf); i++){
    Serial.print(uplinkBuf[i], HEX);
    Serial.print(",");
  }
  Serial.println();
  // Read data from uplinkBuf & send requested num bytes to the Controller
  Wire.write(uplinkBuf, sizeof(uplinkBuf));
  Serial.println("contents of uplinkBuf sent to Controller");
  // DO NOT clear the uplinkBuf (so data isn't destroyed if duplicate needed)
  Serial.println("*******");
}

