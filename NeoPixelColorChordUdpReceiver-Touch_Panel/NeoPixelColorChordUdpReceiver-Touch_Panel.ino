#include <Arduino.h>
#include <math.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#include <ESP8266mDNS.h>
#include <Adafruit_ILI9341esp.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <Ticker.h>
#include <XPT2046.h>
#include "HideableButton.h"
#include "TouchSlider.h"
#include "ColorChord.h"

// Modify the following two lines to match your hardware
// Also, update calibration parameters below, as necessary

// For the esp shield, these are the default.
#define TFT_DC 2
#define TFT_CS 4

bool inManualMode = false;

/* Touch display stuff */
#define SLIDER_AMP_ACTIVE 0
#define SLIDER_VU_ACTIVE  1
#define SLIDER_NOTE_MIN_ACTIVE  2
#define SLIDER_NOTE_GONE_ACTIVE 3

#define BUTTON_AMP        0
#define BUTTON_VU         1
#define BUTTON_MORE       2
#define BUTTON_MODE       3
#define BUTTON_MODE_CC    4
#define BUTTON_MODE_CHASE 5
#define BUTTON_MODE_ON    6
#define BUTTON_MODE_CC1   7
#define BUTTON_MODE_OFF   8
#define BUTTON_NOTE_MIN   9
#define BUTTON_NOTE_GONE  10
#define BUTTON_MASTER     11
HideableButton button[12];

#define MENU_MAIN         1
#define MENU_MORE         2
#define MENU_MODE         3

#define MANUAL_MODE_SOLID 0
#define MANUAL_MODE_CHASE 1
#define MANUAL_MODE_ALTERNATE 2
#define MANUAL_MODE_OFF   3

int current_menu = MENU_MAIN;
int manualMode = MANUAL_MODE_CHASE;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
XPT2046 touch(/*cs=*/ 16, /*irq=*/ 0);

TouchSlider sliderControl;

int sliderX = 30;
int sliderY = 110;
int sliderWidth = 260;

int initialValue = 160;

int activeSlider = -1;
uint16_t x, y, xPrev;
int sliderValue = 0;
int sliderCursorX = 0;
char sliderValueStr[5];
/* End Touch display stuff */

/* WiFi config */
const char *ssid = "GL-MT300N";
const char* pass = "0717640717";
// we'll try to fall back to connecting directly to the colorchord AP, since it will revert to AP mode if it can't connect to the above
const char *esp_ssid = "ESP_451800";
/* END WiFi config stuff */

IPAddress broadcastIp = IPAddress(255,255,255,255);
unsigned int udpBroadcastPort = 5000;      // port to boradcast UDP packets on when in broadcast mode
char sampBuf[5];

// VU settings
bool ledCountDetermined = false;
int numLeds = 150;
int vuLevels[50];
int ledArray[150];
double vuMult = 5.0;
const int vuPadding = 20;
int vuWidth;
const int maxVuY = 150;

// Start byte (address 0) of the EEPROM
int epromAddr = 0;
byte value;


Ticker displayTicker;
Ticker menuTicker;
Ticker mdnsTicker;

