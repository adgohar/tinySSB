// hardware.cpp

// collects hardware-specific init code (other than UI) for all boards

#include "lib/tinySSBlib.h"
#include "hardware.h"

// ---------------------------------------------------------------------------
#ifdef TINYSSB_BOARD_HELTEC

#ifdef USING_SX1276
SX1276 radio = new Module(SS, DI0, RST);
#endif
#ifdef USING_SX1278
SX1278 radio = new Module(SS, DI0, RST);
#endif

void hw_init()
{
  // SX1278 has the following connections:
  // NSS pin:   10
  // DIO0 pin:  2
  // NRST pin:  9
  // DIO1 pin:  3
  // = new Module(10, 2, 9, 3);

  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("RadioLib success!");
  } else {
    while (true) {
      Serial.print(F("RadioLib failed, code "));
      Serial.println(state);
      delay(2000);
    }
  }  
}

#endif

// ---------------------------------------------------------------------------
#ifdef TINYSSB_BOARD_T5GRAY

void hw_init()
{
}

#endif

// ---------------------------------------------------------------------------
#ifdef TINYSSB_BOARD_TBEAM

#include <axp20x.h>
#include <Wire.h>
AXP20X_Class axp;

#ifdef HAS_GPS
TinyGPSPlus gps;
HardwareSerial GPS(1);
#endif

#ifdef USE_RADIO_LIB
# ifdef USING_SX1262
    SX1262 radio = new Module(SS, DI0, RST); // , RADIO_BUSY_PIN);
# endif
# ifdef USING_SX1276
    SX1276 radio = new Module(SS, DI0, RST); // , RADIO_BUSY_PIN);
# endif
#endif

void hw_init()
{
  Wire.begin(21, 22);
  if (axp.begin(Wire, AXP192_SLAVE_ADDRESS)) {
    Serial.println("AXP192 Begin FAIL");
  } else {
    // Serial.println("AXP192 Begin PASS");
#ifdef HAS_LORA
    axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
#else
    axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
#endif
#ifdef HAS_GPS
    axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);
#else
    axp.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
#endif
    axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
    axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
#ifdef HAS_OLED
    axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON); // OLED
#else
    axp.setPowerOutPut(AXP192_DCDC1, AXP202_OFF); // no OLED
#endif

#ifdef HAS_OLED
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high、
#endif
    
#ifdef HAS_GPS
    GPS.begin(9600, SERIAL_8N1, 34, 12);   //17-TX 18-RX
#endif
  }
}

#endif

// ---------------------------------------------------------------------------
#ifdef TINYSSB_BOARD_TDECK

#ifdef USE_RADIO_LIB
   SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN,
                             RADIO_RST_PIN, RADIO_BUSY_PIN);
#endif

void hw_init()
{
}

#endif

// ---------------------------------------------------------------------------
#ifdef TINYSSB_BOARD_TWRIST

void hw_init()
{
}

#endif

// ---------------------------------------------------------------------------
// eof
