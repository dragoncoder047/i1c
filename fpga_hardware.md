# I<sup>1</sup>C implemented in hardware

The general input/output pin would be the same setup as in I<sup>2</sup>C, with an open-drain output and an input latch to synchronize the input with the system clock. Aside from this, I<sup>1</sup>C is completely different because the data is interleaved with the clock.

## Receiving

The "idle timeout" is handled by a counter, that is connected to the system clock. When the bus goes low, this counter is cleared too, and when it overflows, it disables its counting-up and outputs a signal to indicate "idle".

The counter is also used as the timer to determine which bit was sent. On each edge, the timer value is latched into one of two latches (on the falling edge, the value is latched *before* the counter is cleared). The equation `bit = last_rise_time < ((last_fall_time + now_time) / 2)` reduces to `bit = last_rise_time < (now_time / 2)`. The `/ 2` can be eliminated by bitshifting.

Then, on each falling edge of the data line (except the first) the values of the timer latches are compared, and the bit is shifted into a serial-in-parallel-out shift register.

An 8-bit counter counts the bits as they come in, clocked by the falling edge of the data line, and when it overflows (when 8 bits have been read in) the shift register's contents are latched into the input buffer and the buffer pointer is incremented.

## Sending

TODO
