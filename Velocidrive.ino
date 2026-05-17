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


// Define size of waveshaper array
float soft_clip_table[513]; 

// Global Variables
float windowed_rms = 0.0; // moving average for rms
float last_windowed_rms = 0.0;
float alpha_bass = 0.05; // creates a ~58ms smoothing window??

float base_drive = 2.0;  // later: dynamic parameter set by potentiometer  
float max_drive = 15.0;  // maximum pre-waveshaper gain to keep some tone
float rms_sensitivity = 25.0; // multiplier to scale the RMS into drive

void setup() {
  AudioMemory(12);  // allocate memory

  // Configure the Audio Shield
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_1.unmuteLineout();
  sgtl5000_1.lineOutLevel(29);  // default
  
  // Generate a Sigmoid/Soft-clipping curve
  for (int i=0; i<513; i++) {
    float x = (float)(i - 256) / 256.0; // map index to -1.0 to 1.0
    // Simple cubic soft-clipper: 1.5*x - 0.5*x^3
    soft_clip_table[i] = 1.5f * x - 0.5f * (x * x * x);
  }

  // --- Setup Linkwitz-Riley 4th Order Crossover ---
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


  // Fix magic numbers later
  // Static low-end distortion for oomph, 'smart' high end distortion
  amp1.gain(2.5); 
  amp2.gain(0.2);
  amp3.gain(base_drive); 
  amp4.gain(0.1);

  Serial.begin(115200);
}

void loop(){
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

    last_windowed_rms = windowed_rms;

    // Clamp to max
    if (amp_drive > max_drive) {
        amp_drive = max_drive;
    }

    // Apply to the pre-distortion gain for the high frequency band to change distortion texture
    amp3.gain(amp_drive);

    // --- Serial Outputs for the Plotter ---
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