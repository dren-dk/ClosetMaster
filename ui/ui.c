#include <ctype.h>
#include <inttypes.h>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include <avr/wdt.h> 
#include <avr/interrupt.h>
#include <avr/eeprom.h> 
#include <avr/pgmspace.h>
#include <avr/eeprom.h> 

#include "mstdio.h"
#include "adchelper.h"
#include "lcd.h"

// We don't really care about unhandled interrupts.
EMPTY_INTERRUPT(__vector_default)

#define EEPROM_MAGIC_VALUE 0xBEEF

#define EEPROM_MAGIC ((uint16_t *)0)
#define EEPROM_ADC2MV ((uint16_t *)2)
#define EEPROM_ADC2MA ((uint16_t *)4)
#define EEPROM_CONTRAST ((uint16_t *)6)
#define EEPROM_OWNER ((void *)8)

#define OWNER_LENGTH 16
char owner[OWNER_LENGTH+1];

void setContrast(unsigned char value) {
  if (value) {    
    OCR1AL = value;
    TCCR1A |= _BV(COM1A1);
  } else {
    TCCR1A &=~ _BV(COM1A1);
  }
}

void setBacklight(unsigned char value) {
  if (value) {    
    OCR2A = value;
    TCCR2A |= _BV(COM2A1);
  } else {
    TCCR2A &=~ _BV(COM2A1);
  }
}

void initLEDs() {
  DDRB  |= _BV(PB5);  // LED output
  DDRC |= _BV(PC2) | _BV(PC3) | _BV(PC4) | _BV(PC5); // left front panel LEDs
  DDRD |= _BV(PD7); // test LED
}

void statusLED(char on) {
  if (on) {
    PORTB |= _BV(PB5);   
  } else {
    PORTB &=~ _BV(PB5);   
  }
}

void frontLEDs(uint8_t leds) {
  if (leds & _BV(0)) {
    PORTC &=~ _BV(PC2);       
  } else {
    PORTC |= _BV(PC2);
  }

  if (leds & _BV(1)) {
    PORTC &=~ _BV(PC3);       
  } else {
    PORTC |= _BV(PC3);
  }

  if (leds & _BV(2)) {
    PORTC &=~ _BV(PC4);       
  } else {
    PORTC |= _BV(PC4);
  }

  if (leds & _BV(3)) {
    PORTC &=~ _BV(PC5);       
  } else {
    PORTC |= _BV(PC5);
  }

  if (leds & _BV(4)) {
    PORTD &=~ _BV(PD7);       
  } else {
    PORTD |= _BV(PD7);
  }
}

uint8_t readButtons() {
  uint8_t r = PINB & 5;

  if (getADC(7) > 255) {
    r |= 2;
  }

  return r;
}

void lcdInit() {
  lcd_init(LCD_DISP_ON);
  lcd_clrscr();	// initial screen cleanup
  lcd_home();
}

void lcdHello(char frame) {  
  if (frame & 4) {
    lcd_gotoxy(0,0);
    lcd_puts_p(PSTR("   Closet Ctrl  "));

  } else {
    lcd_gotoxy(0,1);
    lcd_puts(owner);
  }

  lcd_home();
}

char menu = 0;
unsigned int adc2mv;
unsigned int adc2ma;
unsigned int currentCalibration;

unsigned int voltage;
unsigned int current;

void readVoltageAndCurrent() {
  unsigned long aadc = getOsADC(0);
  current = (aadc*adc2ma) >> 8;

  unsigned long vadc = getOsADC(1);
  voltage = (vadc*adc2mv) >> 8;
}

void lcdReadout(char watt) {
  int ai =  current / 1000;
  int ad = (current % 1000) / 10;
  int vi =  voltage / 1000;
  int vd = (voltage % 1000) / 10;

  unsigned long uw = current;
  uw *= voltage;
  uw /= 1000;
  unsigned long mw = uw;
  
  int wi =  mw/1000;
  int wd = (mw%1000)/100;

  char buffy[27];
  memset(buffy, 0, sizeof(buffy));

  if (watt) {
    msprintf(buffy, PSTR("%2d.%02d V %4d.%d W"), vi,vd, wi,wd);
  } else {
    msprintf(buffy, PSTR("%2d.%02d V %3d.%02d A"), vi,vd, ai,ad);
  }

  while (strlen(buffy) < OWNER_LENGTH) {
    strcat(buffy, " ");
  }

  lcd_gotoxy(0,0);
  lcd_puts(buffy);

  lcd_gotoxy(0,1);
  lcd_puts(owner);

  lcd_home();
}

int contrast;
unsigned char backlight;
uint8_t cmdLength = 0;
#define MAX_CMD 35
char cmd[MAX_CMD];
char lcdState = 0;

