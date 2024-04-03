#include <ADC.h>

#define THRESHOLD_GATE 16
#define DEBOUNCE_SAMPLES 2000
#define BIAS_ESTIMATION_SAMPLES 0x10000

#define DETECTION_WINDOW 2000

#define BUFSIZE (DEBOUNCE_SAMPLES + DEBOUNCE_SAMPLES)

#define N_PINS 10

ADC adc;

int val[N_PINS];
int recent_max[N_PINS];
int recent_min[N_PINS];
int n;
int debounce_timer;
int bias_count;
long bias_val[N_PINS];
int cur_bias[N_PINS];
bool calibrating;
int buf[BUFSIZE*N_PINS];
int loc;
bool should_debug_print;

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

  n = 0;
  debounce_timer = 0;
  detection_window_loc = 0;
  passed_gate = 0;
  calibrating = true;
  loc = 0;
  bias_count = 0;
  should_debug_print = true;

  for (int pin = 0 ; pin < N_PINS; pin++) {
    bias_val[pin] = 0;
    cur_bias[pin] = 512;
    recent_min[pin] = 0;
    recent_max[pin] = 0;
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
  int window_end = BUFSIZE + loc;
  int window_start = window_end - DEBOUNCE_SAMPLES - DEBOUNCE_SAMPLES;
  for (int pin = 0 ; pin < N_PINS; pin++) {
    for (int i = window_start; i < window_end; i++) {
      Serial.printf("%d ", buf[i % BUFSIZE + pin*BUFSIZE]);
    }
    Serial.println();
  }
  should_debug_print = false;
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

  bias_count++;
  for (int pin = 0 ; pin < N_PINS; pin++) {
    bias_val[pin] += val[pin];
  }

  if (bias_count == BIAS_ESTIMATION_SAMPLES || (
    calibrating && bias_count == BIAS_ESTIMATION_SAMPLES / 16)) {
    for (int pin = 0 ; pin < N_PINS; pin++) {
      cur_bias[pin] = bias_val[pin] / bias_count;
      bias_val[pin] = 0;
      if (calibrating) {
        Serial.printf("pin %d bias %d\n", pin, cur_bias[pin]);
      }
    }
    bias_count = 0;
    calibrating = false;
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

  if (debounce_timer > 0) {
    debounce_timer--;
    if (debounce_timer == 0) {
      debug_print();
    }
  } else if (passed_gate) {
    detection_window_loc++;

    for (int pin = 0 ; pin < N_PINS; pin++) {
      if (val[pin] < recent_min[pin]) {
        recent_min[pin] = val[pin];
      }
      if (val[pin] > recent_max[pin]) {
        recent_max[pin] = val[pin];
      }
    }

    if (detection_window_loc == DETECTION_WINDOW) {
      int strongest_pin = 0;
      bool is_up = false;
      int strength = 0;
      for (int pin = 0 ; pin < N_PINS; pin++) {
        if (-recent_min[pin] > strength) {
          strength = -recent_min[pin];
          is_up = false;
          strongest_pin = pin;
        }
        if (recent_max[pin] > strength) {
          strength = recent_max[pin];
          is_up = true;
          strongest_pin = pin;
        }
        recent_max[pin] = recent_min[pin] = 0;
      }
      Serial.printf("%d %c %d %ld\n", strongest_pin, is_up ? '<' : '>', strength, micros() - gate_micros);
      passed_gate = false;
      detection_window_loc = 0;
      debounce_timer = DEBOUNCE_SAMPLES;
    }
  } else {
    for (int pin = 0 ; pin < N_PINS; pin++) {
      if (val[pin] > THRESHOLD_GATE || val[pin] < -THRESHOLD_GATE) {
        passed_gate = true;
        gate_micros = micros();
      }
    }
  }

  n++;
}