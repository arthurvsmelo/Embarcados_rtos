#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#include <stdint.h>

#define TRUE  1
#define FALSE 0

/* ESTADOS DA FSM */
typedef enum {
    START = 0,
    SIGNAL, 
    NUMBER,
    OP1, 
    OP2, 
    OPERATOR,
    PLUS,
    MINUS,
    MULT,
    DIV,
    COMMA, 
    EQUAL,
    CLEAR,
    INVALID_CHAR
} states;

/* VARIÁVEIS GLOBAIS */
uint8_t op1_done = FALSE,
        op2_done = FALSE,
        op1_isfloat = FALSE,
        op2_isfloat = FALSE;
char op_signal,
     op1_signal, 
     op2_signal, 
     operator, 
     input[21],
     op1_int_str[9],
     op1_dec_str[9],
     op2_int_str[9],
     op2_dec_str[9],
     *p = NULL;
float result, 
      ans, 
      operator1, 
      operator2;
states state, 
       current_state, 
       prev_state;

/* Define um evento a cda caractere lido */
uint8_t input_processing(char ch);

/* Processa o evento de cada caractere */
int event_processing(uint8_t state);

/* Calcula a operação quando o evento '=' for disparado */
float calc(float op1, float op2, char operator);





int main(){
    do{
        p = input;
        current_state = START;
        prev_state = START;
        op1_done = FALSE;
        op2_done = FALSE;
        system("cls");
        printf("Resultado: %f", result);
        printf("\nEntrada: ");
        fgets(input, 21, stdin);
        fflush(stdin);
        
        while(*p){
            state = input_processing(*p);
            if(event_processing(state)){
                printf("SYNTAX ERROR\n");
                break;
            };
            p++;
        }
        input[0] = '\0';        
    }while(TRUE);

    return 0;
}

uint8_t input_processing(char ch){
    states char_state;
    switch(ch){
        case 'C':
        case 'c':
            char_state = CLEAR;
            break;
        case '+':
            char_state = PLUS;
            break;
        case '-':
            char_state = MINUS;
            break;
        case '*':
            char_state = MULT;
            break;
        case '/':
            char_state = DIV;
            break;
        case '.':
        case ',':
            char_state = COMMA;
            break;
        case '=':
            char_state = EQUAL;
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
            char_state = NUMBER;
            break;
        default:
            char_state = INVALID_CHAR;
    }
    return char_state;    
}