uint8_t atob(char* str) {
  uint8_t r = 0;
  char *ch = str;
  while (*ch >= '0' && *ch <= '9') {
    r *= 10;
    r += *ch-'0';
    ch++;
  }

  return r;
}

void printMenu() {
    mprintf(PSTR("Menu:\n +/-: Contrast: %d\n v: Voltage calibration %d\n a: Current calibration %d\n o: Owner: %s\n q: Quit\n"),
	    contrast, adc2mv, adc2ma, owner);
}

void handleMenu() {
  char ch = mchready() ? mgetch() : 0;

  if (menu == 0) {
    if (ch == '\r') {
      menu = 1;
    } else if (ch == '!') {
      menu = 11;
      cmdLength = 0;
     
    } else if (ch) {
      mprintf(PSTR("Ignoring: %c\n"), ch);
    }     

  } else if (menu == 1 || menu==10) {
    printMenu();
    if (menu == 10) {
      menu = 0;
    } else {
      menu = 2;
    }      

  } else if (menu == 2) {
    if (ch == '-' || ch == '+') {
      if (ch == '-') {
	contrast -= 1;
	if (contrast < 0) {
	  contrast = 0;
	}
      } else {
	contrast += 1;
	if (contrast > 60) {
	  contrast = 60;
	}
      }

      menu = 1;

      setContrast(contrast);

      eeprom_write_word(EEPROM_CONTRAST, contrast);

    } else if (ch == 'v') {
      menu = 3;

    } else if (ch == 'a') {
      menu = 5;

    } else if (ch == 'o') {
      menu = 7;

    } else {
      menu = 0;
    }

  } else if (menu == 3 || menu == 5) {
    if (menu == 3) {
      mprintf(PSTR("Enter voltage in mV: "));
      menu = 4;
    } else {
      mprintf(PSTR("Enter current in mA: "));
      menu = 6;
    }
    currentCalibration = 0;
      
  } else if (menu == 4 || menu == 6) {
      
    if (ch == 8) {
      if (currentCalibration) {
	currentCalibration /= 10;
	mputchar(ch);
	mputchar(' ');
	mputchar(ch);	  
      }

    } else if (ch >= '0' && ch <= '9') {
      currentCalibration *= 10;
      currentCalibration += ch - '0';
      mputchar(ch);
	
    } else if (ch == '\r') {

      if (currentCalibration < 1000) {
	mputs("\r\nError: Value entered is too low for calibration, ignoring.\r\n");
	menu = 1;

      } else if (menu == 4) {
	unsigned adc = getOsADC(1);

	if (adc < 100) {
	  mputs("\r\nError: Voltage too low for calibration\r\n");
	  menu = 3;

	} else {
	  mputs("\r\nStored voltage calibration\r\n");
	
	  unsigned long mv = currentCalibration;
	  mv <<= 8;
	  mv /= adc;
	  adc2mv = mv;
	
	  eeprom_write_word(EEPROM_ADC2MV, adc2mv);
	}
      } else {

	unsigned int adc = getOsADC(0);
	if (adc < 20) {
	  mputs("\r\nError: Current too low for calibration\r\n");
	  menu = 5;

	} else {
	  mputs("\r\nStored current calibration\r\n");
	
	  unsigned long ma = currentCalibration;
	  ma <<= 8;
	  ma /= adc;
	  adc2ma = ma;
	
	  eeprom_write_word(EEPROM_ADC2MA, adc2ma);
	}
      }
	
      menu = 1;
    } 

  } else if (menu == 7) {
    mprintf(PSTR("Enter owner string: >"));
    mputs(owner);
    mputchar('<');
    mputchar(8);
    menu = 8;
      
  } else if (menu == 8) {
            
    int ownerLen = strlen(owner);

    if (ownerLen && ch == 8) {
      owner[ownerLen-1] = 0;
      mputchar(ch);
      mputchar(' ');
      mputchar(ch);
	
    } else if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || 
	       ch == ' ' || ch == '-' || ch=='.') {
	
      if (ownerLen < OWNER_LENGTH) {
	owner[ownerLen++] = ch;
	owner[ownerLen] = 0;
	mputchar(ch);
      }
	
    } else if (ch == '\r') {
	
      while (ownerLen < OWNER_LENGTH) {
	owner[ownerLen++] = ' ';
	owner[ownerLen] = 0;
      }
	
      eeprom_write_block(owner, EEPROM_OWNER, OWNER_LENGTH); 
      menu = 1;
    }
    
  } else if (menu == 11) {

    if (ch == '\r') {
      if (cmdLength  < 2) {
	mprintf(PSTR("Error: Command too short: %s\r\n"), cmd);      
      } else {
	char c = cmd[0];
	if (c == 'D') {
	  lcdState = atob(cmd+1);
	  
	} else if (c == 'd') {
	  if (cmdLength != 33) {
	    mprintf(PSTR("Error: Bad display content length '%s' %d != 32\r\n"), cmd+1, cmdLength-1);
	  } else {
	    lcdState = 3;
	    lcd_gotoxy(0,0);
	    lcd_putsn(cmd+1, 16);	
	    lcd_gotoxy(0,1);
	    lcd_putsn(cmd+1+16, 16);
	    //!d0123456789abcdef0123456789abcdef  
	  }
	} else if (c == 'l') {
	  frontLEDs(atob(cmd+1));

	} else if (c == 'b') {
	  setBacklight(atob(cmd+1));

	} else if (c == 'c') {
	  setContrast(atob(cmd+1));
	  
	} else {
	  mprintf(PSTR("Error: Unknown command %s\r\n"), cmd);      
	}
      }      
      cmdLength = 0;
      menu = 0;
      
    } else if (ch) {
    
      if (cmdLength < MAX_CMD-1) {
	cmd[cmdLength++] = ch;
	cmd[cmdLength] = 0;
      } else {
	mprintf(PSTR("Error: Command too long: %s\r\n"), cmd);      
	cmdLength = 0;
	menu = 0;
      }
    }
  }
}

