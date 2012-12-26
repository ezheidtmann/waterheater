// --- signal detection constants. unit is 7 * 0.0049 volts.
// with amplifier, phototransistor signal is peaking at 2 volts.
#define NOISE_CEILING 10 // signal falls below this value => trailing edge
#define SIGNAL_FLOOR 700 // signal rises above this value => rising edge

// msecs required to be sure that a pulse is real
#define PULSE_CHECK_WIDTH 4

// msecs to sleep after detecting real pulse. typical time between rising edges
// is 60 ms, so PULSE_WAIT_MSEC + PULSE_CHECK_WIDTH / 1000 = 55 ms, putting us
// about 5 ms before the next expected rising edge.
#define PULSE_WAIT_MSEC     51

// msecs to wait for next rising edge; if no pulse is found within this window,
// we assume the pulse train is done.
#define MAX_PULSE_SPACING 80

// msecs to wait before reporting "no pulse train" (ROWERR_NOPULSE)
#define MAX_PULSE_TRAIN_SPACING 60000

// size of buffer, in number of records
#define BUFFER_SIZE 100

// --- state machine states
#define STATE_NONE     100 // between pulse groups
#define STATE_WAITING  101 // waiting for another pulse in this group
#define STATE_IN_PULSE 102 // inside a pulse
#define STATE_FINISHED 103 // at end of pulse group, ready to save row
#define STATE_FLUSHING 104 // writing buffer to SD card

// --- pins
#define PIN_PHOTOTRANSISTOR A0
#define PIN_AIRTEMP         A2
#define PIN_WATERTEMP       A1
#define PIN_LED             13

// --- Error codes 
// per group
#define ERR_SD               0b00000001
// per row
#define ROWERR_SHORTPULSE    0b0001
#define ROWERR_TOOMANYPULSES 0b0010
#define ROWERR_NOPULSE       0b0100

// errors particular to a row of data
#define ERRORS_ROW (ERROR_TOOMANYPULSES | ERROR_SHORTPULSE)

//////////////////////////////

/**
 * Compute a - b for unsigned long, with wrapping handling.
 *
 * If (a > b), then this is standard subtraction. Otherwise, a has wrapped
 * but b has not. We compute differences from the edges and add.
 */
#define ULONG_SUB(a, b) \
  ( (a) > (b) ? (a) - (b) : \
    (0xffffffUL - (b)) + (a) )

#include <SdFat.h>
#include <Wire.h>
#include <RTClib.h>

#include "record.h"

RTC_DS1307 RTC;

// --- Globals
// milliseconds now, and last rising edge
unsigned long secs_now, millis_now, rising_edge_millis;
// number of pulses detected; current analogRead value
int pulse_count, val;
// current state machine state
int s;
// error flags
byte row_error;
byte group_error;

// header
struct records_header h;

// chipSelect pin for SD library
const int SDpin = 10;

// current record for output file
struct record r;

// button press flag
volatile bool flush_requested;
bool buffer_save_success;

int buf_write(SdFile* output, struct records_header* = NULL);

void setup() {
  // Initialize LED pin; turn on for boot
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // Initialize RTC, SD
  RTC.begin();
  sd_ready();

  // Initialize in-memory data buffer
  buf_init(BUFFER_SIZE);

  // Set up flush button handler
  flush_requested = false;
  attachInterrupt(0, button_handler, LOW);
  
  row_error = 0;
  group_error = 0;
  pulse_count = 0;

  // Turn off LED
  digitalWrite(PIN_LED, LOW);
}

