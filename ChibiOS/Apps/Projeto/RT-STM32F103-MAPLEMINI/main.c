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
#define BTN_START                 12
#define BTN_HIGH_WATERMARK        13
#define BTN_LOW_WATERMARK         14
#define BTN_OPEN_LID              15
#define TIMEOUT_MOTOR_CYCLE      500
#define TIMEOUT_SOAK           10000
#define TIMEOUT_WASH           12000
#define TIMEOUT_RINSE          15000
#define TIMEOUT_CENTRIFUGE     20000
#define GPIO_INPUT_MASK       0xF000
#define GPIO_OUTPUT_MASK      0x01FF
#define START_EVENT                      EVENT_MASK(0)
#define HIGH_WATERMARK_EVENT             EVENT_MASK(1)
#define LOW_WATERMARK_EVENT              EVENT_MASK(2)
#define LID_OPEN_EVENT                   EVENT_MASK(3)
#define LID_CLOSE_EVENT                  EVENT_MASK(4)
#define TIMEOUT_MOTOR_EVENT              EVENT_MASK(5)
#define TIMEOUT_SOAK_EVENT               EVENT_MASK(6)
#define TIMEOUT_WASH_EVENT               EVENT_MASK(7)
#define TIMEOUT_RINSE_EVENT              EVENT_MASK(8)
#define TIMEOUT_CENTRIFUGE_EVENT         EVENT_MASK(9)
#define FSM_WAKEUP_EVENT                 EVENT_MASK(10)


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

typedef enum {
    INIT_EV = 0,
    LID_OPEN_EV = 1,
    LID_CLOSE_EV = 2,
    HIGH_WATERMARK_EV = 3,
    LOW_WATERMARK_EV = 4,
    TIMEOUT_MOTOR_EV = 5,
    TIMEOUT_SOAK_EV = 6,
    TIMEOUT_WASH_EV = 7,
    TIMEOUT_RINSE_EV = 8,
    TIMEOUT_CENTRIFUGE_EV = 9
}Events;

typedef enum {
    MOTOR_TIMER = 1,
    SOAK_TIMER = 2,
    WASH_TIMER = 3,
    RINSE_TIMER = 4,
    CENTRIFUGE_TIMER = 5
}Timers;

States current_state = IDLE, previous_state = IDLE;
event_source_t evt_src;
Events evt;
Timers timer;
virtual_timer_t vt_motor, vt_soak, vt_wash, vt_rinse, vt_centrifuge;
sysinterval_t remainingTime_motor, remainingTime_soak, remainingTime_wash, remainingTime_rinse, remainingTime_centrifuge;
volatile uint8_t vt_motor_paused = 0, vt_soak_paused = 0, vt_wash_paused = 0, vt_rinse_paused = 0, vt_centrifuge_paused = 0, is_lid_open = 0;

static void motorTimerCallback(void) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&evt_src, TIMEOUT_MOTOR_EVENT);
    chSysUnlockFromISR();
}

static void soakTimerCallback(void) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&evt_src, TIMEOUT_SOAK_EVENT);
    chSysUnlockFromISR();
}

static void washTimerCallback(void) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&evt_src, TIMEOUT_WASH_EVENT);
    chSysUnlockFromISR();
}

static void rinseTimerCallback(void) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&evt_src, TIMEOUT_RINSE_EVENT);
    chSysUnlockFromISR();
}

static void centrifugeTimerCallback(void) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&evt_src, TIMEOUT_CENTRIFUGE_EVENT);
    chSysUnlockFromISR();
}

static void startButtonCallback(void) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&evt_src, START_EVENT);
    chSysUnlockFromISR();
}

static void highWatermarkButtonCallback(void) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&evt_src, HIGH_WATERMARK_EVENT);
    chSysUnlockFromISR();
}

static void lowWatermarkButtonCallback(void) {
    chSysLockFromISR();
    chEvtBroadcastFlagsI(&evt_src, LOW_WATERMARK_EVENT);
    chSysUnlockFromISR();
}

