#include <ADC.h>

// Select Tools > USB Type > MIDI + Serial


// In a stready state, how strong does the signal need to be to fire?
#define THRESHOLD_GATE 16
// When debouncing, how muchn stronger does the signal need to be to fire?  We decrease
// exponentially from here, dropping by 2x every DEBOUNCE_MS / DEBOUNCE_SECTIONS ms.
#define INITIAL_EXTRA_GATE 512

#define BROAD_LEN_THRESHOLD_MS 48

#define DEBOUNCE_MS 50
#define BIAS_ESTIMATION_SAMPLES 0x10000

#define DETECTION_WINDOW_MS 100

#define DEBOUNCE_SECTIONS 8;

#define BUFSIZE 4000

#define N_PINS 18

ADC adc;

int val[N_PINS];
int recent_max[N_PINS];
int recent_min[N_PINS];
int broad_dir[N_PINS];
int broad_len[N_PINS];
int max_broad_dir[N_PINS];
int max_broad_len[N_PINS];
int n;
int bias_count;
long bias_val[N_PINS];
int cur_bias[N_PINS];
bool calibrating;
int buf[BUFSIZE*N_PINS];
int loc;
bool should_debug_print;
int midi_note;
int strongest_pin;
unsigned long start_micros;
float ms_per_sample;
float samples_per_ms;
int debounce_timer;
int debounce_section_timer;
int debounce_samples;
int debounce_section_samples;
int detection_window_samples;
int broad_len_threshold_samples;
int effective_gate;
int extra_gate;

bool passed_gate;
unsigned long gate_micros;
int detection_window_loc;

void setup()
{   
  Serial.begin(38400);

  adc.adc0->setResolution(10);
  adc.adc0->setAveraging(8);
  adc.adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_HIGH_SPEED);
  adc.adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED);
  adc.adc0->setReference(ADC_REFERENCE::REF_3V3);
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);
  pinMode(A6, INPUT);
  pinMode(A7, INPUT);
  pinMode(A8, INPUT);
  pinMode(A9, INPUT);
  pinMode(A10, INPUT);
  pinMode(A11, INPUT);
  pinMode(A12, INPUT);
  pinMode(A13, INPUT);
  pinMode(A14, INPUT);
  pinMode(A15, INPUT);
  pinMode(A16, INPUT);
  pinMode(A17, INPUT);

  n = 0;
  debounce_timer = 0;
  debounce_section_timer = 0;
  detection_window_loc = 0;
  passed_gate = 0;
  calibrating = true;
  loc = 0;
  bias_count = 0;
  should_debug_print = false;
  effective_gate = THRESHOLD_GATE;
  extra_gate = 0;

  for (int pin = 0 ; pin < N_PINS; pin++) {
    bias_val[pin] = 0;
    cur_bias[pin] = 512;
    recent_min[pin] = 0;
    recent_max[pin] = 0;
    broad_dir[pin] = 1;
    broad_len[pin] = 0;
    max_broad_dir[pin] = 1;
    max_broad_len[pin] = 0;
    for (int i = 0; i < BUFSIZE; i++) {
      buf[i + pin*BUFSIZE] = 0;
    }
  }

  Serial.begin(38400);
}

void debug_print() {
  if (!should_debug_print) {
    return;
  }
  int window_end = BUFSIZE + BUFSIZE + loc;
  int window_start = window_end - BUFSIZE;
  //for (int pin = 0 ; pin < N_PINS; pin++) {
    int pin = strongest_pin;
    for (int i = window_start; i < window_end; i++) {
      Serial.printf("%d ", buf[i % BUFSIZE + pin*BUFSIZE]);
    }
    Serial.println();
  //}
  should_debug_print = true;
}

