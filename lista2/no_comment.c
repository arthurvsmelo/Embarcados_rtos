#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>

#define FOSC 16000000UL                  
#define BAUD 9600U                        
#define UBRR (FOSC / (16 * BAUD) - 1)    
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
void flow_off(void);
void flow_on(void);
uint8_t is_flow_on(void);
void USART_Transmit(uint8_t data);
uint8_t USART_Receive(void);
void usart_init(void);
void initTimer1(void);
void delay(unsigned long ms);

int main(void) {
    uint8_t i, j = 20, k = 5, trash_data;
    uint16_t received_bytes = 0;
    DDRB |= (1 << PB5);   
    PORTB &= ~(1 << PB5);  

    usart_init();
    initTimer1();
    sei();

    
    
    
    write(str1, strlen((const char *)str1), (int8_t)1);
    
    i = strlen((const char *)str1);
    while(i){
        if(i < 10){
            write(str1, (uint8_t)10, (int8_t)1);  
        }
        else{
            i -= (write(str1, (uint8_t)10, (int8_t)0));  
        }
    }
    
    while(received_bytes <= 300){
        received_bytes += (read(readBuffer, 1) - 1);  
    }
    flow_off();

    while(j){
        
        dequeue(&rxBuffer, &trash_data);
        j--;
    }
    if(isEmpty(&rxBuffer)){
        
        while(k--){
            PORTB ^= (1 << PB5);
            delay(1000);
        }
    }
    else{
        
        PORTB |= (1 << PB5);
        delay(5000);
        PORTB &= ~(1 << PB5);
    }
    j = 20;
    
    PORTB &= ~(1 << PB5); 
    while(received_bytes <= 300){
        received_bytes += (read(readBuffer, 1) - 1);
    }
    flow_off();
    while(j){
        
        dequeue(&rxBuffer, &trash_data);
        j--;
    }
    if(isEmpty(&rxBuffer)){
        
        while(k--){
            PORTB ^= (1 << PB5);
            delay(1000);
        }
    }
    else{
        
        PORTB |= (1 << PB5);
        delay(5000);
        PORTB &= ~(1 << PB5);
    }
    
    PORTB &= ~(1 << PB5); 
    
    while(1){
        if(write(binary_string, strlen((const char *)binary_string), (int8_t)0) == 0){
            PORTB |= (1 << PB5); 
        }
        else if(write(binary_string, strlen((const char *)binary_string), (int8_t)0) == strlen((const char *)binary_string)){
            break; 
        }
        else{
            PORTB &= ~(1 << PB5); 
        }  
    }
    
    while (1) {
        
        read(readBuffer, 254);
        
        if(!strcmp((const char *)str2, (const char *)readBuffer)){
            PORTB |= (1 << PB5);  
        }
        else{
            PORTB &= ~(1 << PB5);  
        }
        
        while(read(readBuffer, 10) == 11); 
        
        if(!strcmp((const char *)str2, (const char *)readBuffer)){
            PORTB |= (1 << PB5);  
        }
        else{
            PORTB &= ~(1 << PB5);  
        }
    }

    return 0;
}



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
        return 0; 
    }
    cb->buffer[cb->head] = item;
    cb->head = (cb->head + 1) % BUFFER_SIZE; 
    cb->count++;
    return 1;
}

uint8_t dequeue(CircularBuffer *cb, uint8_t *item) {
    if (isEmpty(cb)) {
        return 0; 
    }
    *item = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % BUFFER_SIZE; 
    cb->count--;
    return 1;
}

ISR(USART_RX_vect) {
    uint8_t data = UDR0;
    
    if(data == ESCAPE)
    {
        esc = 1;
    }
    
    else if(data == XON && esc == 0){
        flow = 1;
    }
    
    else if(data == XOFF && esc == 0){
        flow = 0;
    }
    
    else if(data == SYNC && esc == 0){
        enqueue(&rxBuffer, data);
    }
    
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
    
    if (isEmpty(&txBuffer)) {
        UCSR0B &= ~(1 << UDRIE0);
    }
    
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
        else if(position > strlen((const char *)buf) || buf[position] == '\0'){
            position = 0;
            break;
        }
        else{
            data = buf[position];
            
            if(data == XON || data == XOFF || data == SYNC || data == ESCAPE){
                USART_Transmit(ESCAPE);
                USART_Transmit(data);
            }
            else USART_Transmit(data);
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
            buf[position] = '\0';   
            position = 0;  
            return counter;
        }
        else if(position > strlen((const char *)buf)){
            buf[position] = '\0';  
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

void flow_off(void){
    
    USART_Transmit(XOFF);
    receiver_flow = 0;
}

void flow_on(void){
    
    USART_Transmit(XON);
    receiver_flow = 1;
}

uint8_t is_flow_on(void){
    
    if(flow) 
        return (uint8_t)1;
    else
        return (uint8_t)0;
}

void USART_Transmit(uint8_t data){
    
    while (isFull(&txBuffer));
    
    enqueue(&txBuffer, data);
    
    UCSR0B |= (1 << UDRIE0);
}

uint8_t USART_Receive(void){
    uint8_t data;
    
    while (isEmpty(&rxBuffer));
    
    dequeue(&rxBuffer, &data);
    return data;
}

void usart_init(void){
    
    DDRD &= ~(1 << PD1); 
    DDRD |= (1 << PD0);  
    
    UCSR0A = 0U; 
	UBRR0H = ((uint8_t)(UBRR >> 8) & 0xF); 
	UBRR0L = ((uint8_t)(UBRR) & 0xFF);
    
	UCSR0B = (1 << RXEN0)|(1 << TXEN0)|(1 << RXCIE0)|(1 << TXCIE0|(1 << UDRIE0));
    UCSR0B &= ~(1 << UCSZ02); 
	
	UCSR0C = (1 << UCSZ01)|(1 << UCSZ00);
    
    initBuffer(&rxBuffer);
    initBuffer(&txBuffer);
}

void initTimer1(void){ 
    TCCR1B |= (1 << WGM12);               
    TIMSK1 |= (1 << OCIE1A);              
    TCCR1B |= (1 << CS11) | (1 << CS10);  
    OCR1A = 249;                                         
}


void delay(unsigned long ms){
    unsigned long start_time = milisecond_count;
    while(milisecond_count - start_time < ms);
}