void setup() {
  Serial.begin(115200);
  SPI.setFrequency(ESP_SPI_FREQ);
  EEPROM.begin(512);
  vuMult = EEPROM.read(epromAddr) + EEPROM.read(epromAddr+1)/10.0;
  
  tft.begin();
  touch.begin(tft.width(), tft.height());  // Must be done before setting rotation
  tft.setRotation(1);
  Serial.print("tftx ="); Serial.print(tft.width()); Serial.print(" tfty ="); Serial.println(tft.height());
  vuWidth = tft.width() - vuPadding * 2;
  tft.fillScreen(ILI9341_BLACK);
  // Replace these for your screen module
  touch.setCalibration(189, 1739, 1736, 275);
  touch.setRotation(touch.ROT90);

  //Adafruit_GFX *gfx, int16_t x1, int16_t y1, uint16_t w, uint16_t h, uint16_t outline, uint16_t fill, uint16_t textcolor, char *label, uint8_t textsize
  button[BUTTON_AMP].initButton(&tft, 60, 30, 100, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "Sensitiv", 2, true);
  button[BUTTON_MORE].initButton(&tft, 170, 30, 80, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "More", 2, true);
  button[BUTTON_MODE].initButton(&tft, 270, 30, 80, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "Mode", 2, true);
  button[BUTTON_VU].initButton(&tft, 60, 30, 100, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "VU", 2, false);
  button[BUTTON_NOTE_MIN].initButton(&tft, 170, 30, 80, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "A Min", 2, false);
  button[BUTTON_NOTE_GONE].initButton(&tft, 270, 30, 80, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "A Gone", 2, false);
  button[BUTTON_MODE_CC].initButton(&tft, 50, 80, 80, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "CC", 2, false);
  button[BUTTON_MODE_ON].initButton(&tft, 170, 80, 80, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "All On", 2, false);
  button[BUTTON_MODE_CHASE].initButton(&tft, 270, 80, 80, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "Chase", 2, false);
  button[BUTTON_MODE_CC1].initButton(&tft, 60, 130, 100, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "CC All", 2, false);
  button[BUTTON_MODE_OFF].initButton(&tft, 170, 130, 80, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "Off", 2, false);
  button[BUTTON_MASTER].initButton(&tft, 270, 130, 100, 40, ILI9341_DARKCYAN, ILI9341_BLUE, ILI9341_GREENYELLOW, "Master", 2, false);
  
  showMainMenu();

  sliderControl.init(&tft, "", sliderX, sliderY, sliderWidth, 0, sliderValCallback, sliderDoneCallback);

  // setting up Station AP
  sprintf(hostString, "ESP_%04X", ESP.getChipId() & 0xFFFF);

  initWiFi();
  initColorChordClient();
  connectUDP();

  sendCommand(CMD_REQ_VARS, NULL);

  displayTicker.attach(0.1, tickVuMeter);
}

void loop() {

  handleUdpCommands();

  if (touch.isTouching()) {
//    Serial.print(" x ="); Serial.print(x); Serial.print(" y ="); Serial.println(y);
    touch.getPosition(x, y);
    checkButtons(x, y);
  } 
  sliderControl.notify(x, y);

  if (Serial.available() > 0) {
    byte incomingByte = Serial.read();
    if (incomingByte == (byte)'s') {
      sliderControl.show();
    }
  }
}



