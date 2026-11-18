#pragma once
#include <Arduino.h>
#include "app/EventBus.h"
#include "app/Responder.h"
#include "comms/protocol/Commands.h"
#include "config/Pins.h"
#include "drivers/BuzzerDriver.h"
#include "modules/battery/BatteryModule.h"
#include "modules/buzzer/Notes.h"

// Everything the buzzer can play. Only this 1-byte ID goes through the queue.
enum class Sound : uint8_t
{
    BOOT,
    TICK,
    HAPPY,
    WARNING,
    FIND,
    RIFF
};

// Plays melodies without blocking anyone.
//
// onEvent / onCommand only drop a Sound ID into a queue and return right away.
// One buzzer task takes IDs off the queue and plays them note by note, so a
// 3-second tune never stalls the event bus or the command router.
class BuzzerModule
{
private:
    static constexpr uint8_t CHANNEL = 4;
    static constexpr uint32_t WARNING_REPEAT_MS = 60000; // re-warn every minute
    static constexpr int WARNING_CLEAR_PCT = 20;         // same as BatteryModule's LOW_OFF_PCT

    struct Melody
    {
        const Note *notes;
        uint8_t count;
    };

    static constexpr Note BOOT_TUNE[] = {{NOTE_C5, 120}, {NOTE_E5, 120}, {NOTE_G5, 250}};

    static constexpr Note TICK_TUNE[] = {{NOTE_A5, 15}};

    static constexpr Note HAPPY_TUNE[] = {
        {NOTE_G4, 120}, {NOTE_C5, 120}, {NOTE_E5, 120}, {NOTE_G5, 120}, {NOTE_E5, 120}, {NOTE_G5, 400}};

    static constexpr Note WARNING_TUNE[] = {
        {NOTE_A5, 250}, {NOTE_E5, 400}, {NOTE_REST, 200}, {NOTE_A5, 250}, {NOTE_E5, 400}};

    static constexpr Note FIND_TUNE[] = {
        {NOTE_FIND, 150}, {NOTE_REST, 100}, {NOTE_FIND, 150}, {NOTE_REST, 100}, {NOTE_FIND, 150}, {NOTE_REST, 100}, {NOTE_FIND, 150}, {NOTE_REST, 100}, {NOTE_FIND, 150}};

    static constexpr Note RIFF_TUNE[] = {
        {NOTE_FS4, 200}, {NOTE_CS5, 200}, {NOTE_B4, 200}, {NOTE_CS5, 200}, {NOTE_B4, 200}, {NOTE_CS5, 200}, {NOTE_A4, 200}, {NOTE_B4, 200}, {NOTE_CS5, 200}, {NOTE_B4, 200}, {NOTE_A4, 200}, {NOTE_G4, 500}};

    static inline BuzzerDriver driver; // only the buzzer task touches this after init
    static inline QueueHandle_t queue = nullptr;
    static inline volatile bool muted = false;         // written by router task, read by buzzer task
    static inline volatile bool warningActive = false; // set by event bus task, cleared by buzzer task

    static Melody melodyFor(Sound s)
    {
        switch (s)
        {
        case Sound::BOOT:
            return {BOOT_TUNE, sizeof(BOOT_TUNE) / sizeof(Note)};
        case Sound::TICK:
            return {TICK_TUNE, sizeof(TICK_TUNE) / sizeof(Note)};
        case Sound::HAPPY:
            return {HAPPY_TUNE, sizeof(HAPPY_TUNE) / sizeof(Note)};
        case Sound::WARNING:
            return {WARNING_TUNE, sizeof(WARNING_TUNE) / sizeof(Note)};
        case Sound::FIND:
            return {FIND_TUNE, sizeof(FIND_TUNE) / sizeof(Note)};
        case Sound::RIFF:
            return {RIFF_TUNE, sizeof(RIFF_TUNE) / sizeof(Note)};
        }
        return {nullptr, 0};
    }

