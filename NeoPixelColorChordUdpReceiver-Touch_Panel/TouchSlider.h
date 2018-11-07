#ifndef touch_slider_h
#define touch_slider_h

#include <Arduino.h>
#include <Adafruit_ILI9341esp.h>
#include <Adafruit_GFX.h>
#include <XPT2046.h>

#define Slider_Cursor_W 20
#define Slider_Cursor_H 40
#define Slider_Cursor_Y sliderY - Slider_Cursor_H/2
#define Secs_Before_Goes_Away 2

static Adafruit_ILI9341* innerDisplay;

// Callback called with a value
typedef void (*ValueCallback) (int response);
typedef void (*DoneCallback) ();

class TouchSlider {

  public:

    TouchSlider() { }
    
    void init(Adafruit_ILI9341* tft, char* label, int x, int y, int width, int initialValue, ValueCallback valCallback, DoneCallback doneCallback) {
      sliderNameStr = label;
      innerDisplay = tft;
      sliderX = x;
      sliderY = y;
      sliderWidth = width;
      xPrev = initialValue + sliderX; 
      sliderValue = initialValue;
      myValCallback = valCallback;
      myDoneCallback = doneCallback;
    }

    void show() {
      visible = true;
      disappearAt = millis() + Secs_Before_Goes_Away * 1000;
      sliderHandler(xPrev);
      innerDisplay->fillRect(sliderX+5, sliderY-40, sliderWidth-70, 20, ILI9341_BLACK);
      if (strlen(sliderNameStr) > 0) {
        innerDisplay->setCursor(sliderX+5, sliderY-40);
        innerDisplay->setTextColor(ILI9341_BLUE, ILI9341_BLUE);
        innerDisplay->setTextSize(2);
        innerDisplay->print(sliderNameStr);
      }
    }
    
    void hide() {
      disappearAt = millis();
      notify(0,0);
    }
    
    void notify(int x, int y) {
      if (disappearAt > millis()) {
        if (y > Slider_Cursor_Y && y < (Slider_Cursor_Y + Slider_Cursor_H)) {
          sliderCursorX = x - Slider_Cursor_W/2;
          if(x > sliderCursorX && x < (sliderCursorX + Slider_Cursor_W)) {
            if(sliderCursorX < sliderX) {
              x =  sliderX;
            } else if((sliderCursorX + Slider_Cursor_W) > (sliderX+sliderWidth)) {
              x = (sliderX+sliderWidth)-Slider_Cursor_W;
            }
            if (x != xPrev) {
              disappearAt = millis() + Secs_Before_Goes_Away * 1000;
              sliderHandler(x);
              xPrev = x;
            }
          }
        }
      } else {
        if (visible) {
          visible = false;
          innerDisplay->fillRect(sliderX, sliderY-50, sliderWidth+1, sliderY + Slider_Cursor_H, ILI9341_BLACK);
          if (myDoneCallback != NULL) { // if a DoneCallback is attached, call it when control "goes away"
            myDoneCallback();
          }
        }
      }
    }

    bool isVisible() {
      return visible;
    }
    
    void setLabel(char *label) {
      sliderNameStr = label;
      if (isVisible()) showSliderValue(sliderValue);
    }
    
    void setValue(int newValue) {
      sliderValue = newValue;
      xPrev = sliderValue + sliderX;
      if (isVisible()) showSliderValue(sliderValue);
    }
    
    void showSliderValue(int value) {
      sprintf(sliderValueStr,"%03d",value);
      innerDisplay->setCursor(sliderX+sliderWidth-60, sliderY-50);
      innerDisplay->setTextColor(ILI9341_WHITE, ILI9341_BLUE);
      innerDisplay->setTextSize(3);
      innerDisplay->print(sliderValueStr);
    }
    
    void sliderHandler(int x) {
      sliderValue = x - sliderX;
      if (sliderValue < 0) sliderValue = 0; 
      showSliderValue(sliderValue);
    
      // erase previous thumb by redrawing with background color
      //innerDisplay->fillTriangle(xPrev-1, Slider_Cursor_Y-21, x + Slider_Cursor_W + 1, Slider_Cursor_Y-21, xPrev+(Slider_Cursor_W/2), Slider_Cursor_Y+1, ILI9341_BLACK);
      innerDisplay->fillRect(xPrev, Slider_Cursor_Y, Slider_Cursor_W+1, Slider_Cursor_H*2, ILI9341_BLACK);
    
      innerDisplay->writeFastHLine(sliderX, sliderY, sliderWidth, ILI9341_WHITE); 
      innerDisplay->writeFastVLine(sliderX, sliderY-(Slider_Cursor_H/2), Slider_Cursor_H, ILI9341_WHITE); 
      innerDisplay->writeFastVLine(sliderX+sliderWidth, sliderY-(Slider_Cursor_H/2), Slider_Cursor_H, ILI9341_WHITE); 
      
      // then draw new thumb
      innerDisplay->fillTriangle(x, Slider_Cursor_Y, x + Slider_Cursor_W, Slider_Cursor_Y, x+(Slider_Cursor_W/2), Slider_Cursor_Y+(Slider_Cursor_H/2), ILI9341_WHITE);
      innerDisplay->fillTriangle(x, Slider_Cursor_Y+Slider_Cursor_H, x+Slider_Cursor_W, Slider_Cursor_Y+Slider_Cursor_H, x+(Slider_Cursor_W/2), Slider_Cursor_Y+(Slider_Cursor_H/2), ILI9341_WHITE);

      if (myValCallback != NULL) { // if a command callback was initialized
        myValCallback(sliderValue);
      }
    }    
    
  private:
    int sliderX = 0;
    int sliderY = 0;
    int sliderWidth = 0;
    int sliderCursorX = 0;
    int xPrev = 0;
    ValueCallback myValCallback;    
    DoneCallback myDoneCallback;    
    int initialValue = 160;
    int disappearAt = millis();
    bool visible = false;
    int sliderValue = 0;
    char* sliderNameStr;
    char sliderValueStr[5];

};
#endif
