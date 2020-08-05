/*
 * ATMega328 SD card interrupter
 * Copyright 2015 oneTesla, LLC
 * See README.txt for licensing information
 * 
 * added funtionality when pressing buttons up and down simultaniously you return to previous menu
 * in Debug mode with LCD- and Datalogger Shield its the right buttom to return to previous menu 
 * Arduino UNO or Duemilanove: https://store.arduino.cc/arduino-uno-rev3 or cheap remake https://www.az-delivery.de/products/uno-r3-board-mit-atmega328p-und-usb-kabel
 * Datalogger Shield: https://learn.adafruit.com/adafruit-data-logger-shield or cheap remake https://www.az-delivery.de/products/datenlogger-modul?_pos=1&_sid=d2708e960&_ss=r
 * LCD-Keypad Shield: https://wiki.dfrobot.com/LCD_KeyPad_Shield_For_Arduino_SKU__DFR0009 or cheap remake https://www.az-delivery.de/products/azdelivery-hd44780-1602-lcd-module-display-2x16-zeichen-fur-arduino-lcd1602-keypad
 * Quality: you get what you pay for and READ THE FUCKING MANUAL :D
 * Attention: LCD Shield (at least the ones i used) you need to trimm Pin 10, usually used for Backlight control but is needed for the SD card (PIN 10 for Chip selelect). Backlight is now always on, but usually you need this anyway
 * To use it at all you need to enable Debug mode go to constants.h and uncomment the line 11 //#define DEBUG (original firmare would need some minor tweaks, but nothing serious)
 * Attention: in Debug mode the original oneTesla-SD-Interrupter wont work!!!
 * some other parts:
 * 1x LED FIB OPT 660NM SUPERBRITE RED IF-E97 https://www.digikey.de/product-detail/de/industrial-fiber-optics/IF-E97/FB129-ND/272338
 * 1x CONN JACK STEREO 3.5MM R/A https://www.digikey.de/product-detail/de/switchcraft-inc/35RAPC4BH3/SC1464-ND/1288781
 * 1x 4N24 Optokoppler https://www.digikey.de/product-detail/de/vishay-semiconductor-opto-division/4N25/4N25VS-ND/1738516
 * 1x CONN IC DIP SOCKET 6POS https://www.digikey.de/product-detail/de/te-connectivity-amp-connectors/1-2199298-1/A120346-ND/5022037
 * 1x 1N4148
 * 1x 100Ohm 1/4W
 * 1x 100kOhm 1/4W
 * 1x 220Ohm 1/4W
 * 1x 3.3kOhm 1/4W
 * 1x TRS-MIDI Type A Adapter (check thats connected according to specs, DIN 4 = Ring, DIN 5 = Tip otherwise it wont work!!!) : https://www.thomann.de/de/teenage_engineering_midi_cable_kit.htm
 * in total its something between 40$ and 60$ and maybe 1hr work (less if you already know what your are doing)
 * to use it you need to power it with a Battery pack or Notebook when not connected to mains, e.g. when using Live Mode
 */
 
#include "constants.h"
#include "data.h"
#include "datatypes.h"
#include "sdsource.h"
#include "serialsource.h"
#include "player.h"
#include "system.h"
#include "util.h"
#include "timers.h"
#include "lcd.h"

#include <LiquidCrystal.h>
#include <SdFat.h>

LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

note *note1, *note2;
midiMsg *last_message;
serialsource *serial;
sdsource *sd;
char volindex, menuindex = 0;
int ffreq = 100;

void fixedLoop();
void displayMenu();

void setup() {
  lcd_init();
  lcd_printhome("Booting...");
  setupTimers();
  setupPins();
  player_init();
  sdsource_init();
  serialsource_init();
  displayMenu();
}