ISR(USART_RX_vect) {
  handleMenu();
  if (menu) {
    handleMenu();
  }
}

int main(void) {
  wdt_enable(WDTO_4S);
  statusLED(1);
  OSCCAL=0x8a; // Internal oscilator calibration, tuned via uart

  initLEDs();

  // Set up timer 2 for fast PWM mode & the highest frequency available
  TCCR2A = _BV(WGM20);
  TCCR2B = _BV(CS20);
  setBacklight(0);

  DDRB  |= _BV(PB1);  // Contrast PWM OC1A
  DDRB  |= _BV(PB3);  // Backlight PWM OC2A

  initADC();

  muartInit();
  mprintf(PSTR("Hello from Flemming Frandsen:)\n"));
  printMenu();
  menu=0;
    
  _delay_ms(100);

  lcdInit();
  _delay_ms(100);

  // Enable RX interrupts
  sei();
  UCSR0B |= (1 << RXCIE0);

  // Set up timer 1 for fast PWM mode & the highest frequency available
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10);

  unsigned int eepromMagic = eeprom_read_word(EEPROM_MAGIC);
  if (eepromMagic != EEPROM_MAGIC_VALUE) {
    eeprom_write_word(EEPROM_MAGIC,  EEPROM_MAGIC_VALUE);
    eeprom_write_word(EEPROM_ADC2MV, 6365);
    eeprom_write_word(EEPROM_ADC2MA, 11988);
    eeprom_write_word(EEPROM_CONTRAST, 30);
    strcpy_P(owner, PSTR(" Not calibrated "));
    eeprom_write_block(owner, EEPROM_OWNER, OWNER_LENGTH);    
  } 

  adc2mv   = eeprom_read_word(EEPROM_ADC2MV);
  adc2ma   = eeprom_read_word(EEPROM_ADC2MA);
  contrast = eeprom_read_word(EEPROM_CONTRAST);
  memset(owner, 0, OWNER_LENGTH+1);
  eeprom_read_block(owner, EEPROM_OWNER, OWNER_LENGTH); 
  
  setContrast(contrast);

  statusLED(0);

  frontLEDs(0xff);

  uint16_t polly = 0;
  unsigned char frame = 0;
  uint8_t oldButtons = 0;
  while(1) {
    if (backlight < 255) {
      backlight++;
      setBacklight(backlight);
    }

    uint8_t buttons = readButtons();
    if (polly++ > 500) {
      polly=0;
    }

    if (!polly || buttons != oldButtons) {
      if (polly) {
	polly=1;
      }
      readVoltageAndCurrent();
      frame++;
      statusLED(frame & 1);
      oldButtons=buttons;

      if (!menu) {
	mprintf(PSTR("%d %d %d\n"), voltage, current, buttons);
      }
   
      if (lcdState == 0) {
	lcdHello(frame);      

	if (frame > 8) {
	  ++lcdState;
	}

      } else if (lcdState == 1 || lcdState == 2) {

	lcdReadout(lcdState == 2);      
      
	if (!(frame & 15)) {
	  if (++lcdState > 2) {
	    lcdState = 1;
	  }
	}
      }      
    }
    
    _delay_ms(1);

    wdt_reset();
  }	
}