int event_processing(uint8_t state){
    /* START SIGNAL OP1 OP2 OPERATOR EQUAL */
    switch(state){
        case PLUS:
            if(current_state == START && prev_state == START){
                prev_state = START;
                current_state = SIGNAL;
                op1_signal = '+';
            }
            else if(current_state == SIGNAL && (prev_state == START || prev_state == SIGNAL || prev_state == OPERATOR)){
                prev_state = SIGNAL;
                current_state = SIGNAL;
                if(op1_done){
                    if(op2_signal == '+')      op2_signal = '+';
                    else if(op2_signal == '-') op2_signal = '-';
                }
                else{
                    if(op1_signal == '+')      op1_signal = '+';
                    else if(op1_signal == '-') op1_signal = '-';
                }
            }
            else if(current_state == OP1 && prev_state == SIGNAL){
                prev_state = OP1;
                current_state = OPERATOR;
                operator = '+';
                op1_done = TRUE;

            }
            else if(current_state == OPERATOR && prev_state == OP1){
                prev_state = OPERATOR;
                current_state = SIGNAL;
                op2_signal = '+';
            }
            else if(current_state == OP2 && (prev_state == SIGNAL || prev_state == OPERATOR || prev_state == OP2)){
                return 1;
            }
            break;
        case MINUS:
            if(current_state == START && prev_state == START){
                prev_state = START;
                current_state = SIGNAL;
                op1_signal = '-';
            }
            else if(current_state == SIGNAL && (prev_state == START || prev_state == SIGNAL || prev_state == OPERATOR)){
                prev_state = SIGNAL;
                current_state = SIGNAL;
                if(op1_done){
                    if(op2_signal == '+')      op2_signal = '-';
                    else if(op2_signal == '-') op2_signal = '+';
                }
                else{
                    if(op1_signal == '+')      op1_signal = '-';
                    else if(op1_signal == '-') op1_signal = '+';
                }
            }
            else if(current_state == OP1 && prev_state == SIGNAL){
                prev_state = OP1;
                current_state = OPERATOR;
                operator = '-';
                op1_done = TRUE;

            }
            else if(current_state == OPERATOR && prev_state == OP1){
                prev_state = OPERATOR;
                current_state = SIGNAL;
                op2_signal = '-';
            }
            else if(current_state == OP2 && (prev_state == SIGNAL || prev_state == OPERATOR || prev_state == OP2)){
                return 1;
            }
            break;
        case MULT:
            if(current_state == START && prev_state == START){
                prev_state = START;
                current_state = OPERATOR;
                operator = '*';
            }
            else if(current_state == OPERATOR && (prev_state == OP1 || prev_state == START)){
                return 1;
            }
            else if(current_state == OP1 && (prev_state == SIGNAL || prev_state == OP1)){
                prev_state = OP1;
                current_state = OPERATOR;
                operator = '*';
                op1_done = TRUE;
            }
            else if(current_state == SIGNAL && prev_state == OPERATOR){
                return 1;
            }
            else if(current_state == OP2 && (prev_state == SIGNAL || prev_state == OP2)){
                return 1;
            }
            break;
        case DIV:
            if(current_state == START && prev_state == START){
                prev_state = START;
                current_state = OPERATOR;
                operator = '/';
            }
            else if(current_state == OPERATOR && (prev_state == OP1 || prev_state == START)){
                return 1;
            }
            else if(current_state == OP1 && (prev_state == SIGNAL || prev_state == OP1)){
                prev_state = OP1;
                current_state = OPERATOR;
                operator = '/';
                op1_done = TRUE;
            }
            else if(current_state == SIGNAL && prev_state == OPERATOR){
                return 1;
            }
            else if(current_state == OP2 && (prev_state == SIGNAL || prev_state == OP2)){
                return 1;
            }
            break;
        case COMMA:
            if(current_state == START && prev_state == START){
                prev_state = START;
                current_state = OP1;
                op1_signal = '+';
                op1_isfloat = TRUE;
                strncat(op1_int_str, '0', 1);
            }
            else if(current_state == SIGNAL && prev_state == START){
                prev_state = SIGNAL;
                current_state = OP1;
                op1_isfloat = TRUE;
                strncat(op1_int_str, '0', 1);
            }
            else if(current_state == OP1 && (prev_state == START || prev_state == SIGNAL || prev_state == OP1)){
                prev_state = OP1;
                current_state = OP1;
                op1_isfloat = TRUE;
            }
            else if(current_state == SIGNAL && (prev_state == OPERATOR || prev_state == SIGNAL)){
                prev_state = SIGNAL;
                if(op1_done){
                    current_state = OP2;
                    op2_isfloat = TRUE;
                    strncat(op2_int_str, '0', 1);
                }
                else{
                    current_state = OP1;
                    op1_isfloat = TRUE;
                    strncat(op1_int_str, '0', 1);
                }
            }
            else if(current_state == OP2 && (prev_state == SIGNAL || prev_state == OPERATOR || prev_state == OP2)){
                prev_state = OP2;
                current_state = OP2;
                op2_isfloat = TRUE;
            }
            break;
        case NUMBER:
            if(current_state == START && prev_state == START){
                current_state = OP1;
                prev_state = START;
                op1_signal = '+';
                strncat(op1_str, *p, 1);
            }
            else if(current_state == OP1 && prev_state == START){
                current_state = OP1;
                prev_state = OP1;
                strncat(op1_str, *p, 1);
            }
            else if(current_state == OP1 && prev_state == OP1){
                current_state = OP1;
                prev_state = OP1;
                strncat(op1_str, *p, 1);
            }
            else if(current_state == OPERATOR && prev_state == START){
                current_state = OP1;
                prev_state = OPERATOR;
                strncat(op1_str, *p, 1);
            }
            else if(current_state == OPERATOR && prev_state == OP1){
                current_state = OP2;
                prev_state = OPERATOR;
                strncat(op2_str, *p, 1);
            }
            else if(current_state == OP2 && prev_state == OPERATOR){
                current_state = OP2;
                prev_state = OP2;
                strncat(op2_str, *p, 1);
            }
            else if(current_state == OP2 && prev_state == SIGNAL){
                current_state = OP2;
                prev_state = OP2;
                strncat(op2_str, *p, 1);
            }
            else if(current_state == OP2 && prev_state == OP2){
                current_state = OP2;
                prev_state = OP2;
                strncat(op2_str, *p, 1);
            }
            else if(current_state == SIGNAL && prev_state == START){
                current_state = OP1;
                prev_state = SIGNAL;
                strncat(op1_str, *p, 1);
            }
            else if(current_state == SIGNAL && prev_state == OPERATOR){
                current_state = OP2;
                prev_state = SIGNAL;
                strncat(op2_str, *p, 1);
            }
            break;
        case EQUAL:
            if(current_state == START){
                result = ans;
            }
            else if(current_state == SIGNAL){
                current_state = START;
                prev_state = START;
                input[0] = '\0';
                return 1;
            }
            else if(current_state == OP1){
                op1_done = TRUE;
                if(op1_isfloat)  op1 = atoi(op1_str) / strlen(op1_str);
                else             op1 = atoi(op1_str);

                if(op1_signal == '-') op1 *= -1;
                else                  op1 *= 1;

                if(operator == '+'){
                    result = ans + op1;
                    ans = result;
                }
                else if(operator == '-'){
                    result = ans - op1;
                    ans = result;
                }
                else if(operator == '*'){
                    result = ans * op1;
                    ans = result;
                }
                else if(operator == '/'){
                    result = ans / op1;
                    ans = result;
                }
            }
            else if(current_state == OP2){
                op2_done = TRUE;
                if(op2_isfloat)  op2 = atoi(op2_str) / strlen(op2_str);
                else             op2 = atoi(op2_str);

                if(op2_signal == '-') op2 *= -1;
                else                  op2 *= 1;


            }

            break;
        case CLEAR:
            ans = 0.0;
            result = 0.0;
            input[0] = '\0';
            break;
        case INVALID_CHAR:
            /* IGNORA */
            break;
    }
    if(op1_done){
        
        if(op1_isfloat)  op1 = atoi(op1_str) / strlen(op1_str);
        else             op1 = atoi(op1_str);

        if(op1_signal == '-') op1 *= -1;
        else                  op1 *= 1;
    }
    if(op2_done){
        
        if(op2_isfloat)  op2 = atoi(op2_str) / strlen(op2_str);
        else             op2 = atoi(op2_str);

        if(op2_signal == '-') op2 *= -1;
        else                  op2 *= 1;
    }
    return 0; /* SEM ERROS */    
}

float calc(float op1, float op2, char operator){
    switch(operator){
        case '+':
            return op1 + op2;
        case '-':
            return op1 - op2;
        case '*':
            return op1 * op2;
        case '/':
            return op1 / op2;
    }
}