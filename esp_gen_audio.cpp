#include "esp_gen_audio.h"
#include <Arduino.h>

float outputVal = 0;

void SinOsc::cycle(){
        pointerInc = TABLESIZE * (freq / SAMPLEHZ);
        if ((pointerVal - floor(pointerVal)) == 0){                //if pointerVal lands on an integer index use it
          outVal = *(table + (int)pointerVal);
        } else {                               //if pointerVal lands between integer indexes, linearly interpolate the between adjacent samples
          int lower = *(table + (int)floor(pointerVal));                        //sample on lower side
          int upper = *(table + (int)ceil(pointerVal));                        //sample on higher side
          float rem = (pointerVal - (int)floor(pointerVal));
          outVal = (rem - 0)*(upper - lower) / (1 - 0) + lower;
          if (outVal > 300) outVal = lower;
        }
        outputVal = outputVal + mul * envGain * (outVal - 128);
        
        pointerVal += pointerInc;
        if(pointerVal > TABLESIZE) pointerVal -= TABLESIZE;
}

void EnvGen::cycle(void){
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

void Sequencer::cycle(void){
  //      Serial.print("freq: ");
  //      Serial.println(currVal);
      int msTime = millis();
      if (msTime > goalTime){
        valNumber = (valNumber + 1) % valsLen;
        currVal = *(vals + valNumber);
        goalTime += dur;
  //        trig = true;
          destEnv->trig = true;
      }
      destEnv->destOsc->freq = currVal;
}
