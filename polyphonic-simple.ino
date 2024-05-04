#include <ADC.h>

// Select Tools > USB Type > MIDI + Serial


// In a stready state, how strong does the signal need to be to fire?
#define THRESHOLD_GATE 8

#define DETECTION_WINDOW_MS 50
#define DEBOUNCE_MS 50

#define BIAS_ESTIMATION_SAMPLES 0x10000

#define N_PINS 18

#define BOARD_V3

ADC adc;

int val[N_PINS];
int raw_val[N_PINS];
float recent_rms[N_PINS];
int midi_notes[N_PINS];

long n;
int bias_count;
long bias_val[N_PINS];
int cur_bias[N_PINS];
bool calibrating;
unsigned long start_micros;
float ms_per_sample;
float samples_per_ms;

int debounce_samples;
int detection_window_samples;
int debounce_samples_remaining[N_PINS];
int detection_window_remaining[N_PINS];

void setup() {
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

  calibrating = true;
  bias_count = 0;

  for (int pin = 0; pin < N_PINS; pin++) {
    bias_val[pin] = 0;
    cur_bias[pin] = 512;
    recent_rms[pin] = 0;
    midi_notes[pin] = 60;
  }

  Serial.begin(38400);
  Serial.printf("Starting...\n");
}


int determine_midi_note(int strongest_pin) {
  int midi_out = 0;
  switch (strongest_pin) {
#ifdef BOARD_V3
    case 13:
      midi_out = 36;  // C2
      break;
    case 16:
      midi_out = 38;  // D2
      break;
    case 4:
      midi_out = 40;  // E2
      break;
    case 7:
      midi_out = 42;  // F#2
      break;
    case 11:
      midi_out = 41;  // F2
      break;
    case 15:
      midi_out = 43;  // G2
      break;
    case 2:
      midi_out = 45;  // A2
      break;
    case 5:
      midi_out = 47;  // B2
      break;
    case 9:
      midi_out = 49;  // C#3
      break;
    case 14:
      midi_out = 48;  // C3
      break;
    case 0:
      midi_out = 50;  // D3
      break;
    case 3:
      midi_out = 52;  // E3
      break;
    case 8:
      midi_out = 54;  // F#3
      break;
    case 12:
      midi_out = 53;  // F3
      break;
    case 17:
      midi_out = 55;  // G3
      break;
    case 1:
      midi_out = 57;  // A3
      break;
    case 6:
      midi_out = 59;  // B3
      break;
    case 10:
      midi_out = 60;  // C4
      break;
#else
    case 2:
      midi_out = 31;  // G1
      break;
    case 16:
      midi_out = 36;  // C2
      break;
    case 12:
      midi_out = 41;  // F2
      break;
    case 10:
      midi_out = 43;  // G2
      break;
    case 1:
      midi_out = 45;  // A2
      break;
    case 4:
      midi_out = 47;  // B2
      break;
    case 13:
      midi_out = 48;  // C3
      break;
    case 17:
      midi_out = 50;  // D3
      break;
    case 15:
      midi_out = 52;  // E3
      break;
    case 3:
      midi_out = 53;  // F3
      break;
    case 11:
      midi_out = 55;  // G3
      break;
    case 14:
      midi_out = 57;  // A3
      break;
    case 0:
      midi_out = 60;  // C4
      break;
#endif
  }

  return midi_out + 2;  // pitch it in D
}

void loop() {
  raw_val[0] = adc.analogRead(A0, ADC_0);
  raw_val[1] = adc.analogRead(A1, ADC_0);
  raw_val[2] = adc.analogRead(A2, ADC_0);
  raw_val[3] = adc.analogRead(A3, ADC_0);
  raw_val[4] = adc.analogRead(A4, ADC_0);
  raw_val[5] = adc.analogRead(A5, ADC_0);
  raw_val[6] = adc.analogRead(A6, ADC_0);
  raw_val[7] = adc.analogRead(A7, ADC_0);
  raw_val[8] = adc.analogRead(A8, ADC_0);
  raw_val[9] = adc.analogRead(A9, ADC_0);
  raw_val[10] = adc.analogRead(A10, ADC_0);
  raw_val[11] = adc.analogRead(A11, ADC_0);
  // Pins 26 27, 38, 39 are only on the second ADC https://forum.pjrc.com/index.php?threads/teensy-4-1-adc-channels.72373/
  // All others except 24 and 25 (A10 and A11) can go to either.
  raw_val[12] = adc.analogRead(A12, ADC_1);
  raw_val[13] = adc.analogRead(A13, ADC_1);
  raw_val[14] = adc.analogRead(A14, ADC_1);
  raw_val[15] = adc.analogRead(A15, ADC_1);
  raw_val[16] = adc.analogRead(A16, ADC_0);
  raw_val[17] = adc.analogRead(A17, ADC_0);

  if (calibrating && bias_count == 0) {
    Serial.printf("calibrating...\n");
    start_micros = micros();
  }

  bias_count++;
  for (int pin = 0; pin < N_PINS; pin++) {
    bias_val[pin] += raw_val[pin];
  }

  if (bias_count == BIAS_ESTIMATION_SAMPLES) {
    for (int pin = 0; pin < N_PINS; pin++) {
      cur_bias[pin] = bias_val[pin] / bias_count;
      bias_val[pin] = 0;
      if (calibrating) {
        Serial.printf("pin %d bias %d\n", pin, cur_bias[pin]);
      }
    }

    if (calibrating) {

      unsigned long duration_micros = micros() - start_micros;
      ms_per_sample = (1.0 / 1000) * duration_micros / bias_count;
      samples_per_ms = 1000.0 * bias_count / duration_micros;

      debounce_samples = DEBOUNCE_MS * samples_per_ms;
      detection_window_samples = DETECTION_WINDOW_MS * samples_per_ms;

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

  int bias = 0;
  for (int pin = 0; pin < N_PINS; pin++) {
    bias += raw_val[pin];
  }
  bias /= N_PINS;

  for (int pin = 0; pin < N_PINS; pin++) {
    val[pin] = raw_val[pin] - cur_bias[pin];
  }

  for (int pin = 0; pin < N_PINS; pin++) {
    if (detection_window_remaining[pin] == 0 &&
        debounce_samples_remaining[pin] == 0 &&
        (val[pin] > THRESHOLD_GATE || -val[pin] > THRESHOLD_GATE)) {
      detection_window_remaining[pin] = detection_window_samples;
      recent_rms[pin] = 0;
    }

    if (detection_window_remaining[pin] > 0) {
      recent_rms[pin] += val[pin] * val[pin];
      detection_window_remaining[pin]--;

      if (detection_window_remaining[pin] == 0) {
        int midi_velocity = sqrt(recent_rms[pin] / detection_window_samples);
        if (midi_velocity > 127) {
          midi_velocity = 127;
        }
        int midi_note = determine_midi_note(pin);
        Serial.printf("%d %d %d %.1f\n", pin, midi_note, midi_velocity, recent_rms[pin] / detection_window_samples);
        usbMIDI.sendNoteOn(midi_note, midi_velocity, /*channel=*/1);
        midi_notes[pin] = midi_note;
        debounce_samples_remaining[pin] = debounce_samples;
      }
    } else if (debounce_samples_remaining[pin] > 0) {
      debounce_samples_remaining[pin]--;
      if (debounce_samples_remaining[pin] == 0) {
        usbMIDI.sendNoteOff(midi_notes[pin], 0, /*channel=*/1);
      }
    }
  }

  while (usbMIDI.read()) {
    // ignore incoming messages
  }
}