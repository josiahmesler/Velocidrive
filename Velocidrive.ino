#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=55,386
AudioFilterBiquad        biquad_high;        //xy=214,404
AudioFilterBiquad        biquad_low;        //xy=215,351
AudioAnalyzeRMS          rms_detector_amp;
AudioAnalyzeRMS          rms_detector_tempo;
AudioAmplifier           amp1;           //xy=380,350
AudioAmplifier           amp3; //xy=380,411
AudioEffectWaveshaper    waveshape2;     //xy=527,409
AudioEffectWaveshaper    waveshape1;     //xy=530,351
AudioAmplifier           amp2;           //xy=681,350
AudioAmplifier           amp4; //xy=681,411
AudioMixer4              mixer1;         //xy=814,397
AudioOutputI2S           i2s2;           //xy=958,388
AudioConnection          patchCord1(i2s1, 0, biquad_low, 0);
AudioConnection          patchCord2(i2s1, 0, biquad_high, 0);
AudioConnection          patchCord3(i2s1, 0, rms_detector_amp, 0);
AudioConnection          patchCord4(biquad_high, amp3);
AudioConnection          patchCord5(biquad_low, amp1);
AudioConnection          patchCord6(amp1, waveshape1);
AudioConnection          patchCord7(amp3, waveshape2);
AudioConnection          patchCord8(waveshape2, amp4);
AudioConnection          patchCord9(waveshape1, amp2);
AudioConnection          patchCord10(amp2, 0, mixer1, 0);
AudioConnection          patchCord11(amp4, 0, mixer1, 1);
AudioConnection          patchCord12(mixer1, 0, i2s2, 0);
AudioConnection          patchCord13(biquad_high, 0, rms_detector_tempo, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=159,497
// GUItool: end automatically generated code

// Define pin for 'Drive' potentiometer (adjusts base distortion level)
const int digiPot = A0; 

// Define size of waveshaper array
float soft_clip_table[513]; 

// Global Variables

// Default gain parameters, these are overwritten in the loop
float low_drive = 3.0;
float low_cut = 0.17;
float base_drive = 5.0; // minimum gain for high-end band
float high_drive = base_drive; // default to base_drive

const float high_cut = 0.1; 

float windowed_rms = 0.0f; // moving average for RMS for amplitude tracker
float last_windowed_rms = 0.0f;
float fast_envelope = 0.0f; // moving average for RMS for tempo tracker
float last_envelope = 0.0f;
int count = 10;
float amp_drive = 0.0f;
float tempo_drive = 0.0f;

// 2-Second Event Density Tracker
const int WINDOW_SIZE = 689; // 2s * 1000 / ~2.9ms (689 RMS values read per 2 second period)
byte strike_window[WINDOW_SIZE] = {0}; // Array of 0s and 1s, initialized to all 0s
int window_index = 0;
int current_strikes = 0; // The running total of notes in the last 2 seconds

// Tweakable parameters
const float attack_alpha = 0.5f;  // Fast attack for detecting spike 
const float release_alpha = 0.005f; // Slow release for smoothing out AC ripple so it is not detected as a series of spikes
const float fast_attack_alpha = 0.5f;
const float fast_release_alpha = 0.085f; // Slightly faster release to quickly catch new attack
float smart_range = 10.0f; // max_drive - base_drive
float rms_sensitivity = 15.0f; // multiplier to scale the RMS into drive
const int lockout_max = 35; // 35 * ~2.9ms = minimum 101.5ms in between spike detections
const float noise_floor = 0.001f;  // ensures no noise-elicited spikes when not playing
const float tempo_sensitivity = 0.25f; // to count as new attack, must be larger than percentage of previous rms

void setup() {
  AudioMemory(12);  // allocate memory

  // Configure the Audio Shield
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_1.lineInLevel(15); // max for maximal clipping
  sgtl5000_1.unmuteLineout();
  sgtl5000_1.lineOutLevel(29);  // default
  
  // Generate a Sigmoid/Soft-clipping curve
  for (int i=0; i<513; i++) {
    float x = (float)(i - 256) / 256.0; // map index to -1.0 to 1.0
    // Simple cubic soft-clipper: 1.5*x - 0.5*x^3
    soft_clip_table[i] = 1.5f * x - 0.5f * (x * x * x);
  }

  // Set up Linkwitz-Riley 4th Order Crossover
  float crossFreq = 250.0f;
  float q = 0.7071f;

  // Cascade two Butterworth lowpass stages to make an LR4 lowpass
  biquad_low.setLowpass(0, crossFreq, q);
  biquad_low.setLowpass(1, crossFreq, q);

  // Cascade two Butterworth highpass stages to make an LR4 highpass
  biquad_high.setHighpass(0, crossFreq, q);
  biquad_high.setHighpass(1, crossFreq, q);

  // Teensy Audio Library's built-in waveshape function: maps input to clipped output
  waveshape1.shape(soft_clip_table, 513); // distort low frequency band
  waveshape2.shape(soft_clip_table, 513); // distort high frequency band


  // Initialized amplifiers. All are dynamic except high_cut (static 0.1)
  // _drive variables boost signal amplitude (>1)
  // _cut variables attenuate signal amplidute (<1)
  amp1.gain(low_drive); 
  amp2.gain(low_cut);
  amp3.gain(high_drive); 
  amp4.gain(high_cut);

  Serial.begin(115200);
}

void loop(){
  // Set gain according to distortion control potentiometer reading
  int potValue = analogRead(digiPot);

  // Map 'Drive' pot value to minimum high-end drive in range [0,10]
  base_drive = map(potValue, 0, 1023, 0, 10);

  // Also map to low-end digital gain 
  low_drive = map(potValue, 0, 1023, 1, 6);

  // Map low-end drive to low-end attenuation factor (low_cut) to keep output voltage level constant for any potValue
  // This way the 'Drive' pot changes distortion (more drive means more clipping) but not gain
  low_cut = map(low_drive, 1, 6, 0.5, 0.25);

  // Dynamically set low-end drive only according to 'Drive' pot (not 'smart')
  amp1.gain(low_drive); 
  amp2.gain(low_cut);

  float max_drive = base_drive + smart_range;

  // RMS Amplitude Tracking
  if (rms_detector_amp.available()) {
  
    // Get raw RMS block reading (triggers every 128 samples = ~2.9 ms)
    float raw_rms = rms_detector_amp.read(); 
    
    // Use an Exponential Moving Average (EMA) to find the average amplitude
    // EMA: y[n] = a*x[n] + (a-1)*y[n-1]
    // Larger alpha places more weight on current input, smaller alpha places more weight on past history
    // Use asymmetric EMA - larger alpha for quickly reflecting presence of attack, smaller alpha for slower gain release during decay 

    // If the raw signal is shooting up, use the fast attack math
    if (raw_rms > windowed_rms) {
        windowed_rms = (raw_rms * attack_alpha) + (windowed_rms * (1.0 - attack_alpha));
    } 
    // If the raw signal is decreasing relative to the average, use the slow release math
    else {
        windowed_rms = (raw_rms * release_alpha) + (windowed_rms * (1.0 - release_alpha));
    }
    
    // Square the windowed rms to make the gain variance more audible to the human ear
    float shaped_rms = windowed_rms * windowed_rms;
    
    // Scale by sensitivity
    float dynamic_intensity = shaped_rms * rms_sensitivity;
    
    // Set the amp_drive by adding the dynamic_intensity to the base distortion level
    amp_drive = base_drive + dynamic_intensity;

    // Serial outputs for the serial plotter
    Serial.print("Raw_RMS:");
    Serial.print(raw_rms * 10.0); // Scaled up so we can see it next to the drive
    Serial.print(",");
    
    Serial.print("10ms_Windowed_RMS:");
    Serial.print(windowed_rms * 10.0); 
    Serial.print(",");
    
    Serial.print("Drive_Output:");
    Serial.println(amp_drive);
  }

  // Note Density Detection
  // Finds RMS of high-passed signal instead of raw signal to mitigate ripple
  if (rms_detector_tempo.available()) {
    float raw_rms = rms_detector_tempo.read(); 

    // Same EMA logic as above 
    if (raw_rms > fast_envelope) {
      fast_envelope = (raw_rms * fast_attack_alpha) + (fast_envelope * (1.0 - fast_attack_alpha));   
    }
    else {
      fast_envelope = (raw_rms * fast_release_alpha) + (fast_envelope * (1.0 - fast_release_alpha));   
    }

    float derivative = fast_envelope - last_envelope; // change in RMS

    float dynamic_threshold = tempo_sensitivity * last_envelope;
    last_envelope = fast_envelope;

    byte is_spike = 0;

    if (derivative > dynamic_threshold + noise_floor && count == 0) {
      count = lockout_max; // reduces false positives caused by ripple via lockout timer (debouncing)
      is_spike = 1;
    }

    // Decrement lockout timer
    if (count > 0) {
      count--;
    }

    // Running sum of notes played in ~2ms window
    // 1. Subtract the oldest historical record from our running total
    current_strikes = current_strikes - strike_window[window_index]; 
    
    // 2. Overwrite that old record with new data (0 or 1)
    strike_window[window_index] = is_spike; 
    
    // 3. Add the new data to running total
    current_strikes = current_strikes + is_spike; 
    
    // 4. Advance the index, and loop back to 0 if we hit 689
    window_index++;
    if (window_index >= WINDOW_SIZE) {
      window_index = 0;
    }

    // Cap the strikes at 15 so tremolo picking doesn't blow out the math
    int capped_strikes = current_strikes;
    if (capped_strikes > 15) {
      capped_strikes = 15;
    }

    // Map 0-15 strikes to range (0.0 to 30.0) plus the base_drive
    tempo_drive = ((float)capped_strikes * 0.2f) * smart_range + base_drive; 

    // Serial outputs
    Serial.print("Notes_in_Last_2_Sec: ");
    Serial.print(current_strikes);
    Serial.print(", Tempo_Drive_Added: ");
    Serial.println(tempo_drive);

  }
  // Limit amplitude-driven drive (high amplitude means gain is already high, don't need to add too much more)
  if (amp_drive > max_drive) {
    amp_drive = max_drive;
  }

  // 'Smart' drive: increased when loud OR fast playing
  float smart_drive = max(amp_drive, tempo_drive);
  
  // Apply to the pre-distortion gain for the high frequency band to change distortion texture
  amp3.gain(smart_drive);
}