    // One note: sound for 90% of its time, silent for the last 10%,
    // so repeated notes (B4 C#5 B4) don't blur into each other.
    static void playNote(const Note &n)
    {
        uint32_t onMs = n.ms * 9 / 10;
        driver.play(n.freq); // 0 Hz (NOTE_REST) = silence
        vTaskDelay(pdMS_TO_TICKS(onMs));
        driver.stop();
        vTaskDelay(pdMS_TO_TICKS(n.ms - onMs));
    }

    static void playSound(Sound s)
    {
        bool forced = (s == Sound::FIND); // "find my robot" plays even when muted
        Melody m = melodyFor(s);
        for (uint8_t i = 0; i < m.count; i++)
        {
            if (muted && !forced)
                break; // muted halfway through: stop now, not at the end
            playNote(m.notes[i]);
        }
        driver.stop();
    }

    static void taskLoop(void *)
    {
        Sound s;
        for (;;)
        {
            TickType_t wait = warningActive ? pdMS_TO_TICKS(WARNING_REPEAT_MS) : portMAX_DELAY;

            if (xQueueReceive(queue, &s, wait) == pdTRUE)
            {
                playSound(s);
            }
            else
            {
                // Timed out: a quiet minute passed with the warning on
                int pct = BatteryModule::getPercent();
                if (pct >= 0 && pct < WARNING_CLEAR_PCT)
                {
                    playSound(Sound::WARNING);
                }
                else
                {
                    warningActive = false; // charged or unpliggged: stop nagging
                }
            }
        }
    }

public:
    static void init() {
        driver.init(BUZZER_PIN, CHANNEL, false); // silent = LOW, driven with tone()
        queue = xQueueCreate(4, sizeof(Sound));
        xTaskCreatePinnedToCore(taskLoop, "buzzer", 2048, nullptr, 2, nullptr, 1);
    }

    // Safe from any task. Never waits: if the queue is full, the sound is dropped.
    static void request(Sound s) {
        if (queue == nullptr) {
            return;
        }
        if (muted && s != Sound::FIND) {
            return;
        }

        xQueueSend(queue, &s, 0);
    }

    // From event bus
    static void onEvent(const Event &e)
    {
        switch (e.type)
        {
        case EventType::MENU_PRESSED:
        case EventType::UP_PRESSED:
        case EventType::DOWN_PRESSED:
            request(Sound::TICK);
            break;
        case EventType::PAT_STREAK_6:
            request(Sound::HAPPY);
            break;
        case EventType::BATTERY_LOW:
            warningActive = true; // the task repeats it every minute from now on
            request(Sound::WARNING);
            break;
        default:
            break;
        }
    }

    // From command router
    // buzzer:beep | buzzer:mute | buzzer:unmute
    // buzzer:play:boot|tick|happy|warning|find|riff
    static void onCommand(const Command &cmd)
    {
        if (strcmp(cmd.action, "beep") == 0)
        {
            request(Sound::FIND);
            Responder::reply(cmd, "buzzer:beep:ok");
        }
        else if (strcmp(cmd.action, "mute") == 0)
        {
            muted = true;
            Responder::reply(cmd, "buzzer:mute:1");
        }
        else if (strcmp(cmd.action, "unmute") == 0)
        {
            muted = false;
            Responder::reply(cmd, "buzzer:mute:0");
        }
        else if (strcmp(cmd.action, "play") == 0)
        {
            // buzzer:play:<name>, used by the app's sound test screen
            static const struct
            {
                const char *name;
                Sound sound;
            } SOUNDS[] = {
                {"boot", Sound::BOOT},
                {"tick", Sound::TICK},
                {"happy", Sound::HAPPY},
                {"warning", Sound::WARNING},
                {"find", Sound::FIND},
                {"riff", Sound::RIFF},
            };

            for (const auto &entry : SOUNDS)
            {
                if (strcmp(cmd.key, entry.name) == 0)
                {
                    request(entry.sound);
                    char reply[40];
                    snprintf(reply, sizeof(reply), "buzzer:play:%s:ok", entry.name);
                    Responder::reply(cmd, reply);
                    return;
                }
            }
            Responder::reply(cmd, "error:unknown_sound");
        }
        else
            Responder::reply(cmd, "error:unknown_action");
    }
};