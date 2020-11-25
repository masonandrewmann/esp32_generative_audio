#define TABLESIZE 1024
#define SAMPLEHZ 44100

#include <Arduino.h>

#ifndef esp_gen_audio_h_
#define esp_gen_audio_h_



/************************************
 * For generative audio on the ESP-32
 *               :~)
 */

float outputVal = 0;

 
class SinOsc {
  //wavetable oscillator with linear interpolation
  //1024 samples played at sample rate of 8000Hz
  private:
    float pointerInc;
    float pointerVal = 0;
    int* table; 
    
  public:
    float freq;
    float mul;
    int outVal;
    float envGain;
    
    SinOsc(float freq, float phase, float mul, int* table){
      this->freq = freq;
      this->mul = mul;
      this->table = table;
      pointerInc = TABLESIZE * (freq / SAMPLEHZ);
      pointerVal = map(phase, 0, TWO_PI, 0, TABLESIZE - 1);
      envGain = 1;
    }
    
    void cycle();
};

class EnvGen {
  // linear interpolating envelope generator of any length
  // series of volume levels (between 0 and 1), length  of time between them (in milliseconds), and number of levels
  private:
    int numStates;
    int state;
    float startTime;
    float nextTime;
  public:
    float *levels;
    float *times;
    float currVol;
    boolean active;
    boolean trig;
    SinOsc* destOsc;
  EnvGen( float *levels, float *times, int numStates, SinOsc* destOsc){
    this->levels = levels;
    this->times = times;
    this->numStates = numStates;
    this->destOsc = destOsc;
    state = 0;
    startTime = millis();
    nextTime = startTime + *(times + state);
    currVol = 0;
    active = false;
    trig = false;
  }

  void cycle();
};

class Sequencer {
  private:
    int valNumber;
    int valsLen;
    int goalTime;
    EnvGen* destEnv;
  public:
    float *vals;
    float dur;
    float currVal;
    boolean trig;

    Sequencer(float *vals, float dur, int valsLen, EnvGen* destEnv){
      this->vals = vals;
      this->dur = dur;
      this->valsLen = valsLen;
      this->destEnv = destEnv;
      currVal = *vals;
      valNumber = 0;
      goalTime = 0;
//      trig = true;
        destEnv->trig = true;
    }

    void cycle();
};

#endif
