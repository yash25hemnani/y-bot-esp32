#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "comms/protocol/Commands.h"
#include "app/Responder.h"

class SettingsModule
{
private:
    static bool isAllowedKey(const char *key)
    {
        return strcmp(key, "device_id") == 0 ||
               strcmp(key, "wifi_name") == 0 ||
               strcmp(key, "wifi_password") == 0 ||
               strcmp(key, "hmac_secret") == 0;
    }

    static bool isSecretKey(const char *key)
    {
        return strcmp(key, "wifi_password") == 0 || strcmp(key, "hmac_secret") == 0;
    }

public:
    static void onCommand(const Command &cmd)
    {
        Preferences prefs;
        auto reply = [&cmd](const String &msg) { Responder::reply(cmd, msg); };

        if (strcmp(cmd.action, "write") == 0)
        {
            if (!isAllowedKey(cmd.key))
            {
                reply("error:unknown_key");
                return;
            }

            prefs.begin("device", false);
            prefs.putString(cmd.key, cmd.value);
            prefs.end();

            // Don't log cmd.value - wifi_password/hmac_secret would end up in the serial log.
            Serial.printf("Key saved: %s\n", cmd.key);
            reply("settings:write:" + String(cmd.key) + ":ok");
        }
        else if (strcmp(cmd.action, "read") == 0)
        {
            if (!isAllowedKey(cmd.key))
            {
                reply("error:unknown_key");
                return;
            }

            if (isSecretKey(cmd.key))
            {
                reply("error:write_only");
                return;
            }

            prefs.begin("device", true);
            String value = prefs.getString(cmd.key, "");
            prefs.end();

            reply("settings:read:" + String(cmd.key) + ":" + value);
            
            // Don't log the value.
            Serial.printf("Key read: %s\n", cmd.key);
        }
        else if (strcmp(cmd.action, "wifi_ready") == 0)
        {
            reply("settings:wifi_ready:" + String(wifiCredentialAvailable() ? "1" : "0"));
        }
        else if (strcmp(cmd.action, "setup_completed") == 0)
        {
            reply("settings:setup_completed:" + String(deviceSetupCompleted() ? "1" : "0"));
        }
        else
        {
            reply("error:unknown_action");
        }
    }

    static String loadKey(const String &key)
    {
        Preferences prefs;

        prefs.begin("device", true);

        String value = prefs.getString(key.c_str(), "");

        prefs.end();

        return value;
    }

    static boolean wifiCredentialAvailable()
    {
        Preferences prefs;

        prefs.begin("device", true);

        String name = prefs.getString("wifi_name", "");
        String password = prefs.getString("wifi_password", "");

        prefs.end();

        return !name.isEmpty() && !password.isEmpty();
    }

    static boolean deviceSetupCompleted()
    {
        Preferences prefs;

        prefs.begin("device", true);

        String deviceId = prefs.getString("device_id", "");
        String hmacSecret = prefs.getString("hmac_secret", "");

        prefs.end();

        return !deviceId.isEmpty() && !hmacSecret.isEmpty();
    }
};