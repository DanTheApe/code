#pragma once

#include "Config.hpp"

class GeigerCounter {
public:
  explicit GeigerCounter(int pin);
  void begin();
  int takeCountsAndReset();

private:
  int pin_;
};
