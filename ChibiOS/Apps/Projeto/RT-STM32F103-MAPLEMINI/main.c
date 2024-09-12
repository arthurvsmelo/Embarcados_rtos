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
#define QUEUE_SIZE                10

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
    START_EV = 0,
    LID_OPEN_EV = 1,
    LID_CLOSE_EV = 2,
    HIGH_WATERMARK_EV = 3,
    LOW_WATERMARK_EV = 4,
    TIMEOUT_MOTOR_EV = 5,
    TIMEOUT_SOAK_EV = 6,
    TIMEOUT_WASH_EV = 7,
    TIMEOUT_RINSE_EV = 8,
    TIMEOUT_CENTRIFUGE_EV = 9
}event_t;

typedef enum {
    MOTOR_TIMER = 1,
    SOAK_TIMER = 2,
    WASH_TIMER = 3,
    RINSE_TIMER = 4,
    CENTRIFUGE_TIMER = 5
}timer_t;

static event_t queue[QUEUE_SIZE], *rd_ptr, *wr_ptr;
static size_t qsize;
States current_state = IDLE, 
       previous_state = IDLE;

timer_t timer;

virtual_timer_t vt_motor, 
                vt_soak, 
                vt_wash, 
                vt_rinse, 
                vt_centrifuge;

sysinterval_t remainingTime_motor, 
              remainingTime_soak, 
              remainingTime_wash, 
              remainingTime_rinse, 
              remainingTime_centrifuge;

bool vt_motor_paused = false,
     vt_soak_paused = false,
     vt_wash_paused = false,
     vt_rinse_paused = false, 
     vt_centrifuge_paused = false,
     is_lid_open = false;

thread_t *ptr_stateMachine;

MUTEX_DECL(mtx);
CONDVAR_DECL(qempty);
CONDVAR_DECL(qfull);

static void queueInit(void) {
    chMtxObjectInit(&mtx);
    chCondObjectInit(&qempty);
    chCondObjectInit(&qfull);
 
    rd_ptr = wr_ptr = &queue[0];
    qsize = 0;
}

void queueWriteFromISR(event_t evt) {
    chSysLockFromISR();
 
    if(qsize < QUEUE_SIZE) {
         *wr_ptr = evt;
        if (++wr_ptr >= &queue[QUEUE_SIZE])
            wr_ptr = &queue[0];
        qsize++;
    }
    chCondSignalI(&qempty);

    chSysUnlockFromISR();
}

event_t queueRead(void) {
    event_t evt;

    chSysLock();
    chMtxLockS(&mtx);
 
    while (qsize == 0)
        chCondWait(&qempty);
 
    evt = *rd_ptr;

    if (++rd_ptr >= &queue[QUEUE_SIZE])
        rd_ptr = &queue[0];
    qsize--;
 
    chMtxUnlockS(&mtx);
    chSysUnlock();
 
    return evt;
}

static void motorTimerCallback(void *arg) {
    (void)arg;
    event_t evt = TIMEOUT_MOTOR_EV;
    queueWriteFromISR(evt);
}

static void soakTimerCallback(void *arg) {
    (void)arg;
    event_t evt = TIMEOUT_SOAK_EV;
    queueWriteFromISR(evt);
}

static void washTimerCallback(void *arg) {
    (void)arg;
    event_t evt = TIMEOUT_WASH_EV;
    queueWriteFromISR(evt);
}

static void rinseTimerCallback(void *arg) {
    (void)arg;
    event_t evt = TIMEOUT_RINSE_EV;
    queueWriteFromISR(evt);
}

static void centrifugeTimerCallback(void *arg) {
    (void)arg;
    event_t evt = TIMEOUT_CENTRIFUGE_EV;
    queueWriteFromISR(evt);
}

static void startButtonCallback(void *arg) {
    (void)arg;
    event_t evt = START_EV;
    queueWriteFromISR(evt);
}

static void highWatermarkButtonCallback(void *arg) {
    (void)arg;
    event_t evt = HIGH_WATERMARK_EV;
    queueWriteFromISR(evt);
}

static void lowWatermarkButtonCallback(void *arg) {
    (void)arg;
    event_t evt = LOW_WATERMARK_EV;
    queueWriteFromISR(evt);
}

static void openLidButtonCallback(void *arg) {
    (void)arg;
    /* se o evento anterior for porta aberta, o novo evento sera porta fechada */
    if(is_lid_open == true) {
        is_lid_open = false;
        queueWriteFromISR(LID_CLOSE_EV);
    }
    else {
        is_lid_open = true;
        queueWriteFromISR(LID_OPEN_EV);
    }
}

