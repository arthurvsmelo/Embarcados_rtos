/* Necessário para funções e macros básicas */
#include <avr/io.h>
/* Os próximos dois includes são necessários quando se usa interrupçoes */
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define FOSC 16000000 // clock principal
#define BAUD 28800 // baud rate
#define _UBRR (FOSC / (16 * BAUD) - 1)   // freq transmissao/recepção ~ 480khz => UBRR0 = 34
volatile uint16_t counter;

void delay(uint8_t n);
void usart_init();
void usart_write(uint8_t byte);
uint8_t usart_read();
void send_signal(uint16_t signal);
void set_timer0();

ISR(USART_RX_vect) {
    /* Verifique se houve erro. Se houve transmita o byte 0xDE. Se não
       houve, ecoe o byte recebido */
        if(UCSR0A & ((1 << FE0)|(1 << DOR0))) 
            usart_write(0xDE);
        else{
            usart_write(usart_read());
        }
}

ISR(TIMER0_COMPA_vect) {
    /* Pisque o LED a cada 640 ms */
    if(counter <= 4000){ /* 160us * 4000 = 640ms */
        if(counter >= 3900)
            PORTB |= (1 << PB5); /* liga o led por aprox 16ms */
        counter++;
    }
    else{
        counter = 0;
        PORTB &= ~(1 << PB5); /* desliga o led */
        for(uint8_t i = 0; i < 20; i++)
            asm("nop");
    }
    /* Introduza um delay de 20 instruções NOP */
}

int main(void) {
    /*
     * Configure aqui a porta que controla o LED, iniciando-o apagado
     */
    cli();

    DDRB |= (1 << PB5) | (1 << PB1); /* pino led e pino de sinal como saída */
    PORTB &= ~(1 << PB5); /* led inicialmente desligado */
    PORTB |= (1 << PB1); /* pino de sinal em nivel alto */

    /* Configura o timer 0 para dar timeout a cada 160 us (dica: use
       modo CTC) */
    set_timer0();

    /* Configura a interface serial para receber dados no formato 8N1
       a 480 kHz */
    usart_init();

    /* Habilita as interrupções */
    sei();

    /* Gera o sinal digital correspondente a 0xAA55. Se usar loop,
       coloque um delay de 2 instruções NOP antes de mudar o sinal */
    while (1) {
        /* O seu código aqui */
        send_signal(0xAA55);
        /* Pequeno delay para separar os sinais individuais */
        delay(20);
    }

    return 0;
}

/*=================================================================================================================================*/

/*
 * Função para criar um pequeno delay variável de aproximadamente n
 * us. Máximo de 255 us
 */
void delay(uint8_t n) {
    uint8_t i;
    /* instrução "nop" leva 62,5 ns, 16 nop equivalem a 1 us*/
    while (n--)
        for (i=0; i<16; i++) 
            asm("nop");
}

void usart_init(){
    /* configurações das portas RX e TX*/
    DDRD &= ~(1 << PD1); // pino PD1 = RX (configurado como entrada)
    DDRD |= (1 << PD0);  // pino PD0 = TX (configurado como saída)
    /* configurações da USART */
    UCSR0A = 0x00; /* reset, baud rate normal */
	UBRR0H = ((uint8_t)(_UBRR >> 8) & 0xF); // conforme datasheet
	UBRR0L = ((uint8_t)(_UBRR) & 0xFF);
    /* habilita tx, rx e habilita flag de interrupção para RX */
	UCSR0B = (1 << RXEN0)|(1 << TXEN0)|(1 << RXCIE0);
    UCSR0B &= ~(1 << UCSZ02); 
	/* configura frame format = 8bits, 1 stop bit, sem paridade, assincrono */
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);
}

void usart_write(uint8_t byte){
	// espera o buffer de transmissão ficar vazio
	while(!(UCSR0A & (1 << UDRE0)));
	UDR0 = byte;   // coloca o dado no buffer e envia
}

uint8_t usart_read(){
    while(!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

void send_signal(uint16_t signal){
    /* start bit */
    PORTB &= ~(1 << PB1);
    delay(3);
    /* signal */
    for(uint8_t i = 15; i >= 0; i--){
        if((signal >> i) | 0x1)
            PORTB |= (1 << PB1);  /* se bit 1, nivel alto em PB1 */
        else
            PORTB &= ~(1 << PB1); /* se bit 0, nivel baixo em PB1 */
        delay(3);
    }
    /* stop bit */
    PORTB |= (1 << PB1);
}

void set_timer0(void){
    TIMSK0 |= (1 << OCIE0A);             /* habilita flag de interrupção por overflow */
    TIFR0 |= (1 << OCF0A);               /* limpa a flag de interrupção */
    TCCR0A |= (1 << WGM01);              /* configura modo CTC (clear timer on compare)*/
    TCCR0B |= (1 << CS01) | (1 << CS00); /* configura prescale de 64 */
    OCR0A = 0x28;                        /* contagem até 40 */

    /* limite de contagem: fosc/prescaler/(1/period) */
    /* para 160us -> f = 1/160u = 6250hz             */
    /* 16000000/64/6250 = 40                         */
}
