#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define FOSC 16000000UL                  /* clock arduino nano */
#define BAUD 28800U                      /* baud rate */ 
#define UBRR (FOSC / (16 * BAUD) - 1)   /* freq transmissao/recepção ~ 480khz => UBRR0 = 34 */
#define XON 0x11
#define XOFF 0x13
#define SYNC 0x7E
#define ESCAPE 0x7D

ISR(USART_RX_vect) {
}

ISR(USART_UDRE_vect) {
}

uint8_t write(uint8_t *buf, uint8_t n, int8_t close_packet){}

uint8_t read(uint8_t *buf, uint8_t n){}

void flow_off(){}

void flow_on(){}

uint8_t is_flow_on(){}

static char binary_string[] = {0x00, 0x01, 0x11, 0x02, 0x13, 0x04, 0x7E, 0x05, 0x7D, 0x06};

uint8_t buffer[81];

void usart_init(){
    /* configurações das portas RX e TX*/
    DDRD &= ~(1 << PD1); // pino PD1 = RX (configurado como entrada)
    DDRD |= (1 << PD0);  // pino PD0 = TX (configurado como saída)
    /* configurações da USART */
    UCSR0A = 0U; /* reset, baud rate normal */
	UBRR0H = ((uint8_t)(UBRR >> 8) & 0xF); // conforme datasheet
	UBRR0L = ((uint8_t)(UBRR) & 0xFF);
    /* habilita tx, rx e habilita flag de interrupção para RX e TX */
	UCSR0B = (1 << RXEN0)|(1 << TXEN0)|(1 << RXCIE0)|(1 << TXCIE0);
    UCSR0B &= ~(1 << UCSZ02); 
	/* configura frame format = 8bits, 1 stop bit, sem paridade, assincrono */
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);
}

void timer0_init(){
    TIMSK0 |= (1 << OCIE0A);             /* habilita flag de interrupção por overflow */
    TIFR0 |= (1 << OCF0A);               /* limpa a flag de interrupção */
    TCCR0A |= (1 << WGM01);              /* configura modo CTC (clear timer on compare)*/
    TCCR0B |= (1 << CS01) | (1 << CS00); /* configura prescale de 64 */
    OCR0A = 40U;                   
}

/* função de delay em milissegundos usando o timer0 */
void delay(unsigned int n){

}

int main(void) {
    /* Faça a configuração do que for necessário aqui */
    /* Seu código aqui */
    DDRB |= (1 << PB5);   /* Configura pino 5 da porta B (led onboard) como saída */
    PORTB &= ~(1 << PB5);  /* Configura o pino do led em estado LOW como default */

    sei();

    /* Implemente os testes aqui. Veja o texto para os detalhes */

    /* Loop infinito necessário em qualquer programa para
       embarcados */
    while (1) {
    }

    return 0;
}
