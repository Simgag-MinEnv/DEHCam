
#include "Particle.h"
#include "dct.h"

SYSTEM_MODE(MANUAL);

void setup() {
    Cellular.setActiveSim(EXTERNAL_SIM);
    Cellular.setCredentials("staticipeast.telus.com");

    // This clears the setup done flag on brand new devices so it won't stay in listening mode
    const uint8_t val = 0x01;
    dct_write_app_data(&val, DCT_SETUP_DONE_OFFSET, 1);

    // This is just so you know the operation is complete
    pinMode(D7, OUTPUT);
    digitalWrite(D7, HIGH);
}

void loop() {
}