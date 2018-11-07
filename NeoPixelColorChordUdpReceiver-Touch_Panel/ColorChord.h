#ifndef COLORCHORD_INC_H
#define COLORCHORD_INC_H

#include <Arduino.h>
#include <math.h>

#define CMD_AMP_MULT   0
#define CMD_NOTE_MIN   1
#define CMD_NOTE_GONE  2
#define CMD_MODE_CC    3
#define CMD_MODE_CHASE 4
#define CMD_MODE_ON    5
#define CMD_MODE_CC1   6
#define CMD_MODE_OFF   7
#define CMD_REQ_DFT2   8
#define CMD_REQ_VARS   9

// ColorChord globals
int gROOT_NOTE_OFFSET = 0;
int gAMP_1_IIR_BITS = 4;
int gAMP_2_IIR_BITS = 4;
int gMIN_AMP_FOR_NOTE = 48;
int gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR = 32;
int gUSE_NUM_LIN_LEDS = 150;
int gINITIAL_AMP = 100;
int gCOLORCHORD_ACTIVE = 1;
int gCOLORCHORD_OUTPUT_DRIVER = 0;

int numDftBins = 16;  // rFIXBPERO
char sendBuffer[50];
byte packetBuffer[1024]; //buffer to hold incoming and outgoing packets
unsigned int listenCmdPort = 5001;      // local port to listen for UDP packets
char hostString[16] = {0};
boolean updConnected = false;
IPAddress colorChordIP;

WiFiUDP CmdUdp;

void sendCommandBuffer(char *command) {
  //Serial.printf(command);Serial.println();
  CmdUdp.beginPacket(colorChordIP, 7878);
  CmdUdp.write(command);
  CmdUdp.endPacket();
}

void sendCommand(int command, int value) {
  switch ( command ) {
    case CMD_AMP_MULT:
      sprintf(sendBuffer, "CVW\tgINITIAL_AMP\t%d\0", value);
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_NOTE_MIN:
      sprintf(sendBuffer, "CVW\tgMIN_AMP_FOR_NOTE\t%d\0", value);
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_NOTE_GONE:
      sprintf(sendBuffer, "gMINIMUM_AMP_FOR_NOTE_TO_DISAPPEAR\t%d\0", value);
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_MODE_CC:
      //      sprintf(sendBuffer, "CVW\tgCOLORCHORD_ACTIVE\t%d\0", 1);
      //      sendCommandBuffer(sendBuffer);
      sprintf(sendBuffer, "CVW\tgCOLORCHORD_OUTPUT_DRIVER\t%d\0", 0);
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_MODE_CC1:
      sprintf(sendBuffer, "CVW\tgCOLORCHORD_OUTPUT_DRIVER\t%d\0", 1);
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_MODE_CHASE:
      sprintf(sendBuffer, "CVW\tgCOLORCHORD_OUTPUT_DRIVER\t%d\0", 2);
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_MODE_ON:
      sprintf(sendBuffer, "CVW\tgCOLORCHORD_OUTPUT_DRIVER\t%d\0", 3);
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_MODE_OFF:
      sprintf(sendBuffer, "CVW\tgCOLORCHORD_OUTPUT_DRIVER\t%d\0", 4);
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_REQ_DFT2:
      sprintf(sendBuffer, "CB2\0");
      sendCommandBuffer(sendBuffer);
      break;
    case CMD_REQ_VARS:
      sprintf(sendBuffer, "CVR\0");
      sendCommandBuffer(sendBuffer);
      break;
    default:
      return;
  }
}