void checkButtons(int x, int y) {
  for (int i = 0; i < 12; i++) {
    button[i].press(button[i].contains(x, y));
  }
  
  // now we can ask the buttons if their state has changed
  if (button[BUTTON_MORE].justPressed()) {
    hideMainMenu();
    showMoreMenu();
    menuTicker.once(2, cancelMoreMenu);
  }
  if (button[BUTTON_AMP].justPressed()) {
    activeSlider = SLIDER_AMP_ACTIVE;
    clearVu();
    button[BUTTON_AMP].drawButton(true); // draw invert!
    sliderControl.hide();
    sliderControl.setValue(gINITIAL_AMP & 0xFF);
    sliderControl.setLabel("Sensitiv");
    sliderControl.show();
  } else if (button[BUTTON_VU].justPressed()) {
    activeSlider = SLIDER_VU_ACTIVE;
    clearVu();
    button[BUTTON_VU].drawButton(true);
    sliderControl.hide();
    sliderControl.setValue(((int)(vuMult * 10.0)) & 0xFF);
    sliderControl.setLabel(" VU ");
    sliderControl.show();
  } else if (button[BUTTON_NOTE_MIN].justPressed()) {
    activeSlider = SLIDER_NOTE_MIN_ACTIVE;
    clearVu();
    button[BUTTON_NOTE_MIN].drawButton(true);
    sliderControl.hide();
    sliderControl.setValue(gMIN_AMP_FOR_NOTE & 0xFF);
    sliderControl.setLabel("Min note active");
    sliderControl.show();
  } else if (button[BUTTON_NOTE_GONE].justPressed()) {
    activeSlider = SLIDER_NOTE_GONE_ACTIVE;
    clearVu();
    button[BUTTON_NOTE_GONE].drawButton(true);
    sliderControl.hide();
    sliderControl.setValue(gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR & 0xFF);
    sliderControl.setLabel("Min note active");
    sliderControl.show();
  } else if (button[BUTTON_MODE].justPressed()) {
    showModesMenu();
    menuTicker.once(3, cancelModeMenu);
  } else if (button[BUTTON_MODE_CC].justPressed()) {
    inManualMode = false;
    gCOLORCHORD_OUTPUT_DRIVER = 0;
    showModesMenu();
    sendCommand(CMD_MODE_CC, NULL);
    menuTicker.once(2, cancelModeMenu);
    displayTicker.attach(0.1, tickVuMeter);
  } else if (button[BUTTON_MODE_CC1].justPressed()) {
    inManualMode = false;
    gCOLORCHORD_OUTPUT_DRIVER = 1;
    showModesMenu();
    sendCommand(CMD_MODE_CC1, NULL);
    menuTicker.once(2, cancelModeMenu);
  } else if (button[BUTTON_MODE_CHASE].justPressed()) {
    gCOLORCHORD_OUTPUT_DRIVER = 2;
    showModesMenu();
    manualMode = MANUAL_MODE_CHASE;
    if (inManualMode) initializeManual();
    sendCommand(CMD_MODE_CHASE, NULL);
    menuTicker.once(2, cancelModeMenu);
  } else if (button[BUTTON_MODE_ON].justPressed()) {
    gCOLORCHORD_OUTPUT_DRIVER = 3;
    showModesMenu();
    manualMode = MANUAL_MODE_SOLID;
    if (inManualMode) initializeManual();
    sendCommand(CMD_MODE_ON, NULL);
    menuTicker.once(2, cancelModeMenu);
  } else if (button[BUTTON_MODE_OFF].justPressed()) {
    gCOLORCHORD_OUTPUT_DRIVER = 4;
    showModesMenu();
    manualMode = MANUAL_MODE_OFF;
    if (inManualMode) initializeManual();
    sendCommand(CMD_MODE_OFF, NULL);
    menuTicker.once(2, cancelModeMenu);
  } else if ( button[BUTTON_MASTER].justPressed()) {
    inManualMode = true;
    initializeManual();
    clearVu(); 
    showModesMenu();
    menuTicker.once(2, cancelModeMenu);
  }
}

void initializeManual() {
  displayTicker.detach();
  if (manualMode == MANUAL_MODE_CHASE) {
Serial.println("MANUAL_MODE_CHASE");
    loadRainbow();
    displayTicker.attach(0.02, rotateLeds);
  } else if (manualMode == MANUAL_MODE_ALTERNATE) {
Serial.println("MANUAL_MODE_ALTERNATE");
    loadSolid();
    displayTicker.attach(0.5, alternateLeds);        
  } else if (manualMode == MANUAL_MODE_SOLID) {
Serial.println("MANUAL_MODE_SOLID");
    loadSolid();
    displayTicker.attach(0.5, sendLedBuffer);        
  } else if (manualMode == MANUAL_MODE_OFF) {
Serial.println("MANUAL_MODE_OFF");
    loadOff();
    displayTicker.attach(0.5, sendLedBuffer);        
  }
}
void showMainMenu() {
Serial.println("showMainMenu()");
  displayMode();
  current_menu = MENU_MAIN;
  button[BUTTON_AMP].setVisible(true);
  button[BUTTON_MORE].setVisible(true);
  button[BUTTON_MODE].setVisible(true);
  button[BUTTON_AMP].drawButton(false);
  button[BUTTON_MORE].drawButton(false);
  button[BUTTON_MODE].drawButton(false);
}

void showMoreMenu() {
Serial.println("showMoreMenu()");
  current_menu = MENU_MORE;
  hideMainMenu();
  button[BUTTON_VU].setVisible(true);
  button[BUTTON_NOTE_MIN].setVisible(true);
  button[BUTTON_NOTE_GONE].setVisible(true);
  button[BUTTON_VU].drawButton(false);
  button[BUTTON_NOTE_MIN].drawButton(false);
  button[BUTTON_NOTE_GONE].drawButton(false);
}

