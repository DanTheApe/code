#include "GeigerCounter.hpp"

GeigerCounter::GeigerCounter(int pin) : pin_(pin) {}

void GeigerCounter::begin() {
  pinMode(pin_, INPUT);
  attachInterrupt(pin_, geigerISR, RISING);
}

int GeigerCounter::takeCountsAndReset() {
  noInterrupts();
  uint32_t p = GEIGER_PULSE_COUNT;
  GEIGER_PULSE_COUNT = 0;
  interrupts();
  return static_cast<int>(p);
}