void loop()                     
{
  val[0] = adc.analogRead(A0, ADC_0);
  val[1] = adc.analogRead(A1, ADC_0);
  val[2] = adc.analogRead(A2, ADC_0);
  val[3] = adc.analogRead(A3, ADC_0);
  val[4] = adc.analogRead(A4, ADC_0);
  val[5] = adc.analogRead(A5, ADC_0);
  val[6] = adc.analogRead(A6, ADC_0);
  val[7] = adc.analogRead(A7, ADC_0);
  val[8] = adc.analogRead(A8, ADC_0);
  val[9] = adc.analogRead(A9, ADC_0);
  val[10] = adc.analogRead(A10, ADC_0);
  val[11] = adc.analogRead(A11, ADC_0);
  // Pins 26 27, 38, 39 are only on the second ADC https://forum.pjrc.com/index.php?threads/teensy-4-1-adc-channels.72373/
  // All others except 24 and 25 (A10 and A11) can go to either.
  val[12] = adc.analogRead(A12, ADC_1);
  val[13] = adc.analogRead(A13, ADC_1);
  val[14] = adc.analogRead(A14, ADC_1);
  val[15] = adc.analogRead(A15, ADC_1);
  val[16] = adc.analogRead(A16, ADC_0);
  val[17] = adc.analogRead(A17, ADC_0);

  if (calibrating && bias_count == 0) {
    Serial.printf("calibrating...\n");
    start_micros = micros();
  }

  bias_count++;
  for (int pin = 0 ; pin < N_PINS; pin++) {
    bias_val[pin] += val[pin];
  }

  if (bias_count == BIAS_ESTIMATION_SAMPLES) {
    for (int pin = 0 ; pin < N_PINS; pin++) {
      cur_bias[pin] = bias_val[pin] / bias_count;
      bias_val[pin] = 0;
      if (calibrating) {
        Serial.printf("pin %d bias %d\n", pin, cur_bias[pin]);
      }
    }

    if (calibrating) {
      unsigned long duration_micros = micros() - start_micros;
      ms_per_sample = (1.0/1000) * duration_micros / bias_count;
      samples_per_ms = 1000.0 * bias_count / duration_micros;

      debounce_samples = DEBOUNCE_MS * samples_per_ms;
      debounce_section_samples = debounce_samples / DEBOUNCE_SECTIONS;
      detection_window_samples = DETECTION_WINDOW_MS * samples_per_ms;
      broad_len_threshold_samples = BROAD_LEN_THRESHOLD_MS * samples_per_ms;

      Serial.printf("Calibrated with %d samples in %luus.  Sampling rate is %.0f Hz and each sample represents %.2fms\n",
                    bias_count, duration_micros,
                    1000 * samples_per_ms,
                    ms_per_sample);
      Serial.printf("Debounce %dms, %d samples\n", DEBOUNCE_MS, debounce_samples);
      Serial.printf("Detection window %dms, %d samples\n", DETECTION_WINDOW_MS, detection_window_samples);
      calibrating = false;
    }
    bias_count = 0;
  }
  if (calibrating) {
    return;
  }

  for (int pin = 0 ; pin < N_PINS; pin++) {
    val[pin] -= cur_bias[pin];
    buf[loc % BUFSIZE + pin*BUFSIZE] = val[pin];
  }
  loc++;
  if (loc == BUFSIZE) {
    loc = 0;
  }

  bool threshold_passed = false;
  for (int pin = 0 ; pin < N_PINS; pin++) {
    if (val[pin] > effective_gate || val[pin] < -effective_gate) {
      threshold_passed = true;
    }
  }

  if (debounce_timer > 0) {
    debounce_timer--;
    debounce_section_timer--;

    if (debounce_section_timer == 0) {
      extra_gate = extra_gate / 2;
      effective_gate = THRESHOLD_GATE + extra_gate;
      debounce_section_timer = debounce_section_samples;
    }

    if (debounce_timer == 0 || threshold_passed) {
      debounce_timer = 0;
      usbMIDI.sendNoteOff(midi_note, 0, /*channel=*/ 1);
      debug_print();
    }
  }
  
  if (passed_gate) {
    detection_window_loc++;

    for (int pin = 0 ; pin < N_PINS; pin++) {
      if (val[pin] < recent_min[pin]) {
        recent_min[pin] = val[pin];
      }
      if (val[pin] > recent_max[pin]) {
        recent_max[pin] = val[pin];
      }
      if (val[pin] > THRESHOLD_GATE) {
        if (broad_dir[pin] > 0) {
          broad_len[pin]++;
        } else {
          broad_len[pin] = 0;
          broad_dir[pin] = 1;
        }
      } else if (-val[pin] > THRESHOLD_GATE) {
        if (broad_dir[pin] < 0) {
          broad_len[pin]++;
        } else {
          broad_len[pin] = 0;
          broad_dir[pin] = -1;
        }
      } else {
        broad_len[pin] = 0;
      }
      if (broad_len[pin] > max_broad_len[pin]) {
        max_broad_len[pin] = broad_len[pin];
        max_broad_dir[pin] = broad_dir[pin];
      }
    }

    if (detection_window_loc == detection_window_samples) {
      strongest_pin = 0;
      bool is_up = false;
      int strength = 0;
      for (int pin = 0 ; pin < N_PINS; pin++) {
        int pin_strength = recent_max[pin];
        if (-recent_min[pin] > pin_strength) {
          pin_strength = -recent_min[pin];
        }
        if (pin_strength > strength) {
          strength = pin_strength;
          strongest_pin = pin;
          if (max_broad_len[pin] > broad_len_threshold_samples) {
            is_up = max_broad_dir[pin] < 0;
          } else {
            is_up = recent_max[pin] > -recent_min[pin];
          }
        }
        recent_max[pin] = recent_min[pin] = broad_len[pin] =
           broad_dir[pin] = max_broad_len[pin] = max_broad_dir[pin] = 0;
      }
      //Serial.printf("%d %c %d %ld\n", strongest_pin, is_up ? '<' : '>', strength, micros() - gate_micros);
      int midi_velocity = strength / 4;  // 0-511 to 0-127
      if (midi_velocity > 127) {
        midi_velocity = 127;
      }
      midi_note = 36 + strongest_pin * 2 - 1 + is_up;
      usbMIDI.sendNoteOn(midi_note, midi_velocity, /*channel=*/ 1);

      passed_gate = false;
      detection_window_loc = 0;
      debounce_timer = debounce_samples;
      debounce_section_timer = debounce_section_samples;
      extra_gate = INITIAL_EXTRA_GATE;
      effective_gate = THRESHOLD_GATE + extra_gate;
    }
  } else if (threshold_passed) {
    passed_gate = true;
    gate_micros = micros();
  }

  while (usbMIDI.read()) {
    // ignore incoming messages
  }

  n++;
}