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

The way this can be implemented in a microcontroller is to use a hardware timer to count up timestamps and record the time of the last falling and rising edge pair; then on the falling edge interrupt, `bit = last_rise_time < ((last_fall_time + now_time) / 2)`. The effect of this is that the microcontroller can "sample" the data line in the middle of the bit frame, without having to know how long the bit frame will be when it starts.

## Data Format

There is no one "master" / "slave" designation as with conventional protocols that have a separate clock line controlled solely by the master. Whenever a node needs to send a message, it just blasts it out onto the bus. Any message acknowledgement replies are optional and only needed for super-critical operations.

To facilitate communication each node is assigned an address (1 byte or so).

The message format I came up with has a 3 byte header followed by a payload. Byte 1 is the source address, that is, the address of the node that is sending the message (so the target node can reply). Byte 2 is the target node's address. Byte 3 is the payload length, and after that is the payload. Because of the 1-byte restriction on the length, messages cannot be longer than 255 bytes.

Address 0 is the special "all-call" node, that is, a message directed to node 0 is really directed to all nodes simultaneously. With this, a few special messages can be made:

* `nn 00 00` (from node `nn`, to everybody, length of 0) -- this is the SCAN message, all nodes are expected to respond with a HELLO message, indicating their presence.
* `mm nn 00` (from node `mm`, to node `nn`, length of 0) -- this is the HELLO message, indicating that node `mm` exists, sent in reply to a SCAN message from node `nn`.

## State Machine

The states are:
```cpp
enum i1c_state {
    IDLE,
    LOST_ARBITRATION,
    ADDRESS_NOMATCH,
    SENDING,
    RECIEVING
};
```

### Sending

1. If not in `IDLE` state, wait until `(now_time - last_rise_time) > 1ms`.
2. Transition to `SENDING` state.
3. Start sending the bits of this node's address. When the bus needs to be made high, wait until either it actually does so, or a 10&micro;s timeout expires.
    1. If it timed out, another node is in the middle of sending something else. Transition to the `LOST_ARBITRATION` state and go back to 1.
    2. If this bit was transmitted successfully, transmit the next one.
4. Once header is transmitted (3 bytes), transmit the payload without checking for bus contention.
5. When all data has been sent, transition immediately back to the `IDLE` state.

### Receiving

These all happen in the pin-change interrupt routines...

1. On the rising edge, record the time stamp.
2. If in the `SENDING`, `LOST_ARBITRATION`, or `ADDRESS_NOMATCH` state, stop.
3. On the falling edge, record the time stamp.
4. Transition into the `RECEIVING` state unless already in the `ADDRESS_NOMATCH` state.
5. Use the formula above to determine which bit was sent if the time since the last falling edge is < 1ms.
6. Load the bit into the recieve buffer.
7. If the target address (byte 2) is not this node's address, transition into the `ADDRESS_NOMATCH` state and stop.
8. Otherwise continue recieving until all bytes are received, set a "data ready" flag to indicate to the main thread a command is waiting, and transition back into `IDLE`.
