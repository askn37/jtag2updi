/*
 * sys.cpp
 *
 * Created: 02-10-2018 13:07:52
 *  Author: JMR_2
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "sys.h"
#include "dbg.h"
#include <stdio.h>
#include <string.h>


void SYS::init(void) {
  #ifdef DEBUG_ON
    DBG::initDebug();
  #endif

  #ifdef XAVR
    #ifdef __AVR_DX__
      /* experimental overclock frequency */
      #ifndef CLKCTRL_FRQSEL_28M_gc
      #define CLKCTRL_FRQSEL_28M_gc (0x0A<<2)
      #endif
      #ifndef CLKCTRL_FRQSEL_32M_gc
      #define CLKCTRL_FRQSEL_32M_gc (0x0B<<2)
      #endif
      #if   (F_CPU == 32000000L)
        #define _CLKCTRL_FREQSEL_ CLKCTRL_FRQSEL_32M_gc
      #elif (F_CPU == 28000000L)
        #define _CLKCTRL_FREQSEL_ CLKCTRL_FRQSEL_28M_gc
      #elif (F_CPU == 24000000L)
        #define _CLKCTRL_FREQSEL_ CLKCTRL_FRQSEL_24M_gc
      #elif (F_CPU == 20000000L)
        #define _CLKCTRL_FREQSEL_ CLKCTRL_FRQSEL_20M_gc
      #elif (F_CPU == 16000000L)
        #define _CLKCTRL_FREQSEL_ CLKCTRL_FRQSEL_16M_gc
      #elif (F_CPU == 12000000L)
        #define _CLKCTRL_FREQSEL_ CLKCTRL_FRQSEL_12M_gc
      #elif (F_CPU == 8000000L)
        #define _CLKCTRL_FREQSEL_ CLKCTRL_FRQSEL_8M_gc
      #else
      #assert This internal CPU clock is not supported
      #endif
      // AVR_Dx
      _PROTECTED_WRITE(CLKCTRL_OSCHFCTRLA, _CLKCTRL_FREQSEL_);
    #else
      // tinyAVR-0/1/2 megaAVR-0
      _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, 0);
    #endif
  #else
    #if defined(ARDUINO_AVR_LARDU_328E)
    #include <avr/power.h>
    clock_prescale_set ( (clock_div_t) __builtin_log2(32000000UL / F_CPU));
    #endif
    PORT(UPDI_PORT) = 1 << UPDI_PIN;
  #endif


  DDR(LED_PORT) |= (1 << LED_PIN);
  #ifdef LED2_PORT
  DDR(LED2_PORT) |= (1 << LED2_PIN);
  #endif
  #ifndef DISABLE_HOST_TIMEOUT
  TIMER_HOST_MAX=HOST_TIMEOUT;
  #endif
  #ifndef DISABLE_TARGET_TIMEOUT
  TIMER_TARGET_MAX=TARGET_TIMEOUT;
  #endif
  #if defined(DEBUG_ON)
  DBG::debug(0x18,0xC0,0xFF, 0xEE);
  #endif
}

void SYS::setLED(void){
  PORT(LED_PORT) |= 1 << LED_PIN;
}

void SYS::clearLED(void){
  PORT(LED_PORT) &= ~(1 << LED_PIN);
}

void SYS::setVerLED(void){
  #ifdef LED2_PORT
  PORT(LED2_PORT) |= 1 << LED2_PIN;
  #endif
}

void SYS::clearVerLED(void){
  #ifdef LED2_PORT
  PORT(LED2_PORT) &= ~(1 << LED2_PIN);
  #endif
}


