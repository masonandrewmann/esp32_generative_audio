#define TABLESIZE 1024
#define SAMPLEHZ 44100

#define STATE_IDLE  0
#define STATE_DELAY 1
#define STATE_ATTACK  2
#define STATE_HOLD  3
#define STATE_DECAY 4
#define STATE_SUSTAIN 5
#define STATE_RELEASE 6
#define STATE_FORCED  7

//#include "effect_envelope.h"

int SineValues[TABLESIZE];       // an array to store our values for sine
int sineCounter = 0;

float outputVal = 0;
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
  // It is safe to use digitalRead/Write here if you want to toggle an output
}


void IRAM_ATTR onTimerKr(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphoreKr, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}

class SinOsc {
  //wavetable oscillator with linear interpolation
  //1024 samples played at sample rate of 8000Hz
  private:
    float pointerInc;
    float pointerVal = 0;
    
  public:
    float freq;
    float mul;
    int outVal;
    
    SinOsc(float freq, float phase, float mul){
      this->freq = freq;
      this->mul = mul;
      pointerInc = TABLESIZE * (freq / SAMPLEHZ);
      pointerVal = map(phase, 0, TWO_PI, 0, TABLESIZE - 1);
    }

    void cycle(){
        pointerInc = TABLESIZE * (freq / SAMPLEHZ);
//        println(pointerInc);
         if ((pointerVal - floor(pointerVal)) == 0){                //if pointerVal lands on an integer index use it
        outVal = SineValues[(int)pointerVal];
        } else {                               //if pointerVal lands between integer indexes, linearly interpolate the between adjacent samples
        int lower = SineValues[(int)floor(pointerVal)];                        //sample on lower side
        int upper = SineValues[(int)ceil(pointerVal)];                        //sample on higher side
        float rem = (pointerVal - (int)floor(pointerVal));
        outVal = (rem - 0)*(upper - lower) / (1 - 0) + lower;
        if (outVal > 300) outVal = lower;
        }
        outputVal = outputVal + mul * (outVal - 128);
        
        pointerVal += pointerInc;
        if(pointerVal > TABLESIZE) pointerVal -= TABLESIZE;
    }
};

class Sequencer {
  private:
    int valPointer;
    int goalTime;
  public:
    float *vals;
    float dur;
    float currVal;

    Sequencer(float *vals, float dur){
      this->vals = vals;
//      std::vector<float>
      this->dur = dur;
      currVal = 0;
      valPointer = 0;
      goalTime = 0;
    }

    void cycle(){
      int msTime = millis();
      if (msTime > goalTime){
        currVal = vals[valPointer];
        valPointer = (valPointer + 1) % (sizeof(vals) / sizeof(float));
        goalTime = msTime + dur;
      }
    }
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
  EnvGen( float *levels, float *times, int numStates){
    this->levels = levels;
    this->times = times;
    this->numStates = numStates;
    state = 0;
    startTime = millis();
    nextTime = startTime + *(times + state);
    currVol = 0;
    active = false;
    trig = false;
  }

  void cycle(){
//        Serial.print("Gain: ");
//    Serial.println(currVol);
    if (trig){
      active = true;
      trig = false;
      state = 0;
      startTime = millis();
      nextTime = startTime + *(times + state);
    }
    //determine current state
    if ( active && millis() > nextTime){
      //go to next state
      state++;
      startTime = millis();
      nextTime = startTime + *(times + state);
      if ( state >= numStates - 1){
        active = false;
        currVol = *(levels + state);
      }
    }
    if (active){
      float startLevel = *(levels + state);
      float nextLevel = *(levels + state + 1);
      
//    Serial.print("State: ");
//    Serial.println(state);
//    Serial.print("active?:");
//    Serial.println(active);
//    Serial.print("Gain: ");
//    Serial.println(currVol);
//    Serial.print("startTime: ");
//    Serial.println(startTime);
//    Serial.print("nextTime: ");
//    Serial.println(nextTime);
//    Serial.print("startLevel: ");
//    Serial.println(startLevel);
//    Serial.print("nextLevel: ");
//    Serial.println(nextLevel);
//    Serial.print("currTime: ");
//    Serial.println(millis());
//    Serial.println();
      //calculate output gain
      currVol = (millis() - startTime) * (nextLevel - startLevel) / (nextTime - startTime) + startLevel;
      if (currVol > 1){
        currVol = 1;
      } else if (currVol < 0){
        currVol = 0;
      }

//      currVol = 1;
//      currVol = startLevel + (nextLevel - startLevel) * ( (millis() - startTime) / (nextTime - startTime) );
    }
  }
};

//pattern sequencing

  float arpFreq[] = {440, 554.37, 659.25, 554.37, 440, 587.33, 739.99, 587.33};
  float arpDur = 250;
  int arpPointer = 0;
  int arpTime = 0;

  float bassFreq[] = {220, 293.66};
  float bassDur = 1000;
  int bassPointer = 0;
  int bassTime = 0;

  //tigger timer
  int trigTime = 5000;
  
//make a test oscillator
  SinOsc bass(440, 1, 0.6);
  SinOsc arp(554.37, 1, 0.3);

//envelopes for each oscillator
  float bassLevels[] = {0, 1, 0.5, 0.5, 0};
  float bassTimes[] = {100, 50, 1000, 2000};
  byte bassNumLev = 5;
  EnvGen bassEnv(bassLevels, bassTimes, bassNumLev);

  Sequencer bassSeq(bassFreq, bassDur);
  Sequencer arpSeq(arpFreq, arpDur);


  
void setup()
{
//  Serial.begin(9600);

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

  // calculate sine values
  float RadAngle;                           // Angle in Radians
  for(int MyAngle=0;MyAngle<TABLESIZE;MyAngle++) {
    RadAngle=MyAngle*(2*PI)/TABLESIZE;               // angle converted to radians
    SineValues[MyAngle]=(sin(RadAngle)*127)+128;  // get the sine of this angle and 'shift' to center around the middle of output voltage range
  }

  //trigger test oscillator envelope
  bassEnv.trig = true;
}
 
void loop()
{
    //CONTROL RATE CALCULATIONS
  if (xSemaphoreTake(timerSemaphoreKr, 0) == pdTRUE){
    arpSeq.cycle();
    bassSeq.cycle();

//    arp.freq = arpSeq.currVal;
//    bass.freq = bassSeq.currVal;
    
//    int msTime = millis();
//    //sequence bass
//    if (msTime > bassTime){
//      bass.freq = bassFreq[bassPointer];
//      bassPointer = (bassPointer + 1) % (sizeof(bassFreq) / sizeof(float));
//      bassTime = millis() + bassDur;
//    }
//
//    //sequence arp
//    if (msTime > arpTime){
//      arp.freq = arpFreq[arpPointer];
//      arpPointer = (arpPointer + 1) % (sizeof(arpFreq) / sizeof(float));
//      arpTime = millis() + arpDur;
//    }
    //envelope oscillators
    bassEnv.cycle();
    float bassGain = bassEnv.currVol;
    bass.mul = bassGain;

    if (millis() > trigTime){
      bassEnv.trig = true;
      bass.freq = random(400, 1000);
      trigTime = millis() + 5000;
    }
  }
  
  //AUDIO RATE CALCULATIONS
  if (xSemaphoreTake(timerSemaphoreAr, 0) == pdTRUE){
    //reset output value
    outputVal = 0;


    //cycle oscillators
    bass.cycle();
//    arp.cycle();
  
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
