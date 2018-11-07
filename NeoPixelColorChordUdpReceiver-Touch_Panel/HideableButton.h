#ifndef MYHIDEABLEBUTTON_H
#define MYHIDEABLEBUTTON_H
#include <Arduino.h>
#include <Adafruit_GFX.h>

class HideableButton : public Adafruit_GFX_Button {
  public:
    void initButton(
     Adafruit_GFX *gfx, int16_t x, int16_t y, uint16_t w, uint16_t h,
     uint16_t outline, uint16_t fill, uint16_t textcolor,
     char *label, uint8_t textsize, bool visible) {
      initButtonUL(gfx, x - (w / 2), y - (h / 2), w, h, outline, fill, textcolor, label, textsize);
      _buttonLabel = label;
      _visible = visible;
      _x1 = x - (w / 2);
      _y1 = y - (h / 2);
      _w = w;
      _h = h;
      mygfx = gfx;
    }

    boolean contains(int16_t x, int16_t y) {
      if (_visible) {
//Serial.printf("Button at %s contains %d, %d\n", _buttonLabel, x, y);
        return Adafruit_GFX_Button::contains(x, y);
      } else {
        return false;
      }
    }

    void drawButton(boolean inverted) {
      if (_visible) {
        Adafruit_GFX_Button::drawButton(inverted);
      }
    }

    void hide() {
      _visible = false;
      press(false);press(false);  // force reset of button state
      mygfx->fillRect(_x1, _y1, _w, _h, ILI9341_BLACK);
    }
    
    boolean isVisible() { return _visible; }

    void setVisible(bool visible) { _visible = visible; }

  private:
    bool _visible = false;
    Adafruit_GFX *mygfx;
    int16_t       _x1, _y1; // Coordinates of top-left corner
    uint16_t      _w, _h;
    char        * _buttonLabel;
};



#endif
