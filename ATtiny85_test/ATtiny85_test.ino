#define i1c_pin PB3

#define i1c_low() (DDRB |= (1 << i1c_pin))
#define i1c_release() (DDRB &= ~(1 << i1c_pin))
#define i1c_read() ((PINB >> i1c_pin) & 1)

void i1c_setup(void) {
    i1c_release(); // Don't pull it low and cause a glitch
    PORTB = PORTB & ~(1 << i1c_pin); // Make pin force low by default
    // The core I use uses timer 0 for millis() / micros() so this doesn't affect that
    TCCR1 = 0 << CTC1 | 0 << PWM1A | 5 << CS10;  // CTC mode, 500kHz clock (~2us per tick)
    GTCCR = 0 << PWM1B;
}

enum i1c_state {
    IDLE,
    LOST_ARBITRATION,
    ADDRESS_NOMATCH,
    SENDING,
    RECIEVING
};

i1c_state state;

#define BUFFERSIZE 128
uint8_t buffer[BUFFERSIZE]; // ATtiny85 has 512 bytes(!!) of RAM so this is undersize to accommodate that.
uint8_t bit_pointer = 0;
uint8_t byte_pointer = 0;

#define BAH

void
