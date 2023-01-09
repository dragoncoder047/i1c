# I<sup>1</sup>C Specification

## Wiring

Like its 2-wire counterpart, I<sup>1</sup>C is an open-collector bus. The pin connecting each node to the bus can only pull the bus wire low, or read the state of the bus wire (high-impedance). Microcontrollers do this by toggling the pin between "input" and "output" modes. That is, in Arduino code:

```cpp
#define i1c_pin NNNN // whatever you like
void write_1() {
    pinMode(i1c_pin, INPUT);
}
void write_0() {
    pinMode(i1c_pin, OUTPUT);
    digitalWrite(i1c_pin, LOW);
}
int read_bus() {
    write_1();
    return digitalRead(i1c_pin);
}
```

## Timing

I<sup>1</sup>C uses an asynchronous adaptive clocking scheme. Each bit in the data stream is represented as a period of "low" followed by a period of "high", and which bit (0 or 1) is encoded by the ratio of the pulses' lengths. If "low" is longer than "high", it encodes a 0; if "low" is shorter than "high", it encodes a 1.

![timing diagram](https://kroki.io/wavedrom/svg/eNqrVijOTM9LzLFSiOZSUKhWyEvMTbVSUApOzUvJzEtXMFAvVtJRgIPyxDKQrKGBHgIpKdTqYNNqiEOroR4c4dJqYAiCenp6EAOQbQXqA5PoWkMyUhUKEotKFEoyEksUchNLSlKLgPbDtVoiEFhrbC0AtfY_Hg==) *View this in "light mode"!!*

<!--

{ signal: [
  { name: "Sending 0's",          wave: "10.10.10.10.1" },
  { name: "Sending 1's",          wave: "101.01.01.01." },
  { name: "Sending 010101...",    wave: "10.101.0.101." },
  { name: "The part that matters",wave: "1091091091091" },
]}

-->

The bus idles in the "high" state, so after the bus goes high, if it remains high for a long period of time (>=1ms) the bus is considered "idle" again, otherwise data is still coming. This means that the minimum bit-rate is at least 1 bit per millisecond, or "1000 baud" (horribly slow compared to some other protocols, but this is the *minimum*, not the maximium, and things can certainly be sped up considerably.)

The way this can be implemented in a microcontroller is to use a hardware timer to count up timestamps and record the time of the last falling and rising edge pair; then on the falling edge interrupt, `bit = last_rising_time < ((last_falling_time + now_time) / 2)`. The effect of this is that the microcontroller can "sample" the data line in the middle of the bit frame, without having to know how long the bit frame will be when it starts.

## Arbitration

TODO

## Data Format

TODO