void showModesMenu() {
Serial.println("showModesMenu()");
  current_menu = MENU_MODE;
  displayMode();
  button[BUTTON_MODE_CC].setVisible(true);
  button[BUTTON_MODE_CHASE].setVisible(true);
  button[BUTTON_MODE_ON].setVisible(true);
  button[BUTTON_MODE_CC1].setVisible(true);
  button[BUTTON_MODE_OFF].setVisible(true);
  button[BUTTON_MASTER].setVisible(true);
  if (inManualMode) {
    button[BUTTON_MODE_CC].drawButton(false);
    button[BUTTON_MODE_CC1].drawButton(false);
    button[BUTTON_MODE_CHASE].drawButton(manualMode == MANUAL_MODE_CHASE);
    button[BUTTON_MODE_ON].drawButton(manualMode == MANUAL_MODE_SOLID);
    button[BUTTON_MODE_OFF].drawButton(manualMode == MANUAL_MODE_OFF);
    button[BUTTON_MASTER].drawButton(true);
  } else {
    button[BUTTON_MODE_CC].drawButton(gCOLORCHORD_OUTPUT_DRIVER == 0);
    button[BUTTON_MODE_CC1].drawButton(gCOLORCHORD_OUTPUT_DRIVER == 1);
    button[BUTTON_MODE_CHASE].drawButton(gCOLORCHORD_OUTPUT_DRIVER == 2);
    button[BUTTON_MODE_ON].drawButton(gCOLORCHORD_OUTPUT_DRIVER == 3);
    button[BUTTON_MODE_OFF].drawButton(gCOLORCHORD_OUTPUT_DRIVER == 4);
    button[BUTTON_MASTER].drawButton(false);
  }
}

void hideMainMenu() {
Serial.println("hideMainMenu()");
  activeSlider == -1;
  sliderControl.hide();
  button[BUTTON_AMP].hide();
  button[BUTTON_MORE].hide();
  button[BUTTON_MODE].hide();
}

void cancelMoreMenu() {
Serial.println("cancelMoreMenu()");
  if (! activeSlider > -1) { // Only cancel if levels are not still being adjusted
    button[BUTTON_VU].hide();
    button[BUTTON_NOTE_MIN].hide();
    button[BUTTON_NOTE_GONE].hide();
    showMainMenu();  
  } else {  // otherwise, extend ticker check
    menuTicker.once(1, cancelMoreMenu);
  }
}

void cancelModeMenu() {
Serial.println("cancelModeMenu()");
  button[BUTTON_MODE_CC].hide();
  button[BUTTON_MODE_CHASE].hide();
  button[BUTTON_MODE_ON].hide();
  button[BUTTON_MODE_CC1].hide();
  button[BUTTON_MODE_OFF].hide();
  button[BUTTON_MASTER].hide();
  cancelMoreMenu();
  showMainMenu();  
}

void sliderValCallback(int value) {
  Serial.printf("valCallback() got: %d\n", value);
  if (activeSlider == SLIDER_AMP_ACTIVE) {
    gINITIAL_AMP = value;
    sendCommand(CMD_AMP_MULT, value);
  } else if (activeSlider == SLIDER_NOTE_MIN_ACTIVE) {
    gMIN_AMP_FOR_NOTE = value;
    sendCommand(CMD_NOTE_MIN, value);
  } else if (activeSlider == SLIDER_NOTE_GONE_ACTIVE) {
    gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR = value;
    sendCommand(CMD_NOTE_GONE, value);
  } else if (activeSlider == SLIDER_VU_ACTIVE) {
    vuMult = (value / 10.0);
    Serial.printf("VU Mult: %f\n", vuMult);
    tft.drawFastVLine(vuPadding, 230 - maxVuY, maxVuY, ILI9341_BLACK);
    tft.drawFastVLine(vuPadding + 1, 230 - maxVuY, maxVuY, ILI9341_BLACK);
    tft.drawFastVLine(vuPadding, 230 - (5.8 * vuMult), (5.8 * vuMult), ILI9341_RED);
    tft.drawFastVLine(vuPadding + 1, 230 - (5.8 * vuMult), (5.8 * vuMult), ILI9341_RED);
  }
}