static void openLidButtonCallback(void) {
    chSysLockFromISR();
    /* se o evento anterior for porta aberta, o novo evento sera porta fechada */
    if(is_lid_open) {
        chEvtBroadcastFlagsI(&evt_src, LID_CLOSE_EVENT);
        is_lid_open = 0;
    }
    else {
        chEvtBroadcastFlagsI(&evt_src, LID_OPEN_EVENT);
        is_lid_open = 1;
    }
    chSysUnlockFromISR();
}

static void startTimer(Timers vt, sysinterval_t time) {
    if(vt == MOTOR_TIMER) {
        chVTSet(&vt_motor, time, motorTimerCallback, NULL);
    }
    else if(vt == SOAK_TIMER) {
        chVTSet(&vt_soak, time, soakTimerCallback, NULL);
    }
    else if(vt == WASH_TIMER) {
        chVTSet(&vt_wash, time, washTimerCallback, NULL);
    }
    else if(vt == RINSE_TIMER) {
        chVTSet(&vt_rinse, time, rinseTimerCallback, NULL);
    }
    else if(vt == CENTRIFUGE_TIMER) {
        chVTSet(&vt_centrifuge, time, centrifugeTimerCallback, NULL);
    }
}

static void pauseTimer(Timers vt) {
    if(vt == MOTOR_TIMER) {
        if (!vt_motor_paused) {
            remainingTime_motor = chVTGetRemainingIntervalI(&vt_motor);
            chVTReset(&vt_motor);
            vt_motor_paused = 1;
        }
    }
    else if(vt == SOAK_TIMER) {
        if (!vt_soak_paused) {
            remainingTime_soak = chVTGetRemainingIntervalI(&vt_soak);
            chVTReset(&vt_soak);
            vt_soak_paused = 1;
        }
    }
    else if(vt == WASH_TIMER) {
        if (!vt_wash_paused) {
            remainingTime_wash = chVTGetRemainingIntervalI(&vt_wash);
            chVTReset(&vt_wash);
            vt_wash_paused = 1;
        }
    }
    else if(vt == RINSE_TIMER) {
        if (!vt_rinse_paused) {
            remainingTime_rinse = chVTGetRemainingIntervalI(&vt_rinse);
            chVTReset(&vt_rinse);
            vt_rinse_paused = 1;
        }
    }
    else if(vt == CENTRIFUGE_TIMER) {
        if (!vt_centrifuge_paused) {
            remainingTime_centrifuge = chVTGetRemainingIntervalI(&vt_centrifuge);
            chVTReset(&vt_centrifuge);
            vt_centrifuge_paused = 1;
        }
    }
}

static void resumeTimer(Timers vt) {
    if(vt == MOTOR_TIMER) {
        if(vt_motor_paused) {
            chVTSet(&vt_motor, remainingTime_motor, motorTimerCallback, NULL);
            vt_motor_paused = 0;
        }
    }
    else if(vt == SOAK_TIMER) {
        if(vt_soak_paused){
            chVTSet(&vt_soak, remainingTime_soak, soakTimerCallback, NULL);
            vt_soak_paused = 0;
        }
    }
    else if(vt == WASH_TIMER) {
        if(vt_wash_paused) {
            chVTSet(&vt_wash, remainingTime_wash, washTimerCallback, NULL);
            vt_wash_paused = 0;
        }
    }
    else if(vt == RINSE_TIMER) {
        if(vt_rinse_paused) {
            chVTSet(&vt_rinse, remainingTime_rinse, rinseTimerCallback, NULL);
            vt_rinse_paused = 0;
        }
    }
    else if(vt == CENTRIFUGE_TIMER) {
        if(vt_centrifuge_paused) {
            chVTSet(&vt_centrifuge, remainingTime_centrifuge, centrifugeTimerCallback, NULL);
            vt_centrifuge_paused = 0;
        }
    }
}

