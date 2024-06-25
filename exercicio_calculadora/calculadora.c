#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#include <stdint.h>

typedef enum {
    NONE = 0,
    SIGNAL, 
    NUMBER,
    OP1, 
    OP2, 
    OPERAND,
    PLUS,
    MINUS,
    MULT,
    DIV,
    COMMA, 
    EQUAL, 
    INVALID,
    IDLE
} states;

uint8_t input_processing();

void event_processing(uint8_t state);

float calc(float op1, float op2, char operand);

states state, current_state = NONE, prev_state = NONE;

char input[21], operand;
float result = 1500.0;
float op1, op2;

int main(){
    printf("/* Calculadora */\n");
    do{
        state = input_processing();
        /*event_processing(state);*/       
        system("cls");
        printf("/* Calculadora */\n");
        printf("Entrada: ");
        puts(input);
        printf("Resultado: ");
        printf("%f", &result);
        printf("\nCurrent state: %d", current_state);

        
    }while(current_state != INVALID);

    return 0;
}

uint8_t input_processing(){
    char ch = getch();
    strncat(input, &ch, 1);

    switch(ch){
        case 127:
        case 8:
            input[strlen(input)-2] = '\0';
            break;
        case 'C':
        case 'c':
            input[0] = '\0';
            result = 0.0;
            current_state = NONE;
            break;
        case '+':
            current_state = PLUS;
            break;
        case '-':
            current_state = MINUS;
            break;
        case '*':
            current_state = MULT;
            break;
        case '/':
            current_state = DIV;
            break;
        case '.':
        case ',':
            current_state = COMMA;
            break;
        case '=':
            current_state = EQUAL;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            current_state = NUMBER;
            break;
        default:
            current_state = INVALID;
    }
    return current_state;    
}

void event_processing(uint8_t state){
    switch(state){
        case NONE:
            current_state = NONE;
            prev_state = NONE;
            break;
        case PLUS:
            if(prev_state == NONE){
                prev_state = PLUS;
                current_state = SIGNAL;
            }
            else if(prev_state == NUMBER){
                prev_state = OP1;
                current_state = OPERAND;
                operand = '+';
            }
            else if(prev_state == SIGNAL){
                current_state = INVALID;
            }
            else if(current_state == OPERAND){
                prev_state = OPERAND;
                current_state = SIGNAL;
            }
            break;
        case MINUS:
            if(prev_state == NONE){
                prev_state = MINUS;
                current_state = SIGNAL;
            }
            else if(prev_state == NUMBER){
                prev_state = OP1;
                current_state = OPERAND;
                operand = '-';
            }
            else if(prev_state == SIGNAL){
                current_state = INVALID;
            }
            else if(current_state == OPERAND){
                prev_state = OPERAND;
                current_state = SIGNAL;
            }
            break;
    }
}