#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
/* Função de tratamento de interrupção para a interrupção INT0 */
ISR(INT0_vect) {
	PINB |= (1 << PB5);               /* Escrever 1 em PIN quando PORT está em output troca o valor em PORT (toggle). */
    EIFR |= (1 << INTF0);            /* limpa a flag de interupção (opcional) */
}

int main(void) {
    /* Configure aqui o pino INT0 (mapeado no pino PD2 da porta D)
     * como entrada com pull-up (ver seção 14.2.1 da folha de dados do
     * ATMega328p) */
    
	DDRD &= ~(1 << PD2);  /* Configura pino 2 da porta D como entrada */
    PORTD |= (1 << PD2);  /* Configura pino 2 da porta D com resistor pull-up */

    DDRB |= (1 << PB5);   /* Configura pino 5 da porta B (led onboard) como saída */
    PORTB |= (1 << PB5);  /* Configura o pino do led em estado HIGH como default */

    /*
     * Configure aqui os registradores EICRA, EIFR e EIMSK aqui para
     * habilitar (ou não) a interrupção associada à mudança no pino
     * INT0 (veja Seção 13.2 da Folha de Dados do ATMega328p).
     */

    /* Registrador EICRA configura o tipo de gatilho de evento (borda de subida, descida ou
       mudança de nível lógico). Nesse caso, configurado para responder à borda de descida. */
    EICRA |= (1 << ISC01) | (0 << ISC00);

    /* Registrador EIFR armazena a flag de interrupção, quando uma interrupção é ativada. 
       Aqui, as flags de INT0 e INT1 são limpas. */
    EIFR |= (1 << INTF1) | (1 << INTF0);

    /* Registrador EIMSK configura qual pino de interrupção estará ativo. Aqui, só o pino
       INT0 será ativado. */
    EIMSK |= (0 << INT1) | (1 << INT0);

    /* Habilita globalmente as instruções */
    sei();

    /* Aguarda mudar algo nos estados do botão. */
    while (1) {
        // Aqui não precisa fazer nada, já que a cpu só vai mudar o estado do led quando houver uma interrupção,
		//	chamando a função de tratamento. 
    };

    return 0;
}