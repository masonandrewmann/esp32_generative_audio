#include "esp_gen_audio.h"

int SineValues[TABLESIZE];       // an array to store our values for sine
//int sineCounter = 0;

//pattern sequencing

  float arpFreq[] = {440, 554.37, 659.25, 554.37, 440, 587.33, 739.99, 587.33};
  float arpDur = 500;
 
  float bassFreq[] = {220, 293.66};
  float bassDur = 1000;
  

//envelopes for each oscillator
  float bassLevels[] = {0, 1, 0.5, 0.5, 0};
  float bassTimes[] = {5, 50, 500, 300};

  float arpLevels[] = {0, 1, 0.5, 0.5, 0};
  float arpTimes[] = {5, 50, 100, 200};
  
  //make oscillators
  SinOsc bass(440, 1, 0.6, SineValues);
  SinOsc arp(554.37, 1, 0.3, SineValues);

  //make envelopes
  EnvGen bassEnv(bassLevels, bassTimes, sizeof(bassLevels)/sizeof(float), &bass);
  EnvGen arpEnv(arpLevels, arpTimes, sizeof(arpLevels)/sizeof(float), &arp);

  //make sequencers
  Sequencer bassSeq(bassFreq, bassDur, sizeof(bassFreq)/sizeof(float), &bassEnv);
  Sequencer arpSeq(arpFreq, arpDur, sizeof(arpFreq)/sizeof(float), &arpEnv);


  
void setup()
{
//  Serial.begin(9600);

  timerSetup(); //set up audio and control rate clocks
  
  // calculate sine wavetable
  float RadAngle;                           // Angle in Radians
  for(int MyAngle=0;MyAngle<TABLESIZE;MyAngle++) {
    RadAngle=MyAngle*(2*PI)/TABLESIZE;               // angle converted to radians
    SineValues[MyAngle]=(sin(RadAngle)*127)+128;  // get the sine of this angle and 'shift' to center around the middle of output voltage range
  }
}
 
void loop()
{
    //CONTROL RATE CALCULATIONS
  if (xSemaphoreTake(timerSemaphoreKr, 0) == pdTRUE){

    //cycle sequencers
    bassSeq.cycle();
    arpSeq.cycle();
    
    //cycle envelopes
    bassEnv.cycle();
    arpEnv.cycle();
  }
  
  //AUDIO RATE CALCULATIONS
  if (xSemaphoreTake(timerSemaphoreAr, 0) == pdTRUE){
    //reset output value
    outputVal = 0;

    //cycle oscillators
    bass.cycle();
    arp.cycle();
  
    //write output to pin
    writeOutput(26, outputVal);
  }

}
