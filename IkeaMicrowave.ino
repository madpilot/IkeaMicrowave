#include <avr/sleep.h>

#include <TM1637Display.h>
#include <ClickEncoder.h>
#include <TimerOne.h>

#define SLEEP_INTERRUPT_1 2
#define SLEEP_INTERRUPT_2 3
#define SPEAKER 5
#define LIGHT 6
#define ENCODER_BUTTON 2
#define ENCODER_A 3
#define ENCODER_B 4

#define MAX_SECONDS 10 * 60
#define SLEEP_TIMEOUT 60 * 1000;

#define CLK 12
#define DIO 11

ClickEncoder *encoder;
TM1637Display display(CLK, DIO);

uint8_t data[] = {0, 0, 0, 0};
int16_t last, left, milli, sleepTimer;
bool running, showEnd, lightOn;

void timerIsr() {
  encoder->service();
  if(running) {
    milli -= 1;
  }
  sleepTimer -= 1;
}

void feedSleepTimer() {
  sleepTimer = SLEEP_TIMEOUT;
}

void goToSleep() {
  displayOff();

  Timer1.detachInterrupt();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(digitalPinToInterrupt(SLEEP_INTERRUPT_1), wakeUp, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SLEEP_INTERRUPT_2), wakeUp, CHANGE);
  sleep_mode();
  // Zzzzz. When we wake, we resume here...
  sleep_disable();
  detachInterrupt(SLEEP_INTERRUPT_2);
  detachInterrupt(SLEEP_INTERRUPT_1);

  // Run setup again, so everything is back to the initial state
  setup();
}

void wakeUp() {
  // This happens after we wake up.
  feedSleepTimer();
}

void displayOff() {
  // We may need to turn off the display using hardware...
  display.setBrightness(0, false);
  display.setSegments(data);
}

void displayOn() {
  display.setBrightness(7, true);
  display.setSegments(data);
}

void displayEnd() {
//
//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---
//      D
// XGFEDCBA
  data[0] = 0;
  data[1] = 0b01111001;
  data[2] = 0b01010100;
  data[3] = 0b01011110;
}

void displayTime(void) {
  int8_t minutes = left / 60;
  int8_t seconds = left % 60;

  uint8_t dot = (!running || milli < 500) ? 0x80 : 0x0;
    
  data[0] = display.encodeDigit(minutes / 10);
  data[1] = display.encodeDigit(minutes % 10) | dot;
  data[2] = display.encodeDigit(seconds / 10);
  data[3] = display.encodeDigit(seconds % 10);
}

void updateTime(void) {
  if(showEnd) {
    displayEnd();
  } else {  
    displayTime();
  }
  
  display.setSegments(data);
}

void setup() {
  encoder = new ClickEncoder(ENCODER_A, ENCODER_B, ENCODER_BUTTON, 4, HIGH);
  encoder->setAccelerationEnabled(true);
    
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  pinMode(LIGHT, OUTPUT);

  running = false;
  showEnd = false;
  lightOn = false;
  
  left = 0;
  last = 0;
  milli = 1000;
  
  feedSleepTimer();
  displayOn();
  updateTime();
}

void checkButton() {
  ClickEncoder::Button b = encoder->getButton();
  if(b == ClickEncoder::Released) {
    running = !running;
  }
}

void checkLight() {
  if(running && !lightOn) {
    digitalWrite(LIGHT, HIGH);
    lightOn = true;
  } else if(!running && lightOn) {
    digitalWrite(LIGHT, LOW);
    lightOn = false;
  }
}

void done() {
  showEnd = true;
  tone(SPEAKER, 1500, 2000);
}

void loop() {
  checkButton();
  
  left += encoder->getValue();
  
  if(left > MAX_SECONDS) {
    left = MAX_SECONDS;
  }
  
  if(milli <= 0) {
    left--;
    milli = 1000;
  }

  if(left < 0) {
    left = 0;
  }

  // If the timer is running, the display needs to be updated every 500ms, otherwise it only needs updating if the encode value changes.
  if ((running && milli / 500 % 2 == 0) || left != last) {
    if(left != last) {
      last = left;
    }
    
    if(running && left == 0) {
      running = false;
      done();
    } else {
      showEnd = false;
    }
    
    updateTime();
    feedSleepTimer();
  }

  checkLight();

  if(sleepTimer == 0) {
    goToSleep();
  }
}
