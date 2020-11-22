#define tableSize 1024
#define sampleHz 8000

int SineValues[tableSize];       // an array to store our values for sine
int sineCounter = 0;

float outputVal = 0;
int usTime = 0;

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
    
    SinOsc(float freq, float mul){
      this->freq = freq;
      this->mul = mul;
      pointerInc = 1024 * (freq / 8000);
    }

    void calc(){
      if ((pointerVal - floor(pointerVal)) == 0){                //if pointerVal lands on an integer index use it
      outVal = SineValues[(int)pointerVal];
      } else {                               //if pointerVal lands between integer indexes, linearly interpolate the between adjacent samples
      int lower = SineValues[(int)floor(pointerVal)];                        //sample on lower side
      int upper = SineValues[(int)ceil(pointerVal)];                        //sample on higher side
      float rem = (pointerVal - floor(pointerVal));
      outVal = lower + (rem) * (upper - lower);
      }
//      dacWrite(26, outVal * mul);
      outputVal = outputVal + mul * (outVal - 128);
      
      pointerVal += pointerInc;
      if(pointerVal > tableSize) pointerVal -= tableSize;
    }

    void cycle(){
        pointerInc = 1024 * (freq / 8000);
        calc();
    }
};

//make a test oscillator
  SinOsc myOsc(220, 0.3);
  SinOsc myOsc2(277.18, 0.2);
  
void setup()
{
  // calculate sine values
  float RadAngle;                           // Angle in Radians
  for(int MyAngle=0;MyAngle<tableSize;MyAngle++) {
    RadAngle=MyAngle*(2*PI)/tableSize;               // angle converted to radians
    SineValues[MyAngle]=(sin(RadAngle)*127)+128;  // get the sine of this angle and 'shift' to center around the middle of output voltage range
  }
}
 
void loop()
{
  if ( micros() > usTime){
    //reset output value
    outputVal = 0;
  
    //cycle oscillators
    myOsc.cycle();
    myOsc2.cycle();
  
    //write output to pin
    outputVal += 128;
    if (outputVal < 0) outputVal = 0;
    else if (outputVal > 255) outputVal = 255;
    dacWrite(26, outputVal);

    
    usTime = micros() + 125;
  }


  //sequence frequencies
  if ((millis() % 2000) > 1000) {
    myOsc.freq = 220;
  } else {
    myOsc.freq = 293.66;
  }

  if ((millis() % 2000) < 250) {
    myOsc2.freq = 440;
  } else if ((millis() % 2000) < 500){
    myOsc2.freq = 554.37;
  } else if ((millis() % 2000) < 750){
    myOsc2.freq = 659.25;
  } else if ((millis() % 2000) < 1000){
    myOsc2.freq = 554.37;
  } else if ((millis() % 2000) < 1250){
    myOsc2.freq = 440;
  } else if ((millis() % 2000) < 1500){
    myOsc2.freq = 587.33;
  } else if ((millis() % 2000) < 1750){
    myOsc2.freq = 739.99;
  } else {
    myOsc2.freq = 587.33;
  }
  
}

void playSine(float freq, float len){
  float pointerInc = tableSize * (freq / sampleHz); //set pointer to increment to generate desired frequency S = N * (f/Fs) 
  float pointerVal = 0;
  int goalTime = millis() + len;           //length of tone to be played
  while (millis() < goalTime){             //check if length has been reached
    int outVal;               
    if ((pointerVal - floor(pointerVal)) == 0){                //if pointerVal lands on an integer index use it
      outVal = SineValues[(int)pointerVal];
    } else {                               //if pointerVal lands between integer indexes, linearly interpolate the between adjacent samples
      int lower = SineValues[(int)floor(pointerVal)];                        //sample on lower side
      int upper = SineValues[(int)ceil(pointerVal)];                        //sample on higher side
      float rem = (pointerVal - floor(pointerVal));
      outVal = lower + (rem) * (upper - lower);
    }
    dacWrite(26, SineValues[(int)pointerVal]);
    pointerVal += pointerInc;
    if(pointerVal > tableSize) pointerVal -= tableSize;
    delayMicroseconds(125);
  }
}
