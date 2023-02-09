#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define I1C_PIN PB3
#define MY_ADDRESS 0x12 // Change this...

#define I1C_LOW() (DDRB |= (1 << I1C_PIN))
#define I1C_RELEASE() (DDRB &= ~(1 << I1C_PIN))
#define I1C_READ() ((PINB >> I1C_PIN) & 1)

#define TIMER() SREG

void i1c_setup(void) {
    I1C_RELEASE(); // Don't pull it low and cause a glitch
    PORTB &= ~(1 << I1C_PIN); // Make pin force low by default
    GIMSK |= 1 << PCIE; // Enable pin change interrupts on the I1C pin
    PCMSK |= 1 << I1C_PIN;
    sei();
}

enum i1c_state {
    IDLE,
    LOST_ARBITRATION,
    ADDRESS_NOMATCH,
    SENDING,
    RECIEVING,
    BUFFER_OVERFLOW
};

volatile uint8_t state = IDLE;

#define BUFFERSIZE 128 // ATtiny85 has 512 bytes(!!) of RAM so this is undersize to accommodate that.
volatile uint8_t buffer[BUFFERSIZE];
uint8_t bit_pointer = 0; // Tail of the message
uint8_t byte_pointer = 0; // Tail of the message
volatile bool data_ready = false;

volatile unsigned long last_rise_time = 0;
volatile unsigned long last_fall_time = 0;

ISR(PCINT0_vect) {
    switch(state) {
        case SENDING:
        case LOST_ARBITRATION:
        case ADDRESS_NOMATCH:
        case BUFFER_OVERFLOW:
            return;
        default:
            break;
    }
    unsigned long now_time = TIMER();
    uint8_t pstate = I1C_READ();
    if (pstate) {
        last_rise_time = now_time;
        return;
    }
    if (now_time - last_rise_time > 1000) {
        uint8_t bit = last_rise_time < ((last_fall_time + now_time) / 2);
        buffer[byte_pointer] |= bit << bit_pointer;
        bit_pointer++;
        if (bit_pointer == 8) {
            bit_pointer = 0;
            byte_pointer++;
            if (byte_pointer == BUFFERSIZE) {
                state = BUFFER_OVERFLOW;
                goto finished;
            }
        }
    }
    last_fall_time = now_time;
    state = RECIEVING;
    if (bit_pointer != 0) return;
    if (byte_pointer == 2) {
        uint8_t tgt_address = buffer[1];
        if (tgt_address && tgt_address != MY_ADDRESS) {
            state = ADDRESS_NOMATCH;
            goto finished;
        }
    }
    else if (byte_pointer >= 3 && byte_pointer >= buffer[2]) {
        state = IDLE;
        data_ready = true;
        goto finished;
    }
    return;
    finished: byte_pointer = bit_pointer = 0;
}

bool i1c_can_send() {
    unsigned long now = TIMER();
    if (now - last_rise_time > 1000) {
        state = IDLE;
        return true;
    }
    return state == IDLE;
}

#if F_CPU == 1000000
#define DLY() asm("nop\n\tnop")
#else // default F_CPU === 8 Mhz
#define DLY() asm("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop")
#endif

inline bool i1c_send_byte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        I1C_LOW();
        DLY();
        DLY();
        if (byte & (1 << i)) {
            I1C_RELEASE();
            DLY();
            if (I1C_READ() != 1) {
                state = LOST_ARBITRATION;
                return false;
            }
            DLY();
        } else {
            DLY();
            DLY();
            I1C_RELEASE();
        }
        DLY();
        if (I1C_READ() != 1) {
            state = LOST_ARBITRATION;
            return false;
        }
        DLY();
    }
    return true;
}

bool i1c_send(uint8_t to, uint8_t* message, uint8_t len) {
    i1c_can_send(); // Noop call to activate timeout
    if (state == LOST_ARBITRATION) return false;
    while (!i1c_can_send()); // wait until bus is idle
    state = SENDING;
    if (!i1c_send_byte(MY_ADDRESS)) return false;
    if (!i1c_send_byte(to)) return false;
    if (!i1c_send_byte(len)) return false;
    for (uint8_t i = 0; i < len; i++) {
        if (!i1c_send_byte(message[i])) return false;
    }
    state = IDLE;
    return true;
}

bool i1c_receive(uint8_t* sender, uint8_t* len, uint8_t** copybuffer, uint8_t bufsz) {
    if (!data_ready) return false;
    uint8_t count = 0;
    *sender = buffer[0];
    *len = buffer[2];
    *len = bufsz < *len ? bufsz : *len;
    for (uint8_t i = 0; i < *len; i++) {
        *copybuffer[i] = buffer[3 + i];
    }
    data_ready = false;
    return true;
}

// ------------- Begin dummy application -- simply echos A * B ---------------------
//               Note that some of this is still required, to
//               implement device discovery messages (see lines
//               181-182)

void setup() {
    i1c_setup();
}



void loop() {
    static uint8_t len;
    static uint8_t source_addr;
    static uint8_t out_buffer[1];
    static uint8_t in_buffer[2];
    uint8_t got_something = i1c_receive(&source_addr, &len, (uint8_t**)&in_buffer, 2);
    if (got_something) {
        if (!source_addr && !len) {
            i1c_send(source_addr, NULL, 0); // Send reply to who is here message
        } else {
            out_buffer[0] = in_buffer[1] * in_buffer[0];
            i1c_send(source_addr, out_buffer, 1);
        }
    }
}