// Return unit16's containing bit-endocded color levels in format: rrrrrggggggbbbbb
// (format expected by ILI9341)
uint16_t HueToGfx(uint16_t hue) {
  uint16_t SIXTH1 = 43;
  uint16_t SIXTH2 = 85;
  uint16_t SIXTH3 = 120;
  uint16_t SIXTH4 = 165;
  uint16_t SIXTH5 = 213;

  uint16_t red = 0;
  uint16_t green = 0;
  uint16_t blue = 0;


  //hue -= SIXTH1; //Off by 60 degrees.
  hue = (hue + 256) % 256;

  if ( hue < SIXTH1 ) //Ok: Yellow->Red.
  {
    red = 255;
    green = 255 - (hue * 255) / (SIXTH1);
  }
  else if ( hue < SIXTH2 ) //Ok: Red->Purple
  {
    red = 255;
    blue = hue * 255 / SIXTH1 - 255;
  }
  else if ( hue < SIXTH3 ) //Ok: Purple->Blue
  {
    blue = 255;
    red = ((SIXTH3 - hue) * 255) / (SIXTH1);
  }
  else if ( hue < SIXTH4 ) //Ok: Blue->Cyan
  {
    blue = 255;
    green = (hue - SIXTH3) * 255 / SIXTH1;
  }
  else if ( hue < SIXTH5 ) //Ok: Cyan->Green.
  {
    green = 255;
    blue = ((SIXTH5 - hue) * 255) / SIXTH1;
  }
  else //Green->Yellow
  {
    green = 255;
    red = (hue - SIXTH5) * 255 / SIXTH1;
  }

  if ( red > 255 ) red = 255;
  if ( green > 255 ) green = 255;
  if ( blue > 255 ) blue = 255;

  red = (red << 8) & 0xF800;
  green = (green << 2) & 0x7E0;
  blue = blue >> 3;

  uint16_t gfxColor = red + green + blue;
  return gfxColor;
}


uint16_t CCColor(int note) {
  return HueToGfx( note * (255 / numDftBins) );
}


/*
 * Functions copied from ColorChord which map "notes" to colors with saturation and intensity val
 */
 
uint32_t EHSVtoHEX( uint8_t hue, uint8_t sat, uint8_t val ) {
  #define SIXTH1 43
  #define SIXTH2 85
  #define SIXTH3 128
  #define SIXTH4 171
  #define SIXTH5 213

  uint16_t red = 0;
  uint16_t green = 0;
  uint16_t blue = 0;

  hue -= SIXTH1; //Off by 60 degrees.

  //TODO: There are colors that overlap here, consider
  //tweaking this to make the best use of the colorspace.

  if ( hue < SIXTH1 ) //Ok: Yellow->Red.
  {
    red = 255;
    green = 255 - ((uint16_t)hue * 255) / (SIXTH1);
  }
  else if ( hue < SIXTH2 ) //Ok: Red->Purple
  {
    red = 255;
    blue = (uint16_t)hue * 255 / SIXTH1 - 255;
  }
  else if ( hue < SIXTH3 ) //Ok: Purple->Blue
  {
    blue = 255;
    red = ((SIXTH3 - hue) * 255) / (SIXTH1);
  }
  else if ( hue < SIXTH4 ) //Ok: Blue->Cyan
  {
    blue = 255;
    green = (hue - SIXTH3) * 255 / SIXTH1;
  }
  else if ( hue < SIXTH5 ) //Ok: Cyan->Green.
  {
    green = 255;
    blue = ((SIXTH5 - hue) * 255) / SIXTH1;
  }
  else //Green->Yellow
  {
    green = 255;
    red = (hue - SIXTH5) * 255 / SIXTH1;
  }

  uint16_t rv = val;
  if ( rv > 128 ) rv++;
  uint16_t rs = sat;
  if ( rs > 128 ) rs++;

  //red, green, blue range from 0...255 now.
  //Need to apply saturation and value.

  red = ( red * val) >> 8;
  green = (green * val) >> 8;
  blue = (blue * val) >> 8;

  //OR..OB == 0..65025
  red = red * rs + 255 * (256 - rs);
  green = green * rs + 255 * (256 - rs);
  blue = blue * rs + 255 * (256 - rs);

  red >>= 8;
  green >>= 8;
  blue >>= 8;
//Serial.printf( "__%d %d %d =-> %d\n", red, green, blue, rs );

  return red | (green << 8) | ((uint32_t)blue << 16);
}

uint32_t ECCtoHEX( uint8_t note, uint8_t sat, uint8_t val ) {
  uint16_t hue = 0;
  uint16_t third = 65535 / 3;
  uint16_t scalednote = note;
  uint32_t renote = ((uint32_t)note * 65536) / 192;

  //Note is expected to be a vale from 0..(192-1)
  //renote goes from 0 to the next one under 65536.

  if ( renote < third ) {
    //Yellow to Red.
    hue = (third - renote) >> 1;
  } else if ( renote < (third << 1)) {
    //Red to Blue
    hue = (third - renote);
  } else {
    hue = (uint16_t)(((uint32_t)(65536 - renote) << 16) / (third << 1)) + (third >> 1);
  }
  hue >>= 8;

  return EHSVtoHEX( hue, sat, val );
}

#endif
