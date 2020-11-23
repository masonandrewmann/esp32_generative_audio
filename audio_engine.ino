#define TABLESIZE 1024
#define SAMPLEHZ 8000

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

//make a test oscillator
  SinOsc myOsc(220, 1, 0.6);
  SinOsc myOsc2(554.37, 1, 0.3);

//  SinOscs
  
void setup()
{
  Serial.begin(115200);

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
}
 
void loop()
{
  //AUDIO RATE CALCULATIONS
  if (xSemaphoreTake(timerSemaphoreAr, 0) == pdTRUE){
    //reset output value
    outputVal = 0;
  
    //cycle oscillators
    myOsc.cycle();
    myOsc2.cycle();
  
    //write output to pin
    outputVal += 128;
    if (outputVal < 0) {
      outputVal = 0;
    } else if (outputVal > 255) outputVal = 255;
    dacWrite(26, outputVal);
  }

  //CONTROL RATE CALCULATIONS
  if (xSemaphoreTake(timerSemaphoreKr, 0) == pdTRUE){
//    Serial.println("hiiiii");
  int temptime = millis();
  
  //sequence frequencies
  if ((temptime % 2000) > 1000) {
    myOsc.freq = 220;
  } else {
    myOsc.freq = 293.66;
  }

  if ((temptime % 2000) < 250) {
    myOsc2.freq = 440;
  } else if ((temptime % 2000) < 500){
    myOsc2.freq = 554.37;
  } else if ((temptime % 2000) < 750){
    myOsc2.freq = 659.25;
  } else if ((temptime % 2000) < 1000){
    myOsc2.freq = 554.37;
  } else if ((temptime % 2000) < 1250){
    myOsc2.freq = 440;
  } else if ((temptime % 2000) < 1500){
    myOsc2.freq = 587.33;
  } else if ((temptime % 2000) < 1750){
    myOsc2.freq = 739.99;
  } else {
    myOsc2.freq = 587.33;
  }
  }
  
  
}