void loop() {
  millis_now = millis();
  val = analogReadSum7(PIN_PHOTOTRANSISTOR);

  switch (s) {
    case STATE_IN_PULSE:
      if (ULONG_SUB(millis_now, rising_edge_millis) > PULSE_CHECK_WIDTH) {
        // we know this pulse is real. save some cycles.
        pulse_count++;
        // check for too many pulses
        if (pulse_count = 0b1111) {
          row_error |= ROWERR_TOOMANYPULSES;
          s = STATE_FINISHED;
          break;
        }

        // TODO: find out if this is actually saving power
        delay(PULSE_WAIT_MSEC);
        s = STATE_WAITING;
      }
      else if (val < NOISE_CEILING) { // leaving pulse (trailing edge)
        // if we are detecting a trailing edge, this pulse is weird!
        // (our delay() above should skip well past the trailing edge)
        // TODO: set up error variable with available info, set error flag.
        // (then STATE_FINISHED will save the row)
        // AND delay a few seconds so i don't fill up the DB
        row_error |= ROWERR_SHORTPULSE;
        s = STATE_FINISHED;
      }
      break;

    case STATE_WAITING:
      if (ULONG_SUB(millis_now, rising_edge_millis) > MAX_PULSE_SPACING) {
        // Enough time has passed that we are sure we have passed the last
        // pulse. We are ready to save a row.

        s = STATE_FINISHED;
        break; // break here to go straight to STATE_FINISHED.
      }
      // we omit "break;" so we can evaluate the next rule too:
      // When waiting, we need to detect a rising edge.

    case STATE_NONE:
      if (val > SIGNAL_FLOOR) { // entering pulse (rising edge)
        rising_edge_millis = millis_now;
        s = STATE_IN_PULSE;
      }
      // Too much time has passed with no signal. Write a row to report this.
      else if (ULONG_SUB(millis_now, rising_edge_millis) > MAX_PULSE_TRAIN_SPACING) {
        row_error |= ROWERR_NOPULSE;
        s = STATE_FINISHED;
      }
      else if (flush_requested) {
        // N.B. We run the risk of missing a pulse. Low risk, but perhaps worth a check?
        s = STATE_FLUSHING;
      }
      break;

    case STATE_FINISHED: // finished with a pulse group. save a row.
      r.millis      = rising_edge_millis;
      r.pulse_count = (pulse_count & 0b1111) | (row_error << 4);
      r.air_temp    = analogReadSum7(PIN_AIRTEMP);
      r.water_temp  = analogReadSum7(PIN_WATERTEMP);

      buf_add(&r);
      
      row_error = 0;
      pulse_count = 0;

      if (buf_full() || flush_requested) {
        s = STATE_FLUSHING;
      }
      break;

    case STATE_FLUSHING:
      SdFile *outfile;
      if (outfile = sd_ready()) {
        digitalWrite(PIN_LED, HIGH);
        
        h.rtc_secs = RTC.now().unixtime();
        h.millis = millis_now;
        h.flags = group_error;
        
        buf_write(outfile, &h);

        digitalWrite(PIN_LED, LOW);
      }
      else { // SD not ready; set error flag and clear buffer
        group_error |= ERR_SD;
        buf_write(NULL, NULL);
      }
      
      s = STATE_NONE;
      break;
  }

  error_flash(group_error);
}

/**
 * Read the specified analog pin 7 times and return the sum
 */
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

/**
 * Get pointer to output file, if it is available. NULL otherwise.
 */
SdFile* sd_ready() {
  static const int chipSelect = 10; // SS pin
  static SdFile outputfile;
  pinMode(SDpin, OUTPUT);
  static SdFat sd;

  // TODO: improve this function to mount / re-mount an SD card
  // as needed. Currently we can only do it once with SD library.

  int phase = -1;
  int success = 1;

  while (success) {
    phase++;
    switch (phase) {
      case 0:
        success = sd.begin(chipSelect);
        break;

      case 1:
        // TODO: choose an appropriate output file name
        success = outputfile.open("logname.bin", O_WRITE | O_CREAT);
        break;

      case 2:
        return &outputfile;
    }
  }

  return NULL;
}

/**
 * Force sync to card
 */
bool sd_sync() {
  sd_ready()->sync();
}

/**
 * Flash error code(s)
 *
 * TODO: Figure out some kind of flash timing scheme for multiple codes
 */
void error_flash(byte errno) {
  static unsigned long last = 0;
  static unsigned long m;

  if (!errno) { return; }

  m = millis();
  if (errno & ERR_SD) {
    if (m - last > 100) {
      digitalWrite(PIN_LED, HIGH);
    }
    else if (m - last > 200) {
      digitalWrite(PIN_LED, LOW);
      last = m;
    }
  }
}

/**
 * Interrupt callback for button press
 */
void button_handler() {
  flush_requested = true;
}