void sliderDoneCallback() {
  Serial.printf("sliderDoneCallback\n");
  if (activeSlider == SLIDER_VU_ACTIVE) {
    uint8_t vuMultP1 = vuMult;
    uint8_t vuMultP2 = ((int)((vuMult*10))%10);
  //Serial.printf("Store vuMult : %d . %d\n", vuMultP1, vuMultP2);
    EEPROM.write(epromAddr, vuMultP1);
    EEPROM.write(epromAddr+1, vuMultP2);
    EEPROM.commit();
  }
  activeSlider = -1;
  if (button[BUTTON_MORE].isVisible()) {
    showMainMenu();
  } else if (button[BUTTON_MODE].isVisible()) {
    showMoreMenu();
    menuTicker.once(2, cancelMoreMenu);
  } else if (button[BUTTON_MODE_CC].isVisible()) {
    showModesMenu();
    menuTicker.once(2, cancelModeMenu);  }
}

void initColorChordClient() {
  // setting up Station AP
  sprintf(hostString, "ESP_%04X", ESP.getChipId() & 0xFFFF);

  if (!MDNS.begin(hostString)) {
    Serial.println("Error setting up MDNS responder");
    //displayMsg("Error setting up MDNS responder")
    delay(1000);
    ESP.restart();
  } else {
    // Attempt to find colorchord via mDNS lookup

    // TO GET THIS TO WORK, I HAD TO MODIFY ESP8266mDNS.cpp, aparently because the colorchord mDNS service
    // announcement doesn't include a server name after the SRV record, which was breaking mDNS. I changed
    // line ~718 to :         if (tmp8 > 0 && answerRdlength - (6 + 1 + tmp8) > 0) {
    // (added the check for tmp8 > 0)
    // See: https://github.com/esp8266/Arduino/issues/5114
    int n = MDNS.queryService("colorchord", "tcp");
    Serial.println("mDNS query done");
    if (n == 0) {
      Serial.println("mDNS no response");
    } else {
      colorChordIP = MDNS.IP(0);
    }
    if (colorChordIP[0] == 0x0) {
      Serial.println("No colorchord found!");
      displayMsg("colorchord NOT FOUND");
      Serial.println("Reboot in 3 secs...");
      mdnsTicker.attach(3, retryStartup);
    } else {
      Serial.print("colorchord found at: ");
      Serial.println(colorChordIP);
      displayMsg("colorchord @ " + colorChordIP.toString());
    }
  }
}

void retryStartup() {
  ESP.restart();
}

void lookupColorChord() {
}

void initWiFi() {
  WiFi.hostname(hostString);
  WiFi.begin(ssid, pass);
  //WiFi.begin(ssid);

  // Wait for connect to AP
  Serial.print("[Connecting]");
  Serial.print(ssid);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.println(WiFi.status());
    tries++;
    if (tries > 30) {
      break;
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Fall back to");
    Serial.print(esp_ssid);
    WiFi.begin(esp_ssid);
    tries = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(250);
      Serial.println(WiFi.status());
      tries++;
      if (tries > 15) {
        break;
      }
    }
  }
  Serial.println("Connected to wifi");
  Serial.print("Hostname: "); Serial.println(hostString);
  Serial.print("My IP: "); Serial.println(WiFi.localIP());
  IPAddress myIP = WiFi.gatewayIP();
  Serial.print("gatewayIP: "); Serial.println(WiFi.gatewayIP());
  byte lsb = ESP.getChipId() & 255;
  Serial.printf("ChipId LSB: %02X \n", lsb);
  myIP[3] = lsb;
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(myIP, WiFi.gatewayIP(), subnet);
  Serial.print("IP now: "); Serial.println(WiFi.localIP());
  Serial.println();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  IPAddress broadcastIp = ~WiFi.subnetMask() | WiFi.gatewayIP();
  Serial.print("broadcastIp address: "); Serial.println(broadcastIp);
}

boolean connectUDP() {  // connect to UDP – returns true if successful or false if not
  boolean state = false;

  Serial.println("");
  Serial.println("Connecting to UDP");

  if (CmdUdp.begin(listenCmdPort) == 1) {
    Serial.println("Connection successful");
    state = true;
    updConnected = true;
  } else {
    Serial.println("Connection failed");
  }
  return state;
}


