#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=55,386
AudioAmplifier           amp1;           //xy=174,380
AudioEffectWaveshaper    waveshape1;     //xy=320,381
AudioOutputI2S           i2s2;           //xy=485,384
AudioConnection          patchCord1(i2s1, 0, amp1, 0);
AudioConnection          patchCord2(amp1, waveshape1);
AudioConnection          patchCord3(waveshape1, 0, i2s2, 0);
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

  // Teensy Audio Library's built-in waveshape function: maps input to clipped output
  waveshape1.shape(soft_clip_table, 513); // cubic soft-clipping

  //waveshape1.shape(arctan_table, 33); // arctangent soft-clipping

  //waveshape1.shape(sin_table, 33);  // sine soft-clipping

  amp1.gain(5.0); 

}

void loop() {
  // put your main code here, to run repeatedly:

}