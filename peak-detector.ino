/*
  Higher sample rate pluck detector
*/

#define THRESHOLD_GATE 16
#define DEBOUNCE_MS 0x2000
#define BIAS_ESTIMATION_SAMPLES 0x100000

// Measured time from pluck to peak as 3437 samples
#define DETECTION_WINDOW 4000

#define BUFSIZE 0x10000

int val;
int val_min;
int val_max;
int recent_max;
int recent_min;
int n;
int debounce_timer;
int bias_count;
int bias_val;
int cur_bias;
bool calibrating;
int buf[BUFSIZE];
int loc;
bool should_debug_print;

bool passed_gate;
int detection_window_loc;

void setup()
{            
  bias_count = 0;
  bias_val = 0;    
  cur_bias = 512;
  val_min = 0;
  val_max = 0;
  n = 0;
  debounce_timer = 0;
  passed_gate = 0;
  calibrating = true;
  loc = 0;
  for (int i = 0; i < BUFSIZE; i++) {
    buf[i] = 0;
  }
  should_debug_print = false;
  Serial.begin(38400);
}

void debug_print() {
  if (!should_debug_print) {
    return;
  }
  int window_end = BUFSIZE + loc;
  int window_start = window_end - DEBOUNCE_MS - DEBOUNCE_MS;
  for (int i = window_start; i < window_end; i++) {
    Serial.printf("%d ", buf[i % BUFSIZE]);
  }
  Serial.println();
  should_debug_print = false;
}

void loop()                     
{
  val = analogRead(A8);
  bias_count++;
  bias_val += val;
  if (bias_count == BIAS_ESTIMATION_SAMPLES || (
    calibrating && bias_count == BIAS_ESTIMATION_SAMPLES / 16)) {
    //int old_bias = cur_bias;
    cur_bias = bias_val / bias_count;
    //Serial.printf("bias %d -> %d (%d over %d)\n", old_bias, cur_bias, bias_val, bias_count);
    bias_val = 0;
    bias_count = 0;
    calibrating = false;
  }
  if (calibrating) {
    return;
  }
  val -= cur_bias;

  buf[loc % BUFSIZE] = val;
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

    if (val < recent_min) {
      recent_min = val;
    }
    if (val > recent_max) {
      recent_max = val;
    }

    if (detection_window_loc == DETECTION_WINDOW) {
      bool is_up = false;
      int strength = -recent_min;
      if (recent_max > strength) {
        is_up = true;
        strength = recent_max;
      }
      Serial.printf("%c %d\n", is_up ? '<' : '>', strength);
      if (is_up) {
        should_debug_print = true;
      }
      passed_gate = false;
      detection_window_loc = recent_max = recent_min = 0;
      debounce_timer = DEBOUNCE_MS;
    }
  } else if (val > THRESHOLD_GATE || val < -THRESHOLD_GATE) {
    passed_gate = true;
    recent_min = 0;
    recent_max = 0;
    detection_window_loc = 0;
  }

  if (val > val_max) {
    val_max = val;
  }
  if (val < val_min) {
    val_min = val;
  }
  //if (n % 20000 == 0) {
  //  Serial.println();
  //}

  //if (n % 100000 == 0) {
  //  Serial.printf("%d %d %d %d %d\n", val, val_min, val_max, bias_val/bias_count, bias_count);
  //}
  n++;
}