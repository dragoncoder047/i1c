#include <stdint.h>
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
    PORTB = PORTB & ~(1 << I1C_PIN); // Make pin force low by default
    GIMSK |= 1 << PCIE;
    PCMSK |= 1 << I1C_PIN;
    sei();
}

enum i1c_state {
    IDLE,
    LOST_ARBITRATION,
    ADDRESS_NOMATCH,
    SENDING,
    RECIEVING
};

volatile uint8_t state = IDLE;

#define BUFFERSIZE 128
volatile uint8_t buffer[BUFFERSIZE]; // ATtiny85 has 512 bytes(!!) of RAM so this is undersize to accommodate that.
uint8_t bit_pointer = 0; // Tail of the message
uint8_t byte_pointer = 0; // Tail of the message
uint8_t start_pointer = 0; // Head of the message
volatile bool data_ready = false;

#define BUFFER_AT(idx) (buffer[((idx) + start_pointer) % BUFFERSIZE])
#define CURRENT_INDEX ((byte_pointer - start_pointer + BUFFERSIZE) % BUFFERSIZE)
#define IS_POS(idx, op) (bit_pointer == 0 && CURRENT_INDEX op (idx)) // !! Note, 1 based indexing !!

volatile unsigned long last_rise_time = 0;
volatile unsigned long last_fall_time = 0;

ISR(PCINT0_vect) {
    unsigned long now_time = TIMER();
    uint8_t pstate = I1C_READ();
    if (pstate) {
        last_rise_time = now_time;
        return;
    }
    else last_fall_time = now_time;
    switch(state) {
        case SENDING:
        case LOST_ARBITRATION:
        case ADDRESS_NOMATCH:
            return;
        default:
            break;
    }
    if (now_time - last_rise_time > 1000) {
        uint8_t bit = last_rise_time < ((last_fall_time + now_time) / 2);
        buffer[byte_pointer] |= bit << bit_pointer;
        bit_pointer++;
        if (bit_pointer == 8) {
            bit_pointer = 0;
            byte_pointer = (byte_pointer + 1) % BUFFERSIZE;
        }
    }
    state = RECIEVING;
    if IS_POS(2, ==) {
        uint8_t tgt_address = BUFFER_AT(2);
        if (tgt_address && tgt_address != MY_ADDRESS) {
            state = ADDRESS_NOMATCH;
            start_pointer = byte_pointer;
        }
    }
    else if IS_POS(3, >=) {
        if (CURRENT_INDEX >= BUFFER_AT(3)) {
            state = IDLE;
            data_ready = true;
        }
    }
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
#define DLY() __asm__ __volatile__ ("nop\n\tnop")
#else // default F_CPU === 8 Mhz
#define DLY() __asm__ __volatile__ ("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop")
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
    if (state == LOST_ARBITRATION) return false;
    while (!i1c_can_send()); // wait until can send
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

