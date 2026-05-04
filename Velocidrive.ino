#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=55,386
AudioFilterBiquad        biquad_high;        //xy=214,404
AudioFilterBiquad        biquad_low;        //xy=215,351
AudioAmplifier           amp1;           //xy=380,350
AudioAmplifier           amp3; //xy=380,411
AudioEffectWaveshaper    waveshape_low;     //xy=536,354
AudioEffectWaveshaper    waveshape_high;     //xy=542,411
AudioAmplifier           amp2;           //xy=681,350
AudioAmplifier           amp4; //xy=681,411
AudioMixer4              mixer1;         //xy=814,397
AudioOutputI2S           i2s2;           //xy=958,388
AudioConnection          patchCord1(i2s1, 0, biquad_low, 0);
AudioConnection          patchCord2(i2s1, 0, biquad_high, 0);
AudioConnection          patchCord3(biquad_high, amp3);
AudioConnection          patchCord4(biquad_low, amp1);
AudioConnection          patchCord5(amp1, waveshape_low);
AudioConnection          patchCord6(amp3, waveshape_high);
AudioConnection          patchCord7(waveshape_low, amp2);
AudioConnection          patchCord8(waveshape_high, amp4);
AudioConnection          patchCord9(amp2, 0, mixer1, 0);
AudioConnection          patchCord10(amp4, 0, mixer1, 1);
AudioConnection          patchCord11(mixer1, 0, i2s2, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=159,497
// GUItool: end automatically generated code




float soft_clip_table[513]; 

// 33-point Arctangent Soft Clipper (Drive = 4.0) - copy/pasted from Google Gemini
float arctan_table[33] = {
    -1.0000f, -0.9850f, -0.9660f, -0.9420f, -0.9120f, -0.8750f, -0.8350f, -0.7850f,
    -0.7250f, -0.6600f, -0.5920f, -0.5150f, -0.4350f, -0.3500f, -0.2700f, -0.1850f,
     0.0000f,
     0.1850f,  0.2700f,  0.3500f,  0.4350f,  0.5150f,  0.5920f,  0.6600f,  0.7250f,
     0.7850f,  0.8350f,  0.8750f,  0.9120f,  0.9420f,  0.9660f,  0.9850f,  1.0000f
};

// 33-point Sine Wavefolder (Multiplier = 1.5 * Pi) - copy/pasted from Google Gemini
float sin_table[33] = {
     1.0000f,  0.9569f,  0.8315f,  0.6344f,  0.3827f,  0.0980f, -0.1951f, -0.4714f,
    -0.7071f, -0.8819f, -0.9808f, -0.9952f, -0.9239f, -0.7730f, -0.5556f, -0.2903f,
     0.0000f,
     0.2903f,  0.5556f,  0.7730f,  0.9239f,  0.9952f,  0.9808f,  0.8819f,  0.7071f,
     0.4714f,  0.1951f, -0.0980f, -0.3827f, -0.6344f, -0.8315f, -0.9569f, -1.0000f
};

void setup() {
  // put your setup code here, to run once:
  AudioMemory(12);  // alocate memory

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
  waveshape_low.shape(soft_clip_table, 513); // cubic soft-clipping
  waveshape_high.shape(soft_clip_table, 513); // cubic soft-clipping

  

  //waveshape1.shape(arctan_table, 33); // arctangent soft-clipping

  //waveshape1.shape(sin_table, 33);  // sine soft-clipping

  amp1.gain(5.0); 
  amp2.gain(0.05);
  amp3.gain(5.0); 
  amp4.gain(0.05);

}

void loop() {
  // put your main code here, to run repeatedly:

}