void loop() {
  unsigned char key = get_key();
  if (key == btnDOWN) {
    if (menuindex == 2) {
      menuindex = 0;
    } else {
      menuindex++;
    }
    displayMenu();
    delay(180);
  }
  if (key == btnUP) {
    if (menuindex == 0) {
      menuindex = 2;
    } else {
      menuindex--;
    }
    displayMenu();
    delay(180);
  }
  if (key == btnSELECT) {
    if (menuindex == MENU_FIXED) {
      delay(150);
      fixedLoop();
      delay(300);
    } else if (menuindex == MENU_LIVE) {
      delay(150);
      lcd_printhome("Live Mode ON");
      lcd_setcursor(0, 1);
      for (int i = 0; i < volindex; i++) {
        lcd_print((char) (1));
      }
      serialsource_run();
      lcd_printhome("Live Mode OFF");
      delay(300);
    } else { // MENU_SDCARD
      delay(150);
      if (sd->valid) {
        sdsource_run();
      } else {
        sdsource_initcard();
      }
      delay(300);
    }
    displayMenu();
    while (get_key() != btnNONE);
  }
}

void displayMenu()
{
  if (menuindex == MENU_SDCARD) {
    lcd_printhome("SD Card");
    lcd_setcursor(0, 1);
    if (sd->valid) {          
      lcd_print(sd->file_count);
      lcd_print(" file(s).");
    } else {
      lcd_print((char *)sd->last_error);
    }
  } else if (menuindex == MENU_LIVE) {
    lcd_printhome("Live Mode");
  } else {
    lcd_printhome("Fixed Mode");
  }
}

void fixedLoop() {
  lcd_printhome("[Freq: ");
  lcd_print(ffreq);
  lcd_print("Hz");
  lcd_printat(15, 0, ']');
  lcd_setcursor(0, 1);
  for (int i = 0; i < volindex; i++) {
    lcd_print((char) (1));
  }
  char mode = 1;
  unsigned long elapsed = 0;
  unsigned int uinc, dinc = 1;
  note1->velocity = 127;
  note1->on_time = get_on_time(ffreq);
  setTimer1f(ffreq);
  engageISR1();
  for (;;) {
    unsigned char key = get_key();
    if (elapsed > 100000) {
      elapsed = 0;
      if (key != btnUP) uinc = 1;
      if (key != btnDOWN) dinc = 1;
      if (key == btnSELECT) {
        mode = !mode;
        if (mode) {
          lcd_printhome("[Freq: ");
          lcd_print(ffreq);
          lcd_print("Hz");
          lcd_printat(15, 0, ']');
          lcd_setcursor(0, 1);
          for (int i = 0; i < volindex; i++) {
            lcd_print((char) (1));
          }
        } else {
          lcd_printhome("Freq: ");
          lcd_print(ffreq);
          lcd_print("Hz");
          lcd_setcursor(0, 1);
          for (int i = 0; i < volindex; i++) {
            lcd.print((char) (1));
          }
          lcd_printat(0, 1, '[');
          lcd_printat(15, 1, ']');
        }
      }
      if (key == btnUP) {
        if (!mode) {
          lcd_setcursor(0, 1);
          incvol(&lcd);
          lcd_printat(0, 1, '[');
          lcd_printat(15, 1, ']');
        } else {
          ffreq += uinc;
          uinc += 5;
          if (ffreq > 1000) ffreq = 1000;
          lcd_printat(7, 0, 7, (unsigned int) ffreq);
          lcd_print("Hz");
          note1->on_time = get_on_time(ffreq);
          setTimer1f(ffreq);
        }
      }
      if (key == btnDOWN) {
        if (!mode) {
          lcd_setcursor(0, 1);
          decvol(&lcd);
          lcd_printat(0, 1, '[');
          lcd_printat(15, 1, ']');
        } else {
          ffreq -= dinc;
          dinc += 5;
          if (ffreq < 1) ffreq = 1;
          lcd_printat(7, 0, 7, (unsigned int) ffreq);
          lcd_print("Hz");
          note1->on_time = get_on_time(ffreq);
          setTimer1f(ffreq);;
        }
      }
      if (key == btnRETURN) {
        note_stop();
        return;
      }
    }
    delayMicroseconds(1000);
 //   elapsed += 1000;
   elapsed += 500;  // buttoms react a littele less bouncy
  }
}

int main() {
  init();
  setup();
  for(;;) {
    loop();
  }
  return 0;
}
