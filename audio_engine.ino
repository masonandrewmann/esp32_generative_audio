int SineValues[256];  

void setup() {
  generateSineWavetable();

}

void loop() {
  // put your main code here, to run repeatedly:

}

void generateSineWavetable(){
  float ConversionFactor = (2 * PI) / 256;
  float RadAngle;

  for(int MyAngle=0;MyAngle<256;MyAngle+=) {
  RadAngle=MyAngle*ConversionFactor;               // 8 bit angle converted to radians
  SineValues[MyAngle]=(sin(RadAngle)*127)+128;  // get the sine of this angle and 'shift' up so
                                            // there are no negative values in the data
                                            // as the DAC does not understand them and would
                                            // convert to positive values.
  }
}
