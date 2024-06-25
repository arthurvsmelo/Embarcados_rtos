#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define F_CPU 8000000UL                         // 8MHz - prevents default 1MHz
#define USART_BAUDRATE 38400UL                  //300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)
#define BUFFER_SIZE 255

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif
#define pinstate(sfr,bit) ((sfr) & (1<<(bit)))

int coms = 0;                                   // Do we have communication, reset by timer every x milliseconds
int BufferI = 0;                                // Buffer position
char Buffer[BUFFER_SIZE];                       // Receive buffer
char cmd[BUFFER_SIZE];                          // Command variable
int sendheartbeat = 0;
char clearandhome[7] = {'\e', '[', '2', 'J', '\e', '[', 'H'};
#define PROX_SW_EXPANDED            PINB1       // PIN2 POSITION WHEN EXPANDED, READY FOR START
#define MTR_RADIAL_CLOCKWISE        PINC3

void USARTWriteChar(char data) {
    while(!(UCSRA & (1<<UDRE0))) { }             //Do nothing until the transmitter is ready
    UDR0=data;                                   //Now write the data to USART buffer
}

void USARTWriteLine ( const char *str ) {
    while (*str) {
        USARTWriteChar(*str);
        str++;
    }
    USARTWriteChar(0x0D);                       //Carriage return, 13dec, ^M
    USARTWriteChar(0x0A);                       //Line feed      , 10dec, ^J
}
ISR(USART_RXC_vect) {
    char ReceivedByte;
    ReceivedByte = UDR;                         // Fetch the received byte value into the variable "ReceivedByte"
    coms = 1;                                   // We just received something
    TCNT1 = 0;                                  // reset counter
    if (ReceivedByte == 0x0A) {                 // Use linefeed as command separator
        Buffer[BufferI] = '\0';                 // String null terminator
        memcpy(cmd, Buffer, BufferI + 1);       // Copy complete command to command variable    
        BufferI = 0;                            // Reset buffer position
    } else if (ReceivedByte > 0x1F)             // Ignore Control Characters
        Buffer[BufferI++] = ReceivedByte;       // Add received byte to buffer
    #ifdef DEBUG
    switch (ReceivedByte) {
        case 'q': USARTWriteLine(Buffer); break;
        case 'w': USARTWriteLine(cmd); break;
    }    
    #endif  
}
ISR(TIMER1_OVF_vect) {
    coms = 0;                                       // Lost communication
}

ISR(TIMER0_OVF_vect) {
    if (sendheartbeat) USARTWriteChar(0x05);        // 15 times / second, 66,67ms
}

int main(void) {
    TIMSK=(1<<TOIE0) | (1<<TOIE1);      
    TCNT0=0x00;         
    TCCR0 = (1<<CS02) | (1<<CS00);  
    TCCR1B |= (1 << CS10) | (1 << CS12);

    UBRRL = BAUD_PRESCALE;                      // Load lower 8-bits of the baud rate value into the low byte of the UBRR register
    UBRRH = (BAUD_PRESCALE >> 8);               // Load upper 8-bits of the baud rate value into the high byte of the UBRR register
    UCSRB = (1<<RXEN)|(1<<TXEN | 1<< RXCIE);    // Turn on the transmission and USART Receive Complete interrupt (USART_RXC)
    UCSRC |= (1<<URSEL)|(1<<UCSZ0)|(1<<UCSZ1);  // Set frame format: 8data, 1stop bit

    DDRB = 0x00;                                // Set all pins on PORTB for output
    DDRC = 0xff;                                // Set all pins on PORTC for output
    sei();                                      // Enable the Global Interrupt Enable flag so that interrupts can be processed

    while(1) {
        if (!coms) PORTC = 0x00;                // If not coms turn of everything   
        else if (cmd[0] != '\0') {              // We have coms, check for new command
            if (strcmp("help", cmd) == 0) USARTWriteLine("liberate tuteme ex inferis");
            else if (strcmp("sync", cmd) == 0) sendheartbeat = !sendheartbeat;
            else if (strcmp("clear", cmd) == 0) USARTWriteLine(clearandhome);
            else if (strcmp("expand", cmd) == 0) {
                if (!pinstate(PINB, PROX_SW_EXPANDED))
                    sbi(PORTC, MTR_RADIAL_CLOCKWISE);
            } else if (!(strcmp("", cmd) == 0)) USARTWriteLine("Unknown command");
            cmd[0] = '\0';                      // Reset command string with null terminator.
        }
    }
}