static void startTimer(timer_t vt, sysinterval_t time) {
    switch(vt){
        case MOTOR_TIMER:
            chVTSet(&vt_motor, time, motorTimerCallback, NULL);
            break;
        case SOAK_TIMER:
            chVTSet(&vt_soak, time, soakTimerCallback, NULL);
            break;
        case WASH_TIMER:
            chVTSet(&vt_wash, time, washTimerCallback, NULL);
            break;
        case RINSE_TIMER:
            chVTSet(&vt_rinse, time, rinseTimerCallback, NULL);
            break;
        case CENTRIFUGE_TIMER:
            chVTSet(&vt_centrifuge, time, centrifugeTimerCallback, NULL);
            break;
        default:
    }
}

static void pauseTimer(timer_t vt) {
    switch(vt) {
        case MOTOR_TIMER:
            if (vt_motor_paused == false) {
                remainingTime_motor = chVTGetRemainingIntervalI(&vt_motor);
                chVTReset(&vt_motor);
                vt_motor_paused = true;
            }
            break;
        case SOAK_TIMER:
            if (vt_soak_paused == false) {
                remainingTime_soak = chVTGetRemainingIntervalI(&vt_soak);
                chVTReset(&vt_soak);
                vt_soak_paused = true;
            }
            break;
        case WASH_TIMER:
            if (vt_wash_paused == false) {
                remainingTime_wash = chVTGetRemainingIntervalI(&vt_wash);
                chVTReset(&vt_wash);
                vt_wash_paused = true;
            }
            break;
        case RINSE_TIMER:
            if (vt_rinse_paused == false) {
                remainingTime_rinse = chVTGetRemainingIntervalI(&vt_rinse);
                chVTReset(&vt_rinse);
                vt_rinse_paused = true;
            }
            break;
        case CENTRIFUGE_TIMER:
            if (vt_centrifuge_paused == false) {
                remainingTime_centrifuge = chVTGetRemainingIntervalI(&vt_centrifuge);
                chVTReset(&vt_centrifuge);
                vt_centrifuge_paused = true;
            }
            break;
        default:
    }
}

static void resumeTimer(timer_t vt) {
    switch(vt) {
        case MOTOR_TIMER:
            if(vt_motor_paused == true) {
                chVTSet(&vt_motor, remainingTime_motor, motorTimerCallback, NULL);
                vt_motor_paused = false;
            }
            break;
        case SOAK_TIMER:
            if(vt_soak_paused == true){
                chVTSet(&vt_soak, remainingTime_soak, soakTimerCallback, NULL);
                vt_soak_paused = false;
            }
            break;
        case WASH_TIMER:
            if(vt_wash_paused == true) {
                chVTSet(&vt_wash, remainingTime_wash, washTimerCallback, NULL);
                vt_wash_paused = false;
            }
            break;
        case RINSE_TIMER:
            if(vt_rinse_paused == true) {
                chVTSet(&vt_rinse, remainingTime_rinse, rinseTimerCallback, NULL);
                vt_rinse_paused = false;
            }
            break;
        case CENTRIFUGE_TIMER:
            if(vt_centrifuge_paused == true) {
                chVTSet(&vt_centrifuge, remainingTime_centrifuge, centrifugeTimerCallback, NULL);
                vt_centrifuge_paused = false;
            }
            break;
        default:
    }
}

