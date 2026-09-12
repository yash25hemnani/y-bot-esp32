#pragma once
#include <Arduino.h>

struct Command {
    char module[16];
    char action[16];
    char key[16];
    char value[16];
};

