
// --- signal detection constants. unit is 7 * 0.0049 volts.
// with amplifier, phototransistor signal is peaking at 2 volts.
#define NOISE_CEILING 10 // signal falls below this value => trailing edge
#define SIGNAL_FLOOR 700 // signal rises above this value => rising edge

// usecs required to be sure that a pulse is real
#define PULSE_CHECK_WIDTH 4000 

// msecs to sleep after detecting real pulse. typical time between rising edges
// is 60 ms, so PULSE_WAIT_MSEC + PULSE_CHECK_WIDTH / 1000 = 55 ms, putting us
// about 5 ms before the next expected rising edge.
#define PULSE_WAIT_MSEC     51 

#define MAX_PULSE_SPACING 80000

// TODO: improve to handle wrapping
#define MICROS_SUB(a, b) ((a) - (b))

// --- state machine states
#define STATE_NONE     100 // between pulse groups
#define STATE_WAITING  101 // waiting for another pulse in this group
#define STATE_IN_PULSE 102 // inside a pulse
#define STATE_FINISHED 103 // at end of pulse group, ready to save row

// TODO: determine if A0 or 0 is appropriate for analogRead()
// (online docs and example code differs)
#define PIN_PHOTOTRANSISTOR A0
#define PIN_AIRTEMP         A2
#define PIN_WATERTEMP       A1

int micros_now, val;
int rising_edge_micros;
int pulse_count;
int s;

struct record r;

void setup() {
  // TODO: add init code for the card reader
  pulse_count = 0;

  // Initialize in-memory data buffer
  buf_init(BUFFER_SIZE);
}

void loop() {
  micros_now = micros();
  val = analogReadSum7(PIN_PHOTOTRANSISTOR);
  
  switch (s) {
    case STATE_IN_PULSE:
      if (MICROS_SUB(micros_now, rising_edge_micros) > PULSE_CHECK_WIDTH) {
        // we know this pulse is real. save some cycles.
        pulse_count++;
        s = STATE_WAITING;
        // TODO: find out if this is actually saving power
        delay(PULSE_WAIT_MSEC);
        break;
      }
      if (val < NOISE_CEILING) { // leaving pulse (trailing edge)
        // if we are detecting a trailing edge, this pulse is weird!
        // TODO: set up error variable with available info, set error flag.
        // (then STATE_FINISHED will save the row)
        // AND delay a few seconds so i don't fill up the DB
      }
      break;

    case STATE_WAITING:
      if (MICROS_SUB(micros_now, rising_edge_micros) > MAX_PULSE_SPACING) {
        // This is the moment when we report # of pulses and real time.
        // desktop version: printf("%d,%d\n", micros_now, pulse_count);
        // data to record in each row:
        //   real time (long)
        //   # of pulses (int)
        //   air temp (int)
        //   water temp (int)
        // TODO: set up row for saving in STATE_FINISHED
        
        pulse_count = 0;
        s = STATE_NONE;
      }
      // we omit "break;" so we can evaluate the next rule too

    case STATE_NONE:
      if (val > SIGNAL_FLOOR) { // entering pulse (rising edge)
        
        rising_edge_micros = micros_now;
        s = STATE_IN_PULSE;
      }
      break;
      
    case STATE_FINISHED: // finished with a pulse group. save a row.
      r.ms = rising_edge_micros;
      r.pulse_count = pulse_count;
      buf_add(&r);
      break;
  }
}

int analogReadSum7(int pin) {
  static int val;
  val = analogRead(pin);
  val += analogRead(pin);
  val += analogRead(pin);
  val += analogRead(pin);
  val += analogRead(pin);
  val += analogRead(pin);
  val += analogRead(pin);
  return val;
}

