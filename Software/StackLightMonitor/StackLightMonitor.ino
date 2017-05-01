#include "StackLight.h"

#define RED    0
#define YELLOW 1
#define GREEN  2

#define RPIN 9
#define YPIN 6
#define GPIN 10

const uint8_t modulePins[3] = {RPIN, YPIN, GPIN};
StackLight stackLight = StackLight(3, modulePins);

void setup() {
  // put your setup code here, to run once:
  stackLight.setPattern(RED,
                        StackLight::PULSE,
                        255,
                        1000);
  stackLight.setPattern(YELLOW,
                        StackLight::PULSE,
                        255,
                        2000);
  stackLight.setPattern(GREEN,
                        StackLight::PULSE,
                        255,
                        4000);
}

void loop() {
  // put your main code here, to run repeatedly:
  stackLight.update();
}