static THD_FUNCTION(stateMachine, arg) {
    (void)arg;
    event_t event;

    while(1) {
        event = queueRead();
        switch(event){
            case START_EV:
                if(current_state == IDLE) {
                    previous_state = IDLE;
                    current_state = SOAK_WATER_IN;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0023);
                }
                break;
            case HIGH_WATERMARK_EV:
                if(current_state == SOAK_WATER_IN) {
                    previous_state = SOAK_WATER_IN;
                    current_state = SOAK_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0083);
                    startTimer(SOAK_TIMER, TIME_MS2I(TIMEOUT_SOAK));
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                else if(current_state == RINSE_WATER_IN) {
                    previous_state = RINSE_WATER_IN;
                    current_state = RINSE_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0089);
                    startTimer(RINSE_TIMER, TIME_MS2I(4 * TIMEOUT_RINSE));
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                break;
            case LOW_WATERMARK_EV:
                if(current_state == WASH_WATER_OUT){
                    previous_state = WASH_WATER_OUT;
                    current_state = RINSE_WATER_IN;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0029);
                }
                else if(current_state == RINSE_WATER_OUT){
                    previous_state = RINSE_WATER_OUT;
                    current_state = CENTRIFUGE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0091);
                    startTimer(CENTRIFUGE_TIMER, TIME_MS2I(TIMEOUT_CENTRIFUGE));
                }
                break;
            case TIMEOUT_MOTOR_EV:
                if(current_state == SOAK_TURN_CLKWISE) {
                    previous_state = SOAK_TURN_CLKWISE;
                    current_state = SOAK_TURN_ANTI_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0103);
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                else if(current_state == SOAK_TURN_ANTI_CLKWISE) {
                    previous_state = SOAK_TURN_ANTI_CLKWISE;
                    current_state = SOAK_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0083);
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                else if(current_state == WASH_TURN_CLKWISE) {
                    previous_state = WASH_TURN_CLKWISE;
                    current_state = WASH_TURN_ANTI_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0105);
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                else if(current_state == WASH_TURN_ANTI_CLKWISE) {
                    previous_state = WASH_TURN_ANTI_CLKWISE;
                    current_state = WASH_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0085);
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                else if(current_state == RINSE_TURN_CLKWISE) {
                    previous_state = RINSE_TURN_CLKWISE;
                    current_state = RINSE_TURN_ANTI_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0109);
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                else if(current_state == RINSE_TURN_ANTI_CLKWISE) {
                    previous_state = RINSE_TURN_ANTI_CLKWISE;
                    current_state = RINSE_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0089);
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                break;
            case TIMEOUT_SOAK_EV:
                if(current_state == SOAK_TURN_CLKWISE) {
                    previous_state = SOAK_TURN_CLKWISE;
                    current_state = WASH_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0085);
                    chVTReset(&vt_motor);
                    startTimer(WASH_TIMER, TIME_MS2I(4 * TIMEOUT_WASH));
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                else if(current_state == SOAK_TURN_ANTI_CLKWISE) {
                    previous_state = SOAK_TURN_ANTI_CLKWISE;
                    current_state = WASH_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0085);
                    chVTReset(&vt_motor);
                    startTimer(WASH_TIMER, TIME_MS2I(4 * TIMEOUT_WASH));
                    startTimer(MOTOR_TIMER, TIME_MS2I(TIMEOUT_MOTOR_CYCLE));
                }
                break;
            case TIMEOUT_WASH_EV:
                if(current_state == WASH_TURN_CLKWISE) {
                    previous_state = WASH_TURN_CLKWISE;
                    current_state = WASH_WATER_OUT;
                    chVTReset(&vt_motor);
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0045);
                }
                else if(current_state == WASH_TURN_ANTI_CLKWISE) {
                    previous_state = WASH_TURN_ANTI_CLKWISE;
                    current_state = WASH_WATER_OUT;
                    chVTReset(&vt_motor);
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0045);
                }
                break;
            case TIMEOUT_RINSE_EV:
                if(current_state == RINSE_TURN_CLKWISE) {
                    previous_state = RINSE_TURN_CLKWISE;
                    current_state = RINSE_WATER_OUT;
                    chVTReset(&vt_motor);
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0049);
                }
                else if(current_state == RINSE_TURN_ANTI_CLKWISE) {
                    previous_state = RINSE_TURN_ANTI_CLKWISE;
                    current_state = RINSE_WATER_OUT;
                    chVTReset(&vt_motor);
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0049);
                }
                break;
            case TIMEOUT_CENTRIFUGE_EV:
                if(current_state == CENTRIFUGE) {
                    previous_state = IDLE;
                    current_state = IDLE;
                    chVTReset(&vt_motor);
                    chVTReset(&vt_soak);
                    chVTReset(&vt_wash);
                    chVTReset(&vt_rinse);
                    chVTReset(&vt_centrifuge);
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                }
                break;
            case LID_OPEN_EV:
                if(current_state == IDLE){
                    palSetPad(IOPORT3, 13);
                    previous_state = IDLE;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                }
                else if(current_state == SOAK_WATER_IN) {
                    palSetPad(IOPORT3, 13);
                    previous_state = SOAK_WATER_IN;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                }
                else if(current_state == SOAK_TURN_CLKWISE) {
                    palSetPad(IOPORT3, 13);
                    previous_state = SOAK_TURN_CLKWISE;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                    pauseTimer(SOAK_TIMER);
                    pauseTimer(MOTOR_TIMER);
                }
                else if(current_state == SOAK_TURN_ANTI_CLKWISE) {
                    palSetPad(IOPORT3, 13);
                    previous_state = SOAK_TURN_ANTI_CLKWISE;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                    pauseTimer(SOAK_TIMER);
                    pauseTimer(MOTOR_TIMER);
                }
                else if(current_state == WASH_TURN_CLKWISE) {
                    palSetPad(IOPORT3, 13);
                    previous_state = WASH_TURN_CLKWISE;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                    pauseTimer(WASH_TIMER);
                    pauseTimer(MOTOR_TIMER);
                }
                else if(current_state == WASH_TURN_ANTI_CLKWISE) {
                    palSetPad(IOPORT3, 13);
                    previous_state = WASH_TURN_ANTI_CLKWISE;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                    pauseTimer(WASH_TIMER);
                    pauseTimer(MOTOR_TIMER);
                }
                else if(current_state == WASH_WATER_OUT) {
                    palSetPad(IOPORT3, 13);
                    previous_state = WASH_WATER_OUT;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                }
                else if(current_state == RINSE_WATER_IN) {
                    palSetPad(IOPORT3, 13);
                    previous_state = RINSE_WATER_IN;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                }
                else if(current_state == RINSE_TURN_CLKWISE) {
                    palSetPad(IOPORT3, 13);
                    previous_state = RINSE_TURN_CLKWISE;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                    pauseTimer(RINSE_TIMER);
                    pauseTimer(MOTOR_TIMER);
                }
                else if(current_state == RINSE_TURN_ANTI_CLKWISE) {
                    palSetPad(IOPORT3, 13);
                    previous_state = RINSE_TURN_ANTI_CLKWISE;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                    pauseTimer(RINSE_TIMER);
                    pauseTimer(MOTOR_TIMER);
                }
                else if(current_state == RINSE_WATER_OUT) {
                    palSetPad(IOPORT3, 13);
                    previous_state = RINSE_WATER_OUT;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                }
                else if(current_state == CENTRIFUGE) {
                    palSetPad(IOPORT3, 13);
                    previous_state = CENTRIFUGE;
                    current_state = OPEN_LID;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                    pauseTimer(CENTRIFUGE_TIMER);
                }
                else {
                    palClearPad(IOPORT3, 13);
                }
                break;
            case LID_CLOSE_EV:
                if (previous_state == IDLE && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = IDLE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
                }
                else if (previous_state == SOAK_WATER_IN && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = SOAK_WATER_IN;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0023);
                }
                else if(previous_state == SOAK_TURN_CLKWISE && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = SOAK_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0083);
                    resumeTimer(SOAK_TIMER);
                    resumeTimer(MOTOR_TIMER);
                }
                else if(previous_state == SOAK_TURN_ANTI_CLKWISE && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = SOAK_TURN_ANTI_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0103);
                    resumeTimer(SOAK_TIMER);
                    resumeTimer(MOTOR_TIMER);
                }
                else if(previous_state == WASH_WATER_OUT && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = WASH_WATER_OUT;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0045);
                }
                else if(previous_state == WASH_TURN_CLKWISE && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = WASH_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0085);
                    resumeTimer(WASH_TIMER);
                    resumeTimer(MOTOR_TIMER);
                }
                else if(previous_state == WASH_TURN_ANTI_CLKWISE && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = WASH_TURN_ANTI_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0105);
                    resumeTimer(WASH_TIMER);
                    resumeTimer(MOTOR_TIMER);
                }
                else if(previous_state == RINSE_WATER_IN && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = RINSE_WATER_IN;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0029);
                }
                else if(previous_state == RINSE_WATER_OUT && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = RINSE_WATER_OUT;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0049);
                }
                else if(previous_state == RINSE_TURN_CLKWISE && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = RINSE_TURN_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0089);
                    resumeTimer(RINSE_TIMER);
                    resumeTimer(MOTOR_TIMER);
                }
                else if(previous_state == RINSE_TURN_ANTI_CLKWISE && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = RINSE_TURN_ANTI_CLKWISE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0109);
                    resumeTimer(RINSE_TIMER);
                    resumeTimer(MOTOR_TIMER);
                }
                else if(previous_state == CENTRIFUGE && current_state == OPEN_LID) {
                    palClearPad(IOPORT3, 13);
                    current_state = CENTRIFUGE;
                    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0091);
                    resumeTimer(CENTRIFUGE_TIMER);
                }
                else {
                    (palReadPad(IOPORT3, 13) == PAL_HIGH) ? (palSetPad(IOPORT3, 13)) : (palClearPad(IOPORT3, 13));
                }
                break;
            default:
        }
    }
}
static THD_WORKING_AREA(wa_stateMachine, 2048);

int main(void) {

    halInit();
    chSysInit(); 
    queueInit();
    palSetPadMode(IOPORT3, 13, PAL_MODE_OUTPUT_PUSHPULL);
    palClearPad(IOPORT3, 13);
    /* pinos de saida */
    palSetGroupMode(IOPORT1, GPIO_OUTPUT_MASK, 0, PAL_MODE_OUTPUT_PUSHPULL);
    /* pinos de entrada */
    palSetGroupMode(IOPORT2, GPIO_INPUT_MASK, 0, PAL_MODE_INPUT_PULLUP);
    /* desliga todos os leds */
    palWriteGroup(IOPORT1, GPIO_OUTPUT_MASK, 0, 0x0001);
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

    ptr_stateMachine = chThdCreateStatic(wa_stateMachine, sizeof(wa_stateMachine), NORMALPRIO + 1, stateMachine, NULL);
    
    while(1) {

        chThdSleepMilliseconds(500);
    }
    return 0;
}
