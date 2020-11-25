#define TABLESIZE 1024
#define SAMPLEHZ 44100


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
    float envGain;
    
    SinOsc(float freq, float phase, float mul){
      this->freq = freq;
      this->mul = mul;
      pointerInc = TABLESIZE * (freq / SAMPLEHZ);
      pointerVal = map(phase, 0, TWO_PI, 0, TABLESIZE - 1);
      envGain = 1;
    }

    void cycle(){
        pointerInc = TABLESIZE * (freq / SAMPLEHZ);
         if ((pointerVal - floor(pointerVal)) == 0){                //if pointerVal lands on an integer index use it
        outVal = SineValues[(int)pointerVal];
        } else {                               //if pointerVal lands between integer indexes, linearly interpolate the between adjacent samples
        int lower = SineValues[(int)floor(pointerVal)];                        //sample on lower side
        int upper = SineValues[(int)ceil(pointerVal)];                        //sample on higher side
        float rem = (pointerVal - (int)floor(pointerVal));
        outVal = (rem - 0)*(upper - lower) / (1 - 0) + lower;
        if (outVal > 300) outVal = lower;
        }
        outputVal = outputVal + mul * envGain * (outVal - 128);
        
        pointerVal += pointerInc;
        if(pointerVal > TABLESIZE) pointerVal -= TABLESIZE;
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
    }
    destOsc->envGain = currVol;
  }
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

    void cycle(){
//      Serial.print("freq: ");
//      Serial.println(currVal);
      int msTime = millis();
      if (msTime > goalTime){
        valNumber = (valNumber + 1) % valsLen;
        currVal = *(vals + valNumber);
        goalTime = msTime + dur;
//        trig = true;
          destEnv->trig = true;
      }
      destEnv->destOsc->freq = currVal;
    }
};


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
  SinOsc bass(440, 1, 0.6);
  SinOsc arp(554.37, 1, 0.3);

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

    bassSeq.cycle();
    arpSeq.cycle();

    
    //envelope oscillators
    bassEnv.cycle();
    arpEnv.cycle();
//    float bassGain = bassEnv.currVol;
//    bass.mul = bassGain;
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
