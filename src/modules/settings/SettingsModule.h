#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <comms/protocol/Commands.h>
#include <comms/ble/BleManager.h>

class SettingsModule
{
private:
    static inline Preferences prefs;
    static inline BleManager *bleManager = nullptr;

    static bool isAllowedKey(const char *key)
    {
        return strcmp(key, "device_id") == 0 ||
               strcmp(key, "wifi_name") == 0 ||
               strcmp(key, "wifi_password") == 0 ||
               strcmp(key, "hmac_secret") == 0;
    }

public:
    static void init(BleManager *ble)
    {
        bleManager = ble;
    }

    static void onCommand(const Command &cmd)
    {
        if (strcmp(cmd.action, "write") == 0)
        {
            prefs.begin("device", false);

            if (isAllowedKey(cmd.key))
            {
                prefs.putString(cmd.key, cmd.value);
                // Don't log cmd.value - wifi_password/hmac_secret would end up in the serial log.
                Serial.printf("Key saved: %s\n", cmd.key);
            }

            prefs.end();
        }
        else if (strcmp(cmd.action, "read") == 0)
        {
            prefs.begin("device", true);

            if (isAllowedKey(cmd.key))
            {
                String value = prefs.getString(cmd.key, "");

                if (bleManager != nullptr)
                    bleManager->sendResponse("settings:read:" + String(cmd.key) + ":" + value);

                // Don't log the value - wifi_password/hmac_secret would end up in the serial log.
                Serial.printf("Key read: %s\n", cmd.key);
            }

            prefs.end();
        }
        else if (strcmp(cmd.action, "wifi_ready") == 0)
        {
            if (bleManager != nullptr)
                bleManager->sendResponse("settings:wifi_ready:" + String(wifiCredentialAvailable() ? "1" : "0"));
        }
        else if (strcmp(cmd.action, "setup_completed") == 0)
        {
            if (bleManager != nullptr)
                bleManager->sendResponse("settings:setup_completed:" + String(deviceSetupCompleted() ? "1" : "0"));
        }
    }

    static String loadKey(const String &key)
    {
        prefs.begin("device", true);

        String value = prefs.getString(key.c_str(), "");

        prefs.end();

        return value;
    }

    static boolean wifiCredentialAvailable()
    {
        prefs.begin("device", true);

        String name = prefs.getString("wifi_name", "");
        String password = prefs.getString("wifi_password", "");

        prefs.end();

        return !name.isEmpty() && !password.isEmpty();
    }

    static boolean deviceSetupCompleted()
    {
        prefs.begin("device", true);

        String deviceId = prefs.getString("device_id", "");
        String hmacSecret = prefs.getString("hmac_secret", "");

        prefs.end();

        return !deviceId.isEmpty() && !hmacSecret.isEmpty();
    }
};