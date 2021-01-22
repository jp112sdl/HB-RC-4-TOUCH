//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2021-01-18 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <SPI.h>  // after including SPI Library - we can use LibSPI class
#include <AskSinPP.h>
#include <LowPower.h>

#include <MultiChannelDevice.h>
#include <Register.h>
#define NUM_CHANNELS 4

#include "MPR121TouchPad.h"

#define LED_PIN           5
#define LED_PIN2          4
#define CONFIG_BUTTON_PIN 8
#define MPR121_IRQ_PIN    3


// number of available peers per channel
#define PEERS_PER_CHANNEL 10

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
    {0xF3,0x2D,0x01},       // Device ID
    "JPRC4TCH01",           // Device Serial
    {0xF3,0x2D},            // Device Model
    0x10,                   // Firmware Version
    as::DeviceType::Remote, // Device Type
    {0x00,0x00}             // Info Bytes
};

/**
 * Configure the used hardware
 */
typedef LibSPI<10> SPIType;
typedef Radio<SPIType,2> RadioType;
typedef DualStatusLed<LED_PIN2,LED_PIN> LedType;
typedef AskSin<LedType,IrqInternalBatt,RadioType> Hal;


DEFREGISTER(RemoteTouchReg1,CREG_LONGPRESSTIME,CREG_AES_ACTIVE,CREG_DOUBLEPRESSTIME, 0x0a, 0x0b)
class RemoteTouchList1 : public RegList1<RemoteTouchReg1> {
public:
  RemoteTouchList1 (uint16_t addr) : RegList1<RemoteTouchReg1>(addr) {}

  bool touchTouchThreshold (uint8_t value) const { return this->writeRegister(0x0a, value & 0xff);}
  uint8_t touchTouchThreshold () const { return this->readRegister(0x0a, 14); }

  bool touchReleaseThreshold (uint8_t value) const { return this->writeRegister(0x0b, value & 0xff);}
  uint8_t touchReleaseThreshold () const { return this->readRegister(0x0b, 10); }

  void defaults () {
    clear();
    longPressTime(1);
    touchTouchThreshold(14);
    touchReleaseThreshold(10);
    // aesActive(false);
    // doublePressTime(0);
  }
};

class RemoteTouchChannel : public Channel<Hal,RemoteTouchList1,EmptyList,DefList4,PEERS_PER_CHANNEL,List0>, public TouchPadButton {

private:
  uint8_t       repeatcnt;
  volatile bool isr;

public:

  typedef Channel<Hal,RemoteTouchList1,EmptyList,DefList4,PEERS_PER_CHANNEL,List0> BaseChannel;

  RemoteTouchChannel () : BaseChannel(), repeatcnt(0), isr(false) {}
  virtual ~RemoteTouchChannel () {}

  TouchPadButton& button () { return *(TouchPadButton*)this; }
  uint8_t status () const { return 0; }
  uint8_t flags () const { return 0; }

  virtual void state(uint8_t s) {
    //DHEX(BaseChannel::number());
    TouchPadButton::state(s);
    RemoteEventMsg& msg = (RemoteEventMsg&)this->device().message();
    msg.init(this->device().nextcount(),this->number(),repeatcnt,(s==longreleased || s==longpressed),this->device().battery().low());
    if( s == released || s == longreleased) {
      // send the message to every peer
      this->device().sendPeerEvent(msg,*this);
      repeatcnt++;
    }
    else if (s == longpressed) {
      // broadcast the message
      this->device().broadcastPeerEvent(msg,*this);
    }
  }

  uint8_t state() const { return TouchPadButton::state(); }

  bool pressed () const {
    uint8_t s = state();
    return s == Button::pressed || s == Button::debounce || s == Button::longpressed;
  }

  bool configChanged() {
    //we have to add 300ms to the value set in CCU!
    uint16_t _longpressTime = 300 + (this->getList1().longPressTime() * 100);
    //DPRINT("longpressTime = ");DDECLN(_longpressTime);
    setLongPressTime(millis2ticks(_longpressTime));

    uint8_t touchedThreshold = this->getList1().touchTouchThreshold();
    uint8_t releasedThreshold = this->getList1().touchReleaseThreshold();
    setThresholds(touchedThreshold, releasedThreshold, number() - 1);
    return true;
  }
};

typedef MultiChannelDevice<Hal,RemoteTouchChannel,NUM_CHANNELS> RemoteType;

Hal hal;
RemoteType sdev(devinfo,0x20);
ConfigButton<RemoteType> cfgBtn(sdev);
MPR121TouchPad<RemoteType> mTouchPad(sdev);

void setup () {
  DINIT(57600,ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);
  hal.battery.init(seconds2ticks(60UL*60),sysclock);
  hal.battery.low(23);
  hal.battery.critical(21);
  buttonISR(cfgBtn,CONFIG_BUTTON_PIN);
  MPR121TouchPadISR(mTouchPad, MPR121_IRQ_PIN);
  sdev.initDone();
  while( hal.battery.current() == 0 ) ;
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  //mpr121.debug_print();delay(10);

  if( worked == false && poll == false ) {
    if( hal.battery.critical() ) {
      hal.sleepForever();
    }
    hal.sleep<>();
  }
}
