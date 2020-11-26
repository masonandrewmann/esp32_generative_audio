#include "esp_gen_audio.h"
#include <Arduino.h>

float outputVal = 0;

// hardware timer stuff from ESP32 RepeatTimer example 

hw_timer_t * timerAr = NULL;
hw_timer_t * timerKr = NULL;

volatile SemaphoreHandle_t timerSemaphoreAr;
volatile SemaphoreHandle_t timerSemaphoreKr;

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t isrCounter = 0;
volatile uint32_t lastIsrAt = 0;

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

float eqTempHz(float referenceF, float chromInt){
  float noteF = pow(2, chromInt/12) * referenceF;
  return noteF;
}