void handleUdpCommands() {
  if (!inManualMode) {
    char * token;
    int receivePacketSize = CmdUdp.parsePacket();
    if (receivePacketSize) {
      int len = CmdUdp.read(packetBuffer, receivePacketSize);
//  Serial.printf("Got response: %s\n", (char*)packetBuffer);
      if ((char)packetBuffer[0] == 'C' && (char)packetBuffer[1] == 'B') { // Got DFT values
        token = strtok((char*)packetBuffer, "\t");
        token = strtok(NULL, "\t");
        if ( ! ledCountDetermined) {
          numDftBins = atoi(token);
          ledCountDetermined = true;
          Serial.print("Detected DFT bin count: "); Serial.println(numDftBins, DEC);
        }
        token = strtok(NULL, "\t");
        for ( int i = 0; i < numDftBins; i++ ) {
          int x = i * tft.width() / numDftBins;
          memcpy(sampBuf, &token[i * 4], 4 );
          sampBuf[4] = '\0';
          //int samp = (int)((log(strtol(sampBuf, NULL, 16)/10)+1) * 10);
          int samp = (int)((log(strtol(sampBuf, NULL, 16) / 15.0 + 1)) * 15);
          vuLevels[i] = samp;
        }
      } else if ((char)packetBuffer[0] == 'C' && (char)packetBuffer[1] == 'V' && (char)packetBuffer[2] == 'R') { // Got global parameter values
        token = strtok((char*)packetBuffer, "\t");
        Serial.println();
        while (token != NULL) {
          char *value;
          value = split(token, "=");
          if (value) {
            Serial.printf("%s = %s\n", token, value);
            if (strcmp(token, "gAMP_1_IIR_BITS") == 0) {
              gAMP_1_IIR_BITS = atoi(value);
            } else if (strcmp(token, "gAMP_2_IIR_BITS") == 0) {
              gAMP_2_IIR_BITS = atoi(value);
            } else if (strcmp(token, "gMIN_AMP_FOR_NOTE") == 0) {
              gMIN_AMP_FOR_NOTE = atoi(value);
            } else if (strcmp(token, "gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR") == 0) {
              gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR = atoi(value);
            } else if (strcmp(token, "gUSE_NUM_LIN_LEDS") == 0) {
              gUSE_NUM_LIN_LEDS = atoi(value);
            } else if (strcmp(token, "gINITIAL_AMP") == 0) {
              gINITIAL_AMP = atoi(value);
            } else if (strcmp(token, "gCOLORCHORD_ACTIVE") == 0) {
              gCOLORCHORD_ACTIVE = atoi(value);
            } else if (strcmp(token, "gCOLORCHORD_OUTPUT_DRIVER") == 0) {
              gCOLORCHORD_OUTPUT_DRIVER = atoi(value);
            }
          }
          token = strtok(NULL, "\t");
        }
      }
    }
  }
}

void sendLedBuffer() {
  packetBuffer[0] = 255;
  for (int i = 0; i < numLeds; i++) {
    int color = ledArray[i];
//Serial.printf("%#.06x\n", color);
    packetBuffer[i*3+1] = color & 0xFF;
    packetBuffer[i*3+2] = color >> 8 & 0xFF;
    packetBuffer[i*3+3] = color >> 16 & 0xFF;
  }
  packetBuffer[numLeds*3+1] = '\0';

//printBufferDebug();
  CmdUdp.beginPacket(broadcastIp, udpBroadcastPort);
  CmdUdp.write(packetBuffer, numLeds*3+1);
  CmdUdp.endPacket();

  delay(10);
}

void loadRainbow() {
  ledCountDetermined = false; // force re-detect if we switch back to colorchord
  numLeds = 150;
  for (int i = 0; i < numLeds; i++) {
    int BGR = ECCtoHEX(i*192/numLeds, 255, 150);
    ledArray[i] = BGR;
  }
}

void loadSolid() {
  ledCountDetermined = false; // force re-detect if we switch back to colorchord
  numLeds = 24;
  for (int i = 0; i < numLeds; i++) {
    if (i % 4 == 0) {
      ledArray[i] = 0x800000; //red
    } else if (i % 4 == 1) {
      ledArray[i] = 0x008000; //green
    } else if (i % 4 == 2) {
      ledArray[i] = 0x000080; //blue
    } else if (i % 4 == 3) {
      ledArray[i] = 0x006060; //yellow
    }
  }
}

