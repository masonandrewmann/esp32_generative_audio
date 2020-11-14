#define tableSize 1024
#define sampleRate 8000

int SineValues[tableSize];       // an array to store our values for sine
int sineCounter = 0;

void setup()
{
//  Serial.begin(9600);
  // calculate sine values
  float RadAngle;                           // Angle in Radians
  for(int MyAngle=0;MyAngle<tableSize;MyAngle++) {
    RadAngle=MyAngle*(2*PI)/tableSize;               // angle converted to radians
    SineValues[MyAngle]=(sin(RadAngle)*127)+128;  // get the sine of this angle and 'shift' to center around the middle of output voltage range
  }
}
 
void loop()
{
  playSine(100, 1000);
//    dacWrite(26, SineValues[sineCounter]);
//    sineCounter++;
//    if (sineCounter > tableSize) sineCounter -= tableSize;
//    delayMicroseconds(125);
  playSine(300, 2000);
}

void playSine(float freq, float len){
  float pointerInc = 1024 * (freq / 8000); //set pointer to increment to generate desired frequency S = N * (f/Fs) 
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
    if(pointerVal > 1024) pointerVal -= 1024;
    delayMicroseconds(125);
  }
}
