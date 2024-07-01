#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>

#define FOSC 16000000UL                  /* clock arduino nano */
#define BAUD 9600U                       /* baud rate */ 
#define UBRR (FOSC / (16 * BAUD) - 1)    /* freq transmissao/recepção */
#define XON    0x11
#define XOFF   0x13
#define SYNC   0x7E
#define ESCAPE 0x7D
#define BUFFER_SIZE 64

typedef struct {
    uint8_t buffer[BUFFER_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} CircularBuffer;

static uint8_t binary_string[] = {0x00, 0x01, 0x11, 0x02, 0x13, 0x04, 0x7E, 0x05, 0x7D, 0x06};
static uint8_t str1[] = "Universidade Federal de Pernambuco\nDepartamento de Eletronica e Sistemas";
static uint8_t str2[] = "Desenvolvimento de sistemas embarcados";
static uint8_t readBuffer[256] = {0};
static volatile uint8_t flow = 0, esc = 0, receiver_flow = 1;
static volatile unsigned long milisecond_count;
CircularBuffer rxBuffer, txBuffer;

void initBuffer(CircularBuffer *cb);
uint8_t isFull(CircularBuffer *cb);
uint8_t isEmpty(CircularBuffer *cb);
uint8_t enqueue(CircularBuffer *cb, uint8_t item);
uint8_t dequeue(CircularBuffer *cb, uint8_t *item);
uint8_t write(uint8_t *buf, uint8_t n, int8_t close_packet);
uint8_t read(uint8_t *buf, uint8_t n);
void flow_off();
void flow_on();
uint8_t is_flow_on();
void USART_Transmit(uint8_t data);
uint8_t USART_Receive(void);
void usart_init();
void initTimer1();
void delay(unsigned long ms);

int main(void) {
    uint8_t i, j = 0, k = 5;
    uint16_t received_bytes = 0;
    DDRB |= (1 << PB5);   /* Configura pino 5 da porta B (led onboard) como saída */
    PORTB &= ~(1 << PB5);  /* Configura o pino do led em estado LOW como default */

    usart_init();
    initTimer1();
    sei();

    /* Implemente os testes aqui. Veja o texto para os detalhes */
    
    /* Teste 2 */
    write(str1, strlen(str1), (int8_t)1);
    /* Teste 3 */
    i = strlen(str1);
    while(i){
        if(i < 10){
            write(str1, (uint8_t)10, (int8_t)1);  /* fecha o pacote */
        }
        else{
            i -= (write(str1, (uint8_t)10, (int8_t)0));  /* continua enviando o pacote sem fechar */
        }
    }
    /* Teste 4 */
    while(received_bytes <= 300){
        received_bytes += (read(readBuffer, 1) - 1);  /* read retorna 2 enquanto não receber caractere de sincronismo */
    }
    flow_off();
    if(!is_flow_on()){
        /* pisca o led por 5s a uma taxa de 1hz */
        while(k--){
            PORTB ^= (1 << PB5);
            delay(1000);
        }
    }
    else{
        /* acende o led por 5 seg continuamente */
        PORTB |= (1 << PB5);
        delay(5000);
        PORTB &= ~(1 << PB5);
    }
    /* Teste 5 */
    PORTB &= ~(1 << PB5); /* apaga o led */
    while(received_bytes <= 300){
        received_bytes += (read(readBuffer, 1) - 1);
    }
    flow_off();
    if(!is_flow_on()){
        /* pisca o led por 5s a uma taxa de 1hz */
        while(k--){
            PORTB ^= (1 << PB5);
            delay(1000);
        }
    }
    else{
        /* acende o led por 5 seg continuamente */
        PORTB |= (1 << PB5);
        delay(5000);
        PORTB &= ~(1 << PB5);
    }
    /* Teste 6 */
    PORTB &= ~(1 << PB5); /* apaga o led */
    /* envia a string binária sem fechar o pacote */
    while(1){
        if(write(binary_string, strlen(binary_string), (int8_t)0) == 0){
            PORTB |= (1 << PB5); /* acende o led enquanto a função write retornar 0 (se o fluxo estiver fechado)*/
        }
        else if(write(binary_string, strlen(binary_string), (int8_t)0) == strlen(binary_string)){
            break; /* só sai do while quando toda a string for enviada */
        }
        else{
            PORTB &= ~(1 << PB5); /* apaga o led se a função write retornar os bytes enviados */
        }  
    }
    /* Loop infinito necessário em qualquer programa para embarcados */
    while (1) {
        /* Passo 7 (a) */
        read(readBuffer, 254);
        /* strcmp devolve 0 se as strings forem iguais */
        if(!strcmp(str2, readBuffer)){
            PORTB |= (1 << PB5);  /* acende o led se forem iguais */
        }
        else{
            PORTB &= ~(1 << PB5);  /* apaga o led se forem diferentes */
        }
        /* Passo 7 (b) */
        while(read(readBuffer, 10) == 11); /* função read retorna n+1 se pacote não foi finalizado */
        /* strcmp devolve 0 se as strings forem iguais */
        if(!strcmp(str2, readBuffer)){
            PORTB |= (1 << PB5);  /* acende o led se forem iguais */
        }
        else{
            PORTB &= ~(1 << PB5);  /* apaga o led se forem diferentes */
        }
    }

    return 0;
}

/* =============================================== Funções ============================================================ */

void initBuffer(CircularBuffer *cb) {
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
}

uint8_t isFull(CircularBuffer *cb) {
    if(cb->count == BUFFER_SIZE) return 1;
    else return 0;
}

uint8_t isEmpty(CircularBuffer *cb) {
    if(cb->count == 0) return 1;
    else return 0;
}

uint8_t enqueue(CircularBuffer *cb, uint8_t item) {
    if (isFull(cb)) {
        return 0; /* buffer está cheio */
    }
    cb->buffer[cb->head] = item;
    cb->head = (cb->head + 1) % BUFFER_SIZE; /* se o índice chegar ao fim do buffer, retorna ao começo */
    cb->count++;
    return 1;
}

uint8_t dequeue(CircularBuffer *cb, uint8_t *item) {
    if (isEmpty(cb)) {
        return 0; /* buffer está vazio */
    }
    *item = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % BUFFER_SIZE; /* se índice chegar ao fim do buffer, retorna ao começo */
    cb->count--;
    return 1;
}

ISR(USART_RX_vect) {
    uint8_t data = UDR0;
    /* caractere de escape recebido: descarta o caractere e sobe a flag de escape */
    if(data == ESCAPE)
    {
        esc = 1;
    }
    /* caractere XON recebido sem um caractere de escape o precedendo: descarta o caractere de controle e ativa o fluxo */
    else if(data == XON && esc == 0){
        flow = 1;
    }
    /* caractere XOFF recebido sem um escape o precedendo: descarta o caractere e desliga o fluxo */
    else if(data == XOFF && esc == 0){
        flow = 0;
    }
    /* caractere de sincronismo recebido, sem caractere de escape o precedendo: informa o fim do pacote */
    else if(data == SYNC && esc == 0){
        enqueue(&rxBuffer, data);
    }
    /* caractere especial recebido, sendo precedido de escape: adiciona o valor ao buffer */
    else if((data == XON    && esc == 1) ||
            (data == XOFF   && esc == 1) ||
            (data == ESCAPE && esc == 1) ||
            (data == SYNC   && esc == 1)){
                esc = 0;
                enqueue(&rxBuffer, data);
    }
    else{
        enqueue(&rxBuffer, data);
    }
}

ISR(USART_UDRE_vect) {
    /* se o buffer de transmissão estiver vazio, desabilita a interrupção UDRIE0 */
    if (isEmpty(&txBuffer)) {
        UCSR0B &= ~(1 << UDRIE0);
    }
    /* se houver dados no buffer de transmissão, envia o dado mais antigo */
    else{
        uint8_t data;
        dequeue(&txBuffer, &data);
        UDR0 = data;
    }
}

ISR(TIMER1_COMPA_vect)
{
    milisecond_count++;
}

uint8_t write(uint8_t *buf, uint8_t n, int8_t close_packet){
    uint8_t data, counter = 0;
    static uint8_t position = 0;
    while(n--){
        if(!is_flow_on()){
            return 0;
        }
        else if(position > strlen(buf) || buf[position] == '\0'){
            position = 0;
            break;
        }
        else{
            data = buf[position];
            USART_Transmit(data);
            counter++;
            position++;
        }
    }
    if(close_packet){
        USART_Transmit(SYNC);
        position = 0;
    }
    return counter;
}

uint8_t read(uint8_t *buf, uint8_t n){
    uint8_t data, counter = 0;
    static uint8_t position = 0;
    while(n--){
        data = USART_Receive();
        if(!receiver_flow){
            return 0;
        }
        else if(data == SYNC){
            buf[position] = '\0';  /* fecha a string */ 
            position = 0;  /* fim do pacote, reseta a posição do índice do buffer */
            return counter;
        }
        else if(position > strlen(buf)){
            buf[position] = '\0';  /* se buffer estourar, fecha a string e reseta o índice */
            position = 0;
            break;
        }
        else{
            buf[position] = data; 
            counter++;
            position++;
        }
    }
    return n + 1;        
}

void flow_off(){
    /* envia o caractere de controle XOFF para desabilitar o fluxo de dados no outro dispositivo */
    USART_Transmit(XOFF);
    receiver_flow = 0;
}

void flow_on(){
    /* envia o caractere de controle XON para habilitar o fluxo no outro dispositivo */
    USART_Transmit(XON);
    receiver_flow = 1;
}

uint8_t is_flow_on(){
    /* verifica se houve o recebimento de um caractere de controle de fluxo */
    if(flow) 
        return (uint8_t)1;
    else
        return (uint8_t)0;
}

void USART_Transmit(uint8_t data){
    /* não faz nada enquanto o buffer TX estiver cheio */
    while (isFull(&txBuffer));
    /* se buffer tx tiver espaço, enfileira o byte para transmissão */
    enqueue(&txBuffer, data);
    /* habilita a interrupção para o envio de um byte */
    UCSR0B |= (1 << UDRIE0);
}

uint8_t USART_Receive(void){
    uint8_t data;
    /* não faz nada se o buffer de leitura estiver vazio (sem dados para ler)*/
    while (isEmpty(&rxBuffer));
    /* quando tiver dados, desenfileira e retorna o byte */
    dequeue(&rxBuffer, &data);
    return data;
}

void usart_init(){
    /* configurações das portas RX e TX*/
    DDRD &= ~(1 << PD1); // pino PD1 = RX (configurado como entrada)
    DDRD |= (1 << PD0);  // pino PD0 = TX (configurado como saída)
    /* configurações da USART */
    UCSR0A = 0U; /* reset, baud rate normal */
	UBRR0H = ((uint8_t)(UBRR >> 8) & 0xF); // conforme datasheet
	UBRR0L = ((uint8_t)(UBRR) & 0xFF);
    /* habilita tx, rx e habilita flag de interrupção para RX e TX */
	UCSR0B = (1 << RXEN0)|(1 << TXEN0)|(1 << RXCIE0)|(1 << TXCIE0|(1 << UDRIE0));
    UCSR0B &= ~(1 << UCSZ02); 
	/* configura frame format = 8bits, 1 stop bit, sem paridade, assincrono */
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);
    /* inicialização dos buffers rx e tx */
    initBuffer(&rxBuffer);
    initBuffer(&txBuffer);
}

void initTimer1(){ 
    TCCR1B |= (1 << WGM12);               /* Configura o Timer1 no modo CTC (Clear Timer on Compare Match) */
    TIMSK1 |= (1 << OCIE1A);              /* Habilita a interrupção do Timer1 no compare match A */
    TCCR1B |= (1 << CS11) | (1 << CS10);  /* Define o prescaler para 64 e inicia o Timer1 */
    OCR1A = 249;                          /* Define o valor de comparação para gerar uma interrupção a cada 1 ms (16MHz / 64 prescaler / 1000 - 1) */               
}

/* função de delay em milissegundos usando o timer1 */
void delay(unsigned long ms){
    unsigned long start_time = milisecond_count;
    while(milisecond_count - start_time < ms);
}