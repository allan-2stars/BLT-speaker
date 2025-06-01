#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// Touch button for PIN confirmation
const int ValidateButton = 14;
const int ValidateButtonPressed = 40;  // touch limit
// Define analog pin for volume
#define VOLUME_PIN 34  // Change if needed

int lastVolumeLevel = -1; // For tracking changes

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

// Indicator power on or off LED
const int powerLEDPin = 4;

// Button GPIOs
const int pause_play_pin = 12;
const int play_next_pin = 13;
const int play_previous_pin = 32;

// RGB LED pins (common cathode)
const int redPin = 18;
const int greenPin = 19;
const int bluePin = 23;

// Button states
bool buttonState = HIGH;
bool lastPausePlayState = HIGH;
unsigned long lastDebouncePause = 0;
unsigned long lastDebounceNext = 0;
unsigned long lastDebouncePrev = 0;
const unsigned long debounceDelay = 50;
bool lastNextState = HIGH;
bool lastPrevState = HIGH;

// LED control
unsigned long lastBeatTime = 0;
const unsigned long beatInterval = 300; // Simulated beat timing

// Playback state
volatile esp_a2d_audio_state_t audioState = ESP_A2D_AUDIO_STATE_STOPPED;

// Timestamp for last audio state update
unsigned long lastAudioStateChange = 0;

// Digital control volumne
#define VOLUME_UP_PIN 33
#define VOLUME_DOWN_PIN 35
static unsigned long lastButtonTime = 0;
//const unsigned long debounceDelay = 200;

int volumeLevel = 50;  // Start at 50%


// Callback for audio state change
void audio_state_changed(esp_a2d_audio_state_t state, void* ptr) {
  audioState = state;
  lastAudioStateChange = millis();
  Serial.println(a2dp_sink.to_str(state));
}

// Custom audio stream reader
int my_audio_reader(const uint8_t* data, uint32_t length) {
  return i2s.write(data, length);
}

void confirm() {
  a2dp_sink.confirm_pin_code();
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // Optional: ESP32 default is 12-bit (0-4095)

  pinMode(VOLUME_UP_PIN, INPUT_PULLUP);
  pinMode(VOLUME_DOWN_PIN, INPUT_PULLUP);
  a2dp_sink.set_volume(volumeLevel);



  // Set the Power LED pin always on is not ideal
  // it should be like a breathing LED, fadein and fadeout every 3 seconds.
  pinMode(powerLEDPin, OUTPUT);
  digitalWrite(powerLEDPin, HIGH);  // this will make LED always on

  // Configure buttons
  pinMode(pause_play_pin, INPUT_PULLUP);
  pinMode(play_next_pin, INPUT_PULLUP);
  pinMode(play_previous_pin, INPUT_PULLUP);

  // Configure RGB LED pins
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // Configure I2S output
  auto cfg = i2s.defaultConfig();
  cfg.pin_bck = 27;
  cfg.pin_ws = 26;
  cfg.pin_data = 25;
  i2s.begin(cfg);

  // Bluetooth PIN confirmation setup
  a2dp_sink.activate_pin_code(true);

  // Start Bluetooth speaker with secure pairing
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);
  a2dp_sink.start("Berry_Speaker", false);
  Serial.println("Bluetooth ready. Secure pairing with confirmation enabled.");
}

void loop() {
  unsigned long now = millis();
  //updateVolume();  // Call this regularly

  // Confirm PIN if needed and button is touched
  if (a2dp_sink.pin_code() != 0 && touchRead(ValidateButton) < ValidateButtonPressed) {
    a2dp_sink.debounce(confirm, 5000);
  }

  if (digitalRead(VOLUME_DOWN_PIN) == LOW && volumeLevel > 0) {
    volumeLevel -= 5;
    if (volumeLevel < 0) volumeLevel = 0;
    a2dp_sink.set_volume(volumeLevel);
    Serial.printf("Volume Down: %d%%\n", volumeLevel);
    lastButtonTime = millis();
  }
  Serial.printf("Btn UP: %d, Btn DOWN: %d\n", digitalRead(VOLUME_UP_PIN), digitalRead(VOLUME_DOWN_PIN));


  // --- Pause/Play button handling ---
  int reading = digitalRead(pause_play_pin);
  if (reading != lastPausePlayState && (now - lastDebouncePause > debounceDelay)) {
    lastDebouncePause = now;
    if (reading == LOW) {
      Serial.println("Toggle Play/Pause request");
      a2dp_sink.play(); // Always try to play; AVRC handles toggle
    }
  }
  lastPausePlayState = reading;

  // --- Next / Previous track buttons ---
  bool nextState = digitalRead(play_next_pin);
  if (nextState == LOW && lastNextState == HIGH && (now - lastDebounceNext > debounceDelay)) {
    lastDebounceNext = now;
    a2dp_sink.next();
    Serial.println("Next track");
  }
  lastNextState = nextState;

  bool prevState = digitalRead(play_previous_pin);
  if (prevState == LOW && lastPrevState == HIGH && (now - lastDebouncePrev > debounceDelay)) {
    lastDebouncePrev = now;
    a2dp_sink.previous();
    Serial.println("Previous track");
  }
  lastPrevState = prevState;

  // --- RGB LED logic (based on audio state) ---
  if (audioState == ESP_A2D_AUDIO_STATE_STARTED) {
    if (now - lastBeatTime > beatInterval) {
      lastBeatTime = now;

      // Simulate random color change on beat
      int r = 0, g = 0, b = 0;
      switch (random(3)) {
        case 0: r = 255; break;
        case 1: g = 255; break;
        case 2: b = 255; break;
      }

      // Simulate volume-based brightness
      int brightness = random(80, 255); // Simulated volume
      r = r * brightness / 255;
      g = g * brightness / 255;
      b = b * brightness / 255;

      analogWrite(redPin, r);
      analogWrite(greenPin, g);
      analogWrite(bluePin, b);
    }
  } else {
    // Fade out LED over 2 seconds
    static int fadeStep = 0;
    if (fadeStep <= 255) {
      int fadeValue = 255 - fadeStep;
      analogWrite(redPin, fadeValue);
      analogWrite(greenPin, fadeValue);
      analogWrite(bluePin, fadeValue);
      fadeStep += 5; // Adjust fade speed here
      delay(40);     // Total fade time = 255 / 5 * 40ms ≈ 2s
    } else {
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 0);
    }
  }



if (millis() - lastButtonTime > debounceDelay) {
  if (digitalRead(VOLUME_UP_PIN) == LOW && volumeLevel < 100) {
    volumeLevel += 5;
    a2dp_sink.set_volume(volumeLevel);
    Serial.printf("Volume Up: %d%%\n", volumeLevel);
    lastButtonTime = millis();
  }

  if (digitalRead(VOLUME_DOWN_PIN) == LOW && volumeLevel > 0) {
    volumeLevel -= 5;
    a2dp_sink.set_volume(volumeLevel);
    Serial.printf("Volume Down: %d%%\n", volumeLevel);
    lastButtonTime = millis();
    }
  }

}

// use below for physical potentionmeter
// uncomment the function call from loop().
void updateVolume() {
  int raw = analogRead(VOLUME_PIN); // e.g., GPIO34
  int volume = map(raw, 0, 4095, 0, 100);  // Scale to 0–100%
  
  if (volume != lastVolumeLevel) {
    a2dp_sink.set_volume(volume);  // set volume %
    lastVolumeLevel = volume;
    Serial.printf("Volume set to: %d%%\n", volume);
  }
}
