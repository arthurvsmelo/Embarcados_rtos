#include "hal.h"
#include "ch.h"

#define LED_ACTIVE                 0
#define LED_SOAK                   1
#define LED_WASH                   2
#define LED_RINSE                  3
#define LED_CENTRIFUGE             4
#define LED_WATER_IN               5
#define LED_WATER_OUT              6
#define LED_MOTOR_1                7
#define LED_MOTOR_2                8
#define BUZZER_PIN                 9
#define BTN_START                 12
#define BTN_HIGH_WATERMARK        13
#define BTN_LOW_WATERMARK         14
#define BTN_OPEN_LID              15
#define TIMEOUT_MOTOR_CYCLE      500
#define TIMEOUT_SOAK           10000
#define TIMEOUT_WASH           12000
#define TIMEOUT_RINSE          15000
#define TIMEOUT_CENTRIFUGE     20000

typedef enum {
    IDLE = 0,
    SOAK_WATER_IN = 1,
    SOAK_TURN_CLKWISE = 2,
    SOAK_TURN_ANTI_CLKWISE = 3,
    WASH_TURN_CLKWISE = 4,
    WASH_TURN_ANTI_CLKWISE = 5,
    WASH_WATER_OUT = 6,
    RINSE_WATER_IN = 7,
    RINSE_TURN_CLKWISE = 8,
    RINSE_TURN_ANTI_CLKWISE = 9,
    RINSE_WATER_OUT = 10,
    CENTRIFUGE = 11,
    OPEN_LID = 12
}States;



int main(void) {

    halInit();
    chSysInit(); 
    States current_state = IDLE, previous_state = IDLE;
    palSetGroupMode(IOPORT1, LED_ACTIVE|LED_SOAK|LED_WASH|LED_RINSE|LED_CENTRIFUGE
                    |LED_WATER_IN|LED_WATER_OUT|LED_MOTOR_1|LED_MOTOR_2|BUZZER_PIN, 
                    0, PAL_MODE_OUTPUT_PUSHPULL);
    
    palSetGroupMode(IOPORT2, BTN_START|BTN_HIGH_WATERMARK|BTN_LOW_WATERMARK|BTN_OPEN_LID, 0,
                    PAL_MODE_INPUT_PULLUP);
    
    palWriteGroup(IOPORT1, LED_ACTIVE|LED_SOAK|LED_WASH|LED_RINSE|LED_CENTRIFUGE
                    |LED_WATER_IN|LED_WATER_OUT|LED_MOTOR_1|LED_MOTOR_2|BUZZER_PIN,
                    0, 0x0000);   /* Desliga todos os leds e o buzzer */

    while(1) {
        palSetPad(GPIOC, GPIOC_LED);
        chThdSleepMilliseconds(1000);
        palClearPad(GPIOC, GPIOC_LED);
        chThdSleepMilliseconds(1000);
    }
    return 0;
}
