#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=55,386
AudioFilterBiquad        biquad_high;        //xy=214,404
AudioFilterBiquad        biquad_low;        //xy=215,351
AudioAnalyzeRMS          rms_detector;
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
AudioConnection          patchCord3(i2s1, 0, rms_detector, 0);
AudioConnection          patchCord4(biquad_high, amp3);
AudioConnection          patchCord5(biquad_low, amp1);
AudioConnection          patchCord6(amp1, waveshape1);
AudioConnection          patchCord7(amp3, waveshape2);
AudioConnection          patchCord8(waveshape2, amp4);
AudioConnection          patchCord9(waveshape1, amp2);
AudioConnection          patchCord10(amp2, 0, mixer1, 0);
AudioConnection          patchCord11(amp4, 0, mixer1, 1);
AudioConnection          patchCord12(mixer1, 0, i2s2, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=159,497
// GUItool: end automatically generated code

// Define pin for 'Drive' potentiometer (adjusts base distortion level)
const int digiPot = A0; 

// Define size of waveshaper array
float soft_clip_table[513]; 

// Global Variables
float windowed_rms = 0.0; // moving average for rms
float last_windowed_rms = 0.0;
float alpha_bass = 0.05; // creates a ~58ms smoothing window??

// Default gain parameters, these are overwritten in the loop
float low_drive = 3.0;
float low_cut = 0.17;
float base_drive = 5.0; // minimum gain for high-end band
float high_drive = base_drive; // default to base_drive

const float high_cut = 0.1; 

// For dynamically adjusting high_drive according to tempo and average amplitude ('smart' functionality)
float max_drive = 18.0;  // maximum pre-waveshaper gain to keep some tone, default to 18 but later set to base_drive + smart_range
float smart_range = 10.0; // max_drive - base_drive
float rms_sensitivity = 25.0; // multiplier to scale the RMS into drive

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
  float crossFreq = 250.0;
  float q = 0.7071;

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

  max_drive = base_drive + smart_range;

  if (rms_detector.available()) {
  
    // Get raw RMS block reading (triggers every 128 samples = ~2.9 ms)
    float raw_rms = rms_detector.read(); 
    
    // Use an Exponential Moving Average (EMA) to find the average amplitude
    // EMA: y[n] = a*x[n] + (a-1)*y[n-1]
    // Larger alpha places more weight on current input, smaller alpha places more weight on past history
    // Use assymetric EMA - larger alpha for quickly reflecting presence of attack, smaller alpha for slower gain release during decay 
    
    // Fast attack for detecting spike 
    float attack_alpha = 0.5;  
    
    // Slow release for smoothing out AC ripple so it is not detected as a series of spikes
    float release_alpha = 0.01; 

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
    float amp_drive = base_drive + dynamic_intensity;

    //float derivative = windowed_rms - last_windowed_rms;
        

    //last_windowed_rms = windowed_rms;

    // Clamp to max
    if (amp_drive > max_drive) {
        amp_drive = max_drive;
    }

    // Apply to the pre-distortion gain for the high frequency band to change distortion texture
    amp3.gain(amp_drive);

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
}