#include "esp_gen_audio.h"

int SineValues[TABLESIZE];       // an array to store our values for sine
int sineCounter = 0;

float usInc = 6;

// hardware timer stuff from ESP32 RepeatTimer example 

hw_timer_t * timerAr = NULL;
hw_timer_t * timerKr = NULL;

volatile SemaphoreHandle_t timerSemaphoreAr;
volatile SemaphoreHandle_t timerSemaphoreKr;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

void IRAM_ATTR onTimerAr(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphoreAr, NULL);
}


void IRAM_ATTR onTimerKr(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphoreKr, NULL);
}

//pattern sequencing

  float arpFreq[] = {440, 554.37, 659.25, 554.37, 440, 587.33, 739.99, 587.33};
  float arpDur = 250;
 
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

void writeOutput(byte pin, float outputValue){
    outputValue += 128;
    if (outputValue < 0) {
      outputValue = 0;
    } else if (outputValue > 255) outputValue = 255;
    dacWrite(pin, outputValue);
}

void timerSetup(){
  // Create semaphore to inform us when the timer has fired
  timerSemaphoreAr = xSemaphoreCreateBinary();
  timerSemaphoreKr = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timerAr = timerBegin(0, 80, true);
  timerKr = timerBegin(1, 80, true);

  // Attach onTimerAr function to our timer.
  timerAttachInterrupt(timerAr, &onTimerAr, true);
  timerAttachInterrupt(timerKr, &onTimerKr, true);

  // Set alarm to call onTimerAr function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timerAr, (1.0 / SAMPLEHZ) * pow(10, 6), true); //poll at sample rate
  timerAlarmWrite(timerKr, (1.0 / 100) * pow(10, 6), true);      // 125Hz control rate

  // Start an alarm
  timerAlarmEnable(timerAr);
  timerAlarmEnable(timerKr);
}
