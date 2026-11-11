#pragma once
#include <Arduino.h>

constexpr size_t CMD_FIELD_MAX = 16;   // module/action/key: 15 chars + '\0'
constexpr size_t CMD_VALUE_MAX = 128;  // value: 127 chars + '\0' (passwords, secrets)

struct Command {
    char module[CMD_FIELD_MAX];
    char action[CMD_FIELD_MAX];
    char key[CMD_FIELD_MAX];
    char value[CMD_VALUE_MAX];
};