void loadOff() {
  ledCountDetermined = false; // force re-detect if we switch back to colorchord
  numLeds = 150;
  for (int i = 0; i < numLeds; i++) {
    ledArray[i] = 0x00;
  }
}

void alternateLeds() {
  sendLedBuffer();
}

void rotateLeds() {
  int last = ledArray[numLeds-1];
  for (int i = numLeds-1; i >= 0; i--) {
    ledArray[i+1] = ledArray[i];
  }
  ledArray[0] = last;
  sendLedBuffer();
}

void tickVuMeter() {
  if (!inManualMode && current_menu == MENU_MAIN && activeSlider == -1) {  // Only render when not using a slider
// Serial.printf("tickVuMeter() %d\n", activeSlider);
    sendCommand(CMD_REQ_DFT2, NULL);
    for ( int i = 0; i < numDftBins; i++ ) {
      int x = i * vuWidth / numDftBins + vuPadding;
      double f = vuLevels[i];
      int y = (vuMult * f / 2047) * tft.height();
      if (y > maxVuY) y = maxVuY;

      uint16_t color = CCColor(i);

      tft.drawFastVLine(x, 220 - maxVuY, maxVuY, ILI9341_BLACK);
      tft.drawFastVLine(x + 1, 220 - maxVuY, maxVuY, ILI9341_BLACK);
      tft.drawFastVLine(x, 220 - y, y, color);
      tft.drawFastVLine(x + 1, 220 - y, y, color);
    }
  }
}

void clearVu() {
  for ( int i = 0; i < numDftBins; i++ ) {
    int x = i * vuWidth / numDftBins + vuPadding;
    tft.drawFastVLine(x, 220 - maxVuY, maxVuY, ILI9341_BLACK);
    tft.drawFastVLine(x + 1, 220 - maxVuY, maxVuY, ILI9341_BLACK);
  }
}

void displayMode() {
  String sMode = "";
  if (inManualMode) {
    sMode = "Manual: ";
    if (manualMode == MANUAL_MODE_CHASE) {
      sMode = sMode + "CHASE";
    } else if (manualMode == MANUAL_MODE_SOLID) {
      sMode = sMode + "SOLID";
    } else if (manualMode == MANUAL_MODE_ALTERNATE) {
      sMode = sMode + "ALT";
    } else if (manualMode == MANUAL_MODE_OFF) {
      sMode = sMode + "OFF";
    } 
  } else {
    sMode = "CC: ";
    if (gCOLORCHORD_OUTPUT_DRIVER == 0) {
      sMode = sMode + "CHASE";
    } else if (gCOLORCHORD_OUTPUT_DRIVER == 1) {
      sMode = sMode + "ALL";
    } else if (gCOLORCHORD_OUTPUT_DRIVER == 2) {
      sMode = sMode + "ALTERNATE";
    } else if (gCOLORCHORD_OUTPUT_DRIVER == 3) {
      sMode = sMode + "ON";
    } else if (gCOLORCHORD_OUTPUT_DRIVER == 4) {
      sMode = sMode + "OFF";
    }
  }
  tft.fillRect(220, 230, 100, 10, ILI9341_BLACK);
  tft.setCursor(220, 230);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.print(sMode.c_str());
  Serial.println(sMode);
}

void displayMsg(String msg) {
  tft.setCursor(0, 230);
  tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.print(msg.c_str());
}

char *split(char *str, const char *delim) {
  char *p = strstr(str, delim);
  if (p == NULL) return NULL;     // delimiter not found
  *p = '\0';                      // terminate string after head
  return p + strlen(delim);       // return tail substring
}

void printBufferDebug() {
  Serial.print(packetBuffer[0],HEX);
  Serial.print(' ');

  for (int i=0; i<numLeds*3; i+=3) {
    Serial.printf("%.02x%.02x%.02x ", packetBuffer[i+1],  packetBuffer[i+2],  packetBuffer[i+3]);
  } // end for
  Serial.println();  
}
