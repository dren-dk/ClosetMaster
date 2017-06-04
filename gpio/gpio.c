#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>

#include "Descriptors.h"

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>
#include <LUFA/Drivers/Peripheral/Serial.h>

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
  {
    .Config =
    {
      .ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
      .DataINEndpoint           =
      {
	.Address          = CDC_TX_EPADDR,
	.Size             = CDC_TXRX_EPSIZE,
	.Banks            = 1,
      },
      .DataOUTEndpoint =
      {
	.Address          = CDC_RX_EPADDR,
	.Size             = CDC_TXRX_EPSIZE,
	.Banks            = 1,
      },
      .NotificationEndpoint =
      {
	.Address          = CDC_NOTIFICATION_EPADDR,
	.Size             = CDC_NOTIFICATION_EPSIZE,
	.Banks            = 1,
      },
    },
  };

/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs.
 */
static FILE USBSerialStream;
static FILE UI;

void setExtraLED(unsigned char on) {
  if (on) {
    PORTD &=~ _BV(PD5);
  } else {
    PORTD |= _BV(PD5);
  }
}

void setUSBReadyLED(unsigned char on)  {
  if (on) {
    PORTB &=~ _BV(PB0);
  } else {
    PORTB |= _BV(PB0);
  }
}

void frontLED(uint8_t leds) {
  fprintf(&UI, "!l%d\r", leds);
}

void frontLCD(const char *fmt, ...) {
  fprintf(&UI, "!d");
  va_list ap;
  va_start(ap, fmt);
  vfprintf_P(&UI, fmt, ap);
  va_end(ap);
  fprintf(&UI, "\r");
}

typedef struct UIState {
  uint16_t voltage;
  uint16_t current;
  uint8_t buttons;
} UIState;

struct UIState uiState;

void handleLine(char *line) {
  uint16_t voltage;
  uint16_t current;
  uint8_t buttons;
  if (sscanf_P(PSTR("%d %d %d"), line, &voltage, &current, &buttons) == 3) {
    uiState.voltage = voltage;
    uiState.current = current;
    uiState.buttons = buttons;
  }
}

void getUIState(UIState *uis) {
  cli();
  memcpy(uis, &uiState, sizeof(uiState));
  sei();
}


#define MAX_BUF 32
char buf[MAX_BUF];
uint8_t bufLen = 0;

ISR(USART1_RX_vect) {
  char ch = fgetc(&UI);
  if (!ch) {
    return;
  }

  if (ch == '\r') {
    handleLine(buf);
    
  } else if (ch == '\n') {
    // Ignore.
    
  } else {
    if (bufLen > MAX_BUF) {
      bufLen = 0;
    }
    buf[bufLen++] = ch;
    buf[bufLen] = 0;
  }
}


int main(void) {
  USB_Init();
  Serial_Init(115200, false);

  // Enable RX interrupts
  sei();
  UCSR1B |= (1 << RXCIE1);
  
  DDRB = _BV(PB0); // USB LED
  DDRD = _BV(PD5); // Extra LED

  setExtraLED(0);
  setExtraLED(0);
  setUSBReadyLED(0);
  frontLED(16);
  
  CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);
  Serial_CreateStream(&UI);
  
  GlobalInterruptEnable();

  unsigned char cmdLength = 0;
  char cmd[17];
  
  fprintf(&USBSerialStream, "Hello\r\n");

  uint8_t leds = 0;
  uint16_t loopy = 0;
  while (1) {
        
    if (loopy++ > 10000) {
      leds++;
      frontLED(leds);
      frontLCD(PSTR("%32d"), leds);
      loopy = 0;
    }
        
    int16_t ch = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
    if (ch > 0) {
      if (ch == '\b') {
	if (cmdLength > 0) {
	  cmd[--cmdLength]=0;
	  fputs("\b \b", &USBSerialStream);
	}
	
      } else if (ch == '\r' || ch == '\n') {
	fputs("\r\n", &USBSerialStream);
      
	cmdLength = 0;
	cmd[cmdLength] = 0;
	fprintf(&USBSerialStream, "> ");
      
      } else {
	if (cmdLength < 16) {
	  cmd[cmdLength++] = ch;
	  cmd[cmdLength] = 0;
	  fprintf(&USBSerialStream, "%c", ch);
	}	
      }

    }
    
    CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
    USB_USBTask();
  }
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
  setUSBReadyLED(1);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
  setUSBReadyLED(0);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
  bool ConfigSuccess = true;

  ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
  CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** CDC class driver callback function the processing of changes to the virtual
 *  control lines sent from the host..
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo)
{
  /* You can get changes to the virtual CDC lines in this callback; a common
     use-case is to use the Data Terminal Ready (DTR) flag to enable and
     disable CDC communications in your application when set to avoid the
     application blocking while waiting for a host to become ready and read
     in the pending data from the USB endpoints.
  */
  //bool HostReady = (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR) != 0;
}