/* Verifica os eventos ocorridos e os trata */
static THD_FUNCTION(eventHandle, arg) {

    (void)arg;
    event_listener_t evt_listener;
    chEvtRegister(&evt_src, &evt_listener, 0);

    while(1) {
        eventmask_t events = chEvtWaitAny(ALL_EVENTS);
        if(events & START_EVENT) {
            if(current_state == IDLE) {
                previous_state = IDLE;
                current_state = SOAK_WATER_IN;
            }
            else {
                current_state = current_state;
            }
        }
        else if(events & HIGH_WATERMARK_EVENT) {
            if(current_state == SOAK_WATER_IN) {
                previous_state = SOAK_WATER_IN;
                current_state = SOAK_TURN_CLKWISE;
                startTimer(SOAK_TIMER, TIME_MS2I(TIMEOUT_SOAK));
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else if(current_state == RINSE_WATER_IN) {
                previous_state = RINSE_WATER_IN;
                current_state = RINSE_TURN_CLKWISE;
                startTimer(RINSE_TIMER, TIME_MS2I(4 * TIMEOUT_RINSE));
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else {
                current_state = current_state;
            }
        }
        else if(events & LOW_WATERMARK_EVENT) {
            if(current_state == WASH_WATER_OUT){
                previous_state = WASH_WATER_OUT;
                current_state = RINSE_WATER_IN;
            }
            else if(current_state == RINSE_WATER_OUT){
                previous_state = RINSE_WATER_OUT;
                current_state = CENTRIFUGE;
                startTimer(CENTRIFUGE_TIMER, TIME_MS2I(TIMEOUT_CENTRIFUGE));
            }
            else {
                current_state = current_state;
            }
        }
        else if(events & TIMEOUT_MOTOR_EVENT) {
            if(current_state == SOAK_TURN_CLKWISE) {
                previous_state = SOAK_TURN_CLKWISE;
                current_state = SOAK_TURN_ANTI_CLKWISE;
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else if(current_state == SOAK_TURN_ANTI_CLKWISE) {
                previous_state = SOAK_TURN_ANTI_CLKWISE;
                current_state = SOAK_TURN_CLKWISE;
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else if(current_state == WASH_TURN_CLKWISE) {
                previous_state = WASH_TURN_CLKWISE;
                current_state = WASH_TURN_ANTI_CLKWISE;
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else if(current_state == WASH_TURN_ANTI_CLKWISE) {
                previous_state = WASH_TURN_ANTI_CLKWISE;
                current_state = WASH_TURN_CLKWISE;
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else if(current_state == RINSE_TURN_CLKWISE) {
                previous_state = RINSE_TURN_CLKWISE;
                current_state = RINSE_TURN_ANTI_CLKWISE;
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else if(current_state == RINSE_TURN_ANTI_CLKWISE) {
                previous_state = RINSE_TURN_ANTI_CLKWISE;
                current_state = RINSE_TURN_CLKWISE;
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else {
                current_state = current_state;
            }
        }
        else if(events & TIMEOUT_SOAK_EVENT) {
            if(current_state == SOAK_TURN_CLKWISE) {
                previous_state = SOAK_TURN_CLKWISE;
                current_state = WASH_TURN_CLKWISE;
                startTimer(WASH_TIMER, TIME_MS2I(4 * TIMEOUT_WASH));
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else if(current_state == SOAK_TURN_ANTI_CLKWISE) {
                previous_state = SOAK_TURN_ANTI_CLKWISE;
                current_state = WASH_TURN_CLKWISE;
                startTimer(WASH_TIMER, TIME_MS2I(4 * TIMEOUT_WASH));
                startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
            }
            else {
                current_state = current_state;
            }
        }
        else if(events & TIMEOUT_WASH_EVENT) {
            if(current_state == WASH_TURN_CLKWISE) {
                previous_state = WASH_TURN_CLKWISE;
                current_state = WASH_WATER_OUT;
            }
            else if(current_state == WASH_TURN_ANTI_CLKWISE) {
                previous_state = WASH_TURN_ANTI_CLKWISE;
                current_state = WASH_WATER_OUT;
            }
            else {
                current_state = current_state;
            }
        }
        else if(events & TIMEOUT_RINSE_EVENT) {
            if(current_state == RINSE_TURN_CLKWISE) {
                previous_state = RINSE_TURN_CLKWISE;
                current_state = RINSE_WATER_OUT;
            }
            else if(current_state == RINSE_TURN_ANTI_CLKWISE) {
                previous_state = RINSE_TURN_ANTI_CLKWISE;
                current_state = RINSE_WATER_OUT;
            }
            else {
                current_state = current_state;
            }
        }
        else if(events & TIMEOUT_CENTRIFUGE_EVENT) {
            if(current_state == CENTRIFUGE) {
                previous_state = CENTRIFUGE;
                current_state = IDLE;
            }
            else {
                current_state = current_state;
            }
        }
        else if(events & LID_OPEN_EVENT) {
            previous_state = current_state;
            current_state = OPEN_LID;
            if(previous_state == SOAK_TURN_CLKWISE || previous_state == SOAK_TURN_ANTI_CLKWISE) {
                pauseTimer(SOAK_TIMER);
                pauseTimer(MOTOR_TIMER);
            }
            else if(previous_state == WASH_TURN_CLKWISE || previous_state == WASH_TURN_ANTI_CLKWISE) {
                pauseTimer(WASH_TIMER);
                pauseTimer(MOTOR_TIMER);
            }
            else if(previous_state == RINSE_TURN_CLKWISE || previous_state == RINSE_TURN_ANTI_CLKWISE) {
                pauseTimer(RINSE_TIMER);
                pauseTimer(MOTOR_TIMER);
            }
            else if(current_state == CENTRIFUGE) {
                pauseTimer(CENTRIFUGE_TIMER);
            }
        }
        else if(events & LID_CLOSE_EVENT) {
            current_state = previous_state;
            if(previous_state == SOAK_TURN_CLKWISE || previous_state == SOAK_TURN_ANTI_CLKWISE) {
                resumeTimer(SOAK_TIMER);
                resumeTimer(MOTOR_TIMER);
            }
            else if(previous_state == WASH_TURN_CLKWISE || previous_state == WASH_TURN_ANTI_CLKWISE) {
                resumeTimer(WASH_TIMER);
                resumeTimer(MOTOR_TIMER);
            }
            else if(previous_state == RINSE_TURN_CLKWISE || previous_state == RINSE_TURN_ANTI_CLKWISE) {
                resumeTimer(RINSE_TIMER);
                resumeTimer(MOTOR_TIMER);
            }
            else if(current_state == CENTRIFUGE) {
                resumeTimer(CENTRIFUGE_TIMER);
            }
        }
        /* acorda a maquina de estados para verificar o novo estado atualizado */
        chEvtBroadcastFlags(&evt_src, FSM_WAKEUP_EVENT);
    }
}
static THD_WORKING_AREA(wa_eventHandle, 2048);

/* Recebe como parametro os estados da FSM e os processa */
static THD_FUNCTION(stateMachine, arg) {

    (void)arg;
    event_listener_t fsm_evt_listener;
    chEvtRegisterMask(&evt_src, &fsm_evt_listener, FSM_WAKEUP_EVENT);

    while(1) {
        /* aguarda a thd eventHandle acordar a FSM */
        chEvtWaitAny(FSM_WAKEUP_EVENT);
        switch(current_state) {
            case IDLE:
                /* desliga todos os leds, menos o led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                break;

            case SOAK_WATER_IN:
                /* acende led de entrada de agua, estado molho e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0023);
                /* espera evento do botao de nivel alto */
                break;

            case SOAK_TURN_CLKWISE:
                /* acende led de motor 2, estado molho e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0083);
                break;

            case SOAK_TURN_ANTI_CLKWISE:
                /* acende led de motor 2, estado molho e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0103);
                break;

            case WASH_TURN_CLKWISE:
                /* acende led de motor 1, estado lavagem e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0085);
                break;

            case WASH_TURN_ANTI_CLKWISE:
                /* acende led de motor 2, estado lavagem e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0105);
                break;

            case WASH_WATER_OUT:
                /* acende led de saida de agua, estado lavagem e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0045);
                /* espera evento do botao de nivel baixo */
                break;

            case RINSE_WATER_IN:
                /* acende led de entrada de agua, estado enxague e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0029);
                /* espera evento do botao de nivel alto */
                break;

            case RINSE_TURN_CLKWISE:
                /* acende led de motor 1, estado enxague e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0089);
                break;

            case RINSE_TURN_ANTI_CLKWISE:
                /* acende led de motor 2, estado enxague e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0109);
                break;

            case RINSE_WATER_OUT:
                /* acende led de saída de agua, do estado enxague e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0049);
                /* espera evento do botao de nivel baixo */
                break;

            case CENTRIFUGE:
                /* acende led do motor 1, do estado centrifuge e led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0091);
                break;

            case OPEN_LID:
                /* desliga todas as saídas, menos o led de funcionamento */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                break;

            default:
                /* ERRO: acende todos os leds */
                palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x01FF);
                break;
        }
    }
}
static THD_WORKING_AREA(wa_stateMachine, 2048);

int main(void) {

    halInit();
    chSysInit(); 
    
    /* pinos de saida */
    palSetGroupMode(IOPORT1, GPIO_OUTPUT_MASK, 0, PAL_MODE_OUTPUT_PUSHPULL);
    /* pinos de entrada */
    palSetGroupMode(IOPORT2, GPIO_INPUT_MASK, 0, PAL_MODE_INPUT_PULLUP);
    /* desliga todos os leds */
    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0000);
    /* habilita interrupçoes nos botoes de entrada */
    palEnablePadEvent(IOPORT2, BTN_START, PAL_EVENT_MODE_FALLING_EDGE);
    palEnablePadEvent(IOPORT2, BTN_HIGH_WATERMARK, PAL_EVENT_MODE_FALLING_EDGE);
    palEnablePadEvent(IOPORT2, BTN_LOW_WATERMARK, PAL_EVENT_MODE_FALLING_EDGE);
    palEnablePadEvent(IOPORT2, BTN_OPEN_LID, PAL_EVENT_MODE_FALLING_EDGE);
    /* define os callbacks dos botoes de entrada */
    palSetPadCallback(IOPORT2, BTN_START, startButtonCallback, NULL);
    palSetPadCallback(IOPORT2, BTN_HIGH_WATERMARK, highWatermarkButtonCallback, NULL);
    palSetPadCallback(IOPORT2, BTN_LOW_WATERMARK, lowWatermarkButtonCallback, NULL);
    palSetPadCallback(IOPORT2, BTN_OPEN_LID, openLidButtonCallback, NULL);
    /* inicializa os virtual timers */
    /*
    chVTObjectInit(&vt_motor);
    chVTObjectInit(&vt_soak);
    chVTObjectInit(&vt_wash);
    chVTObjectInit(&vt_rinse);
    chVTObjectInit(&vt_centrifuge);
    */
    /* inicializa os event sources */
    chEvtObjectInit(&evt_src);

    thread_t* ptr_eventHandle = chThdCreateStatic(wa_eventHandle, sizeof(wa_eventHandle), NORMALPRIO + 1, eventHandle, NULL);
    thread_t* ptr_stateMachine = chThdCreateStatic(wa_stateMachine, sizeof(wa_stateMachine), NORMALPRIO + 1, stateMachine, NULL);
    
    while(1) {
        chThdSleepMilliseconds(1000);
    }
    return 0;
}
