#include "LED/led-v1.hpp"

StatusLeds::StatusLeds(const char* chip_name,
                       int red_gpio, int yellow_gpio, int green_gpio,
                       const char* consumer)
  : red_(chip_name, red_gpio, consumer),
    yellow_(chip_name, yellow_gpio, consumer),
    green_(chip_name, green_gpio, consumer) {}

void StatusLeds::idle() {
  red_.off();
  green_.off();
  yellow_.on();
}

void StatusLeds::granted() {
  red_.off();
  yellow_.on();   
  green_.on();
}

void StatusLeds::denied() {
  green_.off();
  yellow_.on();
  red_.on();
}

void StatusLeds::all_off() {
  red_.off();
  yellow_.off();
  green_.off();
}