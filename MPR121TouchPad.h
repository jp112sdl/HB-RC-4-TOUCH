//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2021-01-17 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------


#ifndef __MPR121TOUCHPAD_H_
#define __MPR121TOUCHPAD_H_

#include "MPR121.h"

namespace as {
MPR121<NUM_CHANNELS> mpr121;

class TouchPadButton: public Alarm {

#define DEBOUNCETIME millis2ticks(50)

  public:
    enum States {
      invalid = 0,
      none = 1,
      released = 2,
      pressed = 3,
      debounce = 4,
      longpressed = 5,
      longreleased = 6,
    };

  protected:
    uint8_t  stat     : 3;
    uint8_t  pinstate : 1;

    uint16_t longpresstime;

  public:
    TouchPadButton() : Alarm(0), stat(none), pinstate(false), longpresstime(millis2ticks(400)) { }
    virtual ~TouchPadButton() {}

    void setLongPressTime(uint16_t t) {
      longpresstime = t;
    }

    void setThresholds(uint8_t touched, uint8_t released, uint8_t ele) {
      mpr121.set_thresholds(touched, released, ele);
    }

    virtual void trigger(AlarmClock& clock) {
      uint8_t  nextstate = invalid;
      uint16_t nexttick = 0;
      switch ( state() ) {
        case released:
        case longreleased:
          nextstate = none;
          break;

        case debounce:
          nextstate = pressed;
          if (pinstate == true) {
            // set timer for detect longpressed
            nexttick = longpresstime - DEBOUNCETIME;
          } else {
            nextstate = released;
            nexttick = DEBOUNCETIME;
          }
          break;

        case pressed:
        case longpressed:
          if ( pinstate == true) {
            nextstate = longpressed;
            nexttick = longpresstime;
          }
          break;
      }
      // reactivate alarm if needed
      if ( nexttick != 0 ) {
        tick = nexttick;
        clock.add(*this);
      }
      // trigger the state change
      if ( nextstate != invalid ) {
        state(nextstate);
      }
    }

    virtual void state(uint8_t s) {
      switch (s) {
          /*case released: DPRINTLN(F(" released")); break;
            case pressed: DPRINTLN(F(" pressed")); break;
            case debounce: DPRINTLN(F(" debounce")); break;
            case longpressed: DPRINTLN(F(" longpressed")); break;
            case longreleased: DPRINTLN(F(" longreleased")); break;
            default: DPRINTLN(F("")); break;*/
      }
      stat = s;
    }

    uint8_t state() const {
      return stat;
    }

    void check(bool b) {
      uint8_t ps = b;
      if ( pinstate != ps ) {
        pinstate = ps;
        uint16_t nexttick = 0;
        uint8_t  nextstate = state();
        switch ( state() ) {
          case none:
            nextstate = debounce;
            nexttick = DEBOUNCETIME;
            break;

          case pressed:
          case longpressed:
            if (pinstate == false) {
              nextstate = state() == pressed ? released : longreleased;
              nexttick = DEBOUNCETIME;
            }
            break;
          default:
            break;
        }
        if ( nexttick != 0 ) {
          sysclock.cancel(*this);
          tick = nexttick;
          sysclock.add(*this);
        }
        if ( nextstate != state () ) {
          state(nextstate);
        }
      }
    }
};

template <class DEVTYPE>
class MPR121TouchPad : public MPR121<NUM_CHANNELS> {
  public:
    class ReadTouchedButtonAlarm : public Alarm {
      public:
      MPR121TouchPad& mkb;
      ReadTouchedButtonAlarm (MPR121TouchPad& _mkp) : Alarm(0), mkb(_mkp) {}
        virtual ~ReadTouchedButtonAlarm () {}
        virtual void trigger(__attribute__((unused)) AlarmClock& clock) {
          mkb.readTouchedButton();
        }
    };

  private:
    DEVTYPE& device;
    ReadTouchedButtonAlarm rtba;
    uint8_t irqPin;

  public:
    MPR121TouchPad(DEVTYPE& dev) : device(dev), rtba(*this), irqPin(0) { }
    virtual ~MPR121TouchPad() {}

    void irq () {
      sysclock.cancel(rtba);
      rtba.set(millis2ticks(5));
      sysclock.add(rtba);
    }

    void readTouchedButton() {
      uint16_t reg02 = mpr121.get_oor_state();
      if (reg02) {
        DPRINT(F("MPR121 error: 0x")); DHEXLN(reg02);
      } else {

        uint8_t bnum = 0;
        if (digitalRead(irqPin) == true) {
          uint16_t t = mpr121.get_touched();
          for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
            if (t & _BV(i)) {
              bnum = i+1;
              break;
           }
          }
        }

        if (bnum > 0) {
          device.channel(bnum).check(true);
          sysclock.cancel(rtba);
          rtba.set(2);
          sysclock.add(rtba);
        } else {
          for (uint8_t i = 0; i < 4; i++)
            device.channel(i + 1).check(false);
        }
      }
    }

    void init(uint8_t p) {
     mpr121.init();
     irqPin = p;
     pinMode(irqPin, INPUT);
    }

};

#define MPR121TouchPadISR(btn, irqPin) class btn##ISRHandler { \
    public: \
      static void isr () { btn.irq(); } \
  }; \
  btn.init(irqPin); \
  if( digitalPinToInterrupt(irqPin) == NOT_AN_INTERRUPT ) \
    enableInterrupt(irqPin,btn##ISRHandler::isr,CHANGE); \
  else \
    attachInterrupt(digitalPinToInterrupt(irqPin),btn##ISRHandler::isr,CHANGE);\

}
#endif
