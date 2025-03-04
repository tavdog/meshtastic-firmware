/**
 * @file ExternalNotificationModule.cpp
 * @brief Implementation of the ExternalNotificationModule class.
 *
 * This file contains the implementation of the ExternalNotificationModule class, which is responsible for handling external
 * notifications such as vibration, buzzer, and LED lights. The class provides methods to turn on and off the external
 * notification outputs and to play ringtones using PWM buzzer. It also includes default configurations and a runOnce() method to
 * handle the module's behavior.
 *
 * Documentation:
 * https://meshtastic.org/docs/configuration/module/external-notification
 *
 * @author Jm Casler & Meshtastic Team
 * @date [Insert Date]
 */
#include "ExternalNotificationModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "Router.h"
#include "buzz/buzz.h"
#include "configuration.h"
#include "main.h"
#include "mesh/generated/meshtastic/rtttl.pb.h"
#include <Arduino.h>

#include <AXS15231B.h>
#include <TFT_eSPI.h>
TFT_eSPI m_lcd = TFT_eSPI(640, 180);
TFT_eSprite m_sprite = TFT_eSprite(&m_lcd);

#ifdef HAS_NCP5623
#include <graphics/RAKled.h>
#endif

#ifdef HAS_NEOPIXEL
#include <graphics/NeoPixel.h>
#endif

#ifdef UNPHONE
#include "unPhone.h"
extern unPhone unphone;
#endif

#if defined(HAS_NCP5623) || defined(RGBLED_RED) || defined(HAS_NEOPIXEL) || defined(UNPHONE)
uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;
uint8_t colorState = 1;
uint8_t brightnessIndex = 0;
uint8_t brightnessValues[] = {0, 10, 20, 30, 50, 90, 160, 170}; // blue gets multiplied by 1.5
bool ascending = true;
#endif

#ifndef PIN_BUZZER
#define PIN_BUZZER false
#endif

/*
    Documentation:
        https://meshtastic.org/docs/configuration/module/external-notification
*/

// Default configurations
#ifdef EXT_NOTIFY_OUT
#define EXT_NOTIFICATION_MODULE_OUTPUT EXT_NOTIFY_OUT
#else
#define EXT_NOTIFICATION_MODULE_OUTPUT 0
#endif
#define EXT_NOTIFICATION_MODULE_OUTPUT_MS 1000

#define EXT_NOTIFICATION_DEFAULT_THREAD_MS 25

#define ASCII_BELL 0x07

meshtastic_RTTTLConfig rtttlConfig;

ExternalNotificationModule *externalNotificationModule;

bool externalCurrentState[3] = {};

uint32_t externalTurnedOn[3] = {};

static const char *rtttlConfigFile = "/prefs/ringtone.proto";

int32_t ExternalNotificationModule::runOnce()
{
    if (!moduleConfig.external_notification.enabled) {
        return INT32_MAX; // we don't need this thread here...
    } else {

        bool isPlaying = rtttl::isPlaying();
#ifdef HAS_I2S
        isPlaying = rtttl::isPlaying() || audioThread->isPlaying();
#endif
        if ((nagCycleCutoff < millis()) && !isPlaying) {
            // let the song finish if we reach timeout
            nagCycleCutoff = UINT32_MAX;
            LOG_INFO("Turning off external notification: ");
            for (int i = 0; i < 3; i++) {
                setExternalState(i, false);
                externalTurnedOn[i] = 0;
                LOG_INFO("%d ", i);
            }
            LOG_INFO("");
#ifdef HAS_I2S
            // GPIO0 is used as mclk for I2S audio and set to OUTPUT by the sound library
            // T-Deck uses GPIO0 as trackball button, so restore the mode
#if defined(T_DECK) || (defined(BUTTON_PIN) && BUTTON_PIN == 0)
            pinMode(0, INPUT);
#endif
#endif
            isNagging = false;
            return INT32_MAX; // save cycles till we're needed again
        }

        // If the output is turned on, turn it back off after the given period of time.
        if (isNagging) {
            if (externalTurnedOn[0] + (moduleConfig.external_notification.output_ms ? moduleConfig.external_notification.output_ms
                                                                                    : EXT_NOTIFICATION_MODULE_OUTPUT_MS) <
                millis()) {
                setExternalState(0, !getExternal(0));
            }
            if (externalTurnedOn[1] + (moduleConfig.external_notification.output_ms ? moduleConfig.external_notification.output_ms
                                                                                    : EXT_NOTIFICATION_MODULE_OUTPUT_MS) <
                millis()) {
                setExternalState(1, !getExternal(1));
            }
            if (externalTurnedOn[2] + (moduleConfig.external_notification.output_ms ? moduleConfig.external_notification.output_ms
                                                                                    : EXT_NOTIFICATION_MODULE_OUTPUT_MS) <
                millis()) {
                LOG_DEBUG("EXTERNAL 2 %d compared to %d", externalTurnedOn[2] + moduleConfig.external_notification.output_ms,
                          millis());
                setExternalState(2, !getExternal(2));
            }
#if defined(HAS_NCP5623) || defined(RGBLED_RED) || defined(HAS_NEOPIXEL) || defined(UNPHONE)
            red = (colorState & 4) ? brightnessValues[brightnessIndex] : 0;          // Red enabled on colorState = 4,5,6,7
            green = (colorState & 2) ? brightnessValues[brightnessIndex] : 0;        // Green enabled on colorState = 2,3,6,7
            blue = (colorState & 1) ? (brightnessValues[brightnessIndex] * 1.5) : 0; // Blue enabled on colorState = 1,3,5,7
#ifdef HAS_NCP5623
            if (rgb_found.type == ScanI2C::NCP5623) {
                rgb.setColor(red, green, blue);
            }
#endif
#ifdef RGBLED_CA
            analogWrite(RGBLED_RED, 255 - red); // CA type needs reverse logic
            analogWrite(RGBLED_GREEN, 255 - green);
            analogWrite(RGBLED_BLUE, 255 - blue);
#elif defined(RGBLED_RED)
            analogWrite(RGBLED_RED, red);
            analogWrite(RGBLED_GREEN, green);
            analogWrite(RGBLED_BLUE, blue);
#endif
#ifdef HAS_NEOPIXEL
            pixels.fill(pixels.Color(red, green, blue), 0, NEOPIXEL_COUNT);
            pixels.show();
#endif
#ifdef UNPHONE
            unphone.rgb(red, green, blue);
#endif
            if (ascending) { // fade in
                brightnessIndex++;
                if (brightnessIndex == (sizeof(brightnessValues) - 1)) {
                    ascending = false;
                }
            } else {
                brightnessIndex--; // fade out
            }
            if (brightnessIndex == 0) {
                ascending = true;
                colorState++; // next color
                if (colorState > 7) {
                    colorState = 1;
                }
            }
#endif

#ifdef T_WATCH_S3
            drv.go();
#endif
        }

        // Play RTTTL over i2s audio interface if enabled as buzzer
#ifdef HAS_I2S
        if (moduleConfig.external_notification.use_i2s_as_buzzer) {
            if (audioThread->isPlaying()) {
                // Continue playing
            } else if (isNagging && (nagCycleCutoff >= millis())) {
                audioThread->beginRttl(rtttlConfig.ringtone, strlen_P(rtttlConfig.ringtone));
            }
        }
#endif
        // now let the PWM buzzer play
        if (moduleConfig.external_notification.use_pwm && config.device.buzzer_gpio) {
            if (rtttl::isPlaying()) {
                rtttl::play();
            } else if (isNagging && (nagCycleCutoff >= millis())) {
                // start the song again if we have time left
                rtttl::begin(config.device.buzzer_gpio, rtttlConfig.ringtone);
            }
        }

        return EXT_NOTIFICATION_DEFAULT_THREAD_MS;
    }
}

bool ExternalNotificationModule::wantPacket(const meshtastic_MeshPacket *p)
{
    return MeshService::isTextPayload(p);
}

/**
 * Sets the external notification for the specified index.
 *
 * @param index The index of the external notification to change state.
 * @param on Whether we are turning things on (true) or off (false).
 */
void ExternalNotificationModule::setExternalState(uint8_t index, bool on)
{
    externalCurrentState[index] = on;
    externalTurnedOn[index] = millis();

    switch (index) {
    case 1:
#ifdef UNPHONE
        unphone.vibe(on); // the unPhone's vibration motor is on a i2c GPIO expander
#endif
        if (moduleConfig.external_notification.output_vibra)
            digitalWrite(moduleConfig.external_notification.output_vibra, on);
        break;
    case 2:
        if (moduleConfig.external_notification.output_buzzer)
            digitalWrite(moduleConfig.external_notification.output_buzzer, on);
        break;
    default:
        if (output > 0)
            digitalWrite(output, (moduleConfig.external_notification.active ? on : !on));
        break;
    }

#if defined(HAS_NCP5623) || defined(RGBLED_RED) || defined(HAS_NEOPIXEL) || defined(UNPHONE)
    if (!on) {
        red = 0;
        green = 0;
        blue = 0;
    }
#endif

#ifdef HAS_NCP5623
    if (rgb_found.type == ScanI2C::NCP5623) {
        rgb.setColor(red, green, blue);
    }
#endif
#ifdef RGBLED_CA
    analogWrite(RGBLED_RED, 255 - red); // CA type needs reverse logic
    analogWrite(RGBLED_GREEN, 255 - green);
    analogWrite(RGBLED_BLUE, 255 - blue);
#elif defined(RGBLED_RED)
    analogWrite(RGBLED_RED, red);
    analogWrite(RGBLED_GREEN, green);
    analogWrite(RGBLED_BLUE, blue);
#endif
#ifdef HAS_NEOPIXEL
    pixels.fill(pixels.Color(red, green, blue), 0, NEOPIXEL_COUNT);
    pixels.show();
#endif
#ifdef UNPHONE
    unphone.rgb(red, green, blue);
#endif
#ifdef T_WATCH_S3
    if (on) {
        drv.go();
    } else {
        drv.stop();
    }
#endif
}

bool ExternalNotificationModule::getExternal(uint8_t index)
{
    return externalCurrentState[index];
}

void ExternalNotificationModule::stopNow()
{
    rtttl::stop();
#ifdef HAS_I2S
    if (audioThread->isPlaying())
        audioThread->stop();
#endif
    nagCycleCutoff = 1; // small value
    isNagging = false;
    setIntervalFromNow(0);
#ifdef T_WATCH_S3
    drv.stop();
#endif
}

ExternalNotificationModule::ExternalNotificationModule()
    : SinglePortModule("ExternalNotificationModule", meshtastic_PortNum_TEXT_MESSAGE_APP),
      concurrency::OSThread("ExternalNotification")
{

    /*
        Uncomment the preferences below if you want to use the module
        without having to configure it from the PythonAPI or WebUI.
    */

    moduleConfig.external_notification.alert_message = true;

    moduleConfig.external_notification.active = true;

    moduleConfig.external_notification.enabled = true;

    if (moduleConfig.external_notification.enabled) {

        // m_lcd = TFT_eSPI(640, 180);
        // m_sprite = TFT_eSprite(&m_lcd);
        axs15231_init();

        LOG_INFO("DOING WINDYTRON_LOGO");
        digitalWrite(TFT_BL, LOW); // turn off backlight asap to minimise power on artifacts

        m_sprite.createSprite(640, 180); // full screen landscape sprite in psram
        m_sprite.setSwapBytes(1);
        // m_sprite.setFreeFont(&FreeMonoBold12pt7b);
        // m_sprite.setCursor(75, 16);
        // m_sprite.print(devicestate.owner.long_name); // maximum 6
        // lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t *)m_sprite.getPointer());
        lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t *)&gImage);
        // fade in the screen
        for (int i = 0; i < 256; i++) {
            analogWrite(TFT_BL, i);
            delay(10); // Adjust delay to control the wipe speed
        }
        // delay(3000);
        LOG_INFO("DONE WINDYTRON_LOGO");

        if (nodeDB->loadProto(rtttlConfigFile, meshtastic_RTTTLConfig_size, sizeof(meshtastic_RTTTLConfig),
                              &meshtastic_RTTTLConfig_msg, &rtttlConfig) != LoadFileResult::LOAD_SUCCESS)
        {
            memset(rtttlConfig.ringtone, 0, sizeof(rtttlConfig.ringtone));
            strncpy(rtttlConfig.ringtone,
                    "24:d=32,o=5,b=565:f6,p,f6,4p,p,f6,p,f6,2p,p,b6,p,b6,p,b6,p,b6,p,b,p,b,p,b,p,b,p,b,p,b,p,b,p,b,1p.,2p.,p",
                    sizeof(rtttlConfig.ringtone));
        }

        LOG_INFO("Init External Notification Module");

        output = moduleConfig.external_notification.output ? moduleConfig.external_notification.output
                                                           : EXT_NOTIFICATION_MODULE_OUTPUT;

        // Set the direction of a pin
        if (output > 0) {
            LOG_INFO("Use Pin %i in digital mode", output);
            pinMode(output, OUTPUT);
        }
        setExternalState(0, false);
        externalTurnedOn[0] = 0;
        if (moduleConfig.external_notification.output_vibra) {
            LOG_INFO("Use Pin %i for vibra motor", moduleConfig.external_notification.output_vibra);
            pinMode(moduleConfig.external_notification.output_vibra, OUTPUT);
            setExternalState(1, false);
            externalTurnedOn[1] = 0;
        }
        if (moduleConfig.external_notification.output_buzzer) {
            if (!moduleConfig.external_notification.use_pwm) {
                LOG_INFO("Use Pin %i for buzzer", moduleConfig.external_notification.output_buzzer);
                pinMode(moduleConfig.external_notification.output_buzzer, OUTPUT);
                setExternalState(2, false);
                externalTurnedOn[2] = 0;
            } else {
                config.device.buzzer_gpio = config.device.buzzer_gpio ? config.device.buzzer_gpio : PIN_BUZZER;
                // in PWM Mode we force the buzzer pin if it is set
                LOG_INFO("Use Pin %i in PWM mode", config.device.buzzer_gpio);
            }
        }
#ifdef HAS_NCP5623
        if (rgb_found.type == ScanI2C::NCP5623) {
            rgb.begin();
            rgb.setCurrent(10);
        }
#endif
#ifdef RGBLED_RED
        pinMode(RGBLED_RED, OUTPUT); // set up the RGB led pins
        pinMode(RGBLED_GREEN, OUTPUT);
        pinMode(RGBLED_BLUE, OUTPUT);
#endif
#ifdef RGBLED_CA
        analogWrite(RGBLED_RED, 255);   // with a common anode type, logic is reversed
        analogWrite(RGBLED_GREEN, 255); // so we want to initialise with lights off
        analogWrite(RGBLED_BLUE, 255);
#endif
#ifdef HAS_NEOPIXEL
        pixels.begin(); // Initialise the pixel(s)
        pixels.clear(); // Set all pixel colors to 'off'
        pixels.setBrightness(moduleConfig.ambient_lighting.current);
#endif
    } else {
        LOG_INFO("External Notification Module Disabled");
        disable();
    }
}

ProcessMessage ExternalNotificationModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (moduleConfig.external_notification.enabled && !isMuted) {
#ifdef T_WATCH_S3
        drv.setWaveform(0, 75);
        drv.setWaveform(1, 56);
        drv.setWaveform(2, 0);
        drv.go();
#endif
        if (!isFromUs(&mp)) {
            // Check if the message contains a bell character. Don't do this loop for every pin, just once.
            auto &p = mp.decoded;
            bool containsBell = false;
            for (int i = 0; i < p.payload.size; i++) {
                if (p.payload.bytes[i] == ASCII_BELL) {
                    containsBell = true;
                }
            }

            if (moduleConfig.external_notification.alert_bell) {
                if (containsBell) {
                    LOG_INFO("externalNotificationModule - Notification Bell");
                    isNagging = true;
                    setExternalState(0, true);
                    if (moduleConfig.external_notification.nag_timeout) {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                    } else {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                    }
                }
            }

            if (moduleConfig.external_notification.alert_bell_vibra) {
                if (containsBell) {
                    LOG_INFO("externalNotificationModule - Notification Bell (Vibra)");
                    isNagging = true;
                    setExternalState(1, true);
                    if (moduleConfig.external_notification.nag_timeout) {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                    } else {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                    }
                }
            }

            if (moduleConfig.external_notification.alert_bell_buzzer) {
                if (containsBell) {
                    LOG_INFO("externalNotificationModule - Notification Bell (Buzzer)");
                    isNagging = true;
                    if (!moduleConfig.external_notification.use_pwm) {
                        setExternalState(2, true);
                    } else {
#ifdef HAS_I2S
                        audioThread->beginRttl(rtttlConfig.ringtone, strlen_P(rtttlConfig.ringtone));
#else
                        rtttl::begin(config.device.buzzer_gpio, rtttlConfig.ringtone);
#endif
                    }
                    if (moduleConfig.external_notification.nag_timeout) {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                    } else {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                    }
                }
            }

            if (moduleConfig.external_notification.alert_message) {
                LOG_INFO("externalNotificationModule - Notification Module\n");
                LOG_INFO("FromID : 0x%0x\n", mp.from);
                if (mp.from == 0xa3251978) {
                    LOG_INFO("DISPLAY_WIND");

                    displayWind(mp);
                    return ProcessMessage::STOP; // Just display and then stop.
                } else {
                    // LOG_INFO("DISPLAY_TEXT");
                    // displayText(mp);
                }

                isNagging = true;
                setExternalState(0, true);
                if (moduleConfig.external_notification.nag_timeout) {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                } else {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                }
            }

            if (moduleConfig.external_notification.alert_message_vibra) {
                LOG_INFO("externalNotificationModule - Notification Module (Vibra)");
                isNagging = true;
                setExternalState(1, true);
                if (moduleConfig.external_notification.nag_timeout) {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                } else {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                }
            }

            if (moduleConfig.external_notification.alert_message_buzzer) {
                LOG_INFO("externalNotificationModule - Notification Module (Buzzer)");
                isNagging = true;
                if (!moduleConfig.external_notification.use_pwm && !moduleConfig.external_notification.use_i2s_as_buzzer) {
                    setExternalState(2, true);
                } else {
#ifdef HAS_I2S
                    if (moduleConfig.external_notification.use_i2s_as_buzzer) {
                        audioThread->beginRttl(rtttlConfig.ringtone, strlen_P(rtttlConfig.ringtone));
                    }
#else
                    rtttl::begin(config.device.buzzer_gpio, rtttlConfig.ringtone);
#endif
                }
                if (moduleConfig.external_notification.nag_timeout) {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                } else {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                }
            }
            setIntervalFromNow(0); // run once so we know if we should do something
        }
    } else {
        LOG_INFO("External Notification Module Disabled or muted");
    }

    return ProcessMessage::CONTINUE; // Let others look at this message also if they want
}

/**
 * @brief An admin message arrived to AdminModule. We are asked whether we want to handle that.
 *
 * @param mp The mesh packet arrived.
 * @param request The AdminMessage request extracted from the packet.
 * @param response The prepared response
 * @return AdminMessageHandleResult HANDLED if message was handled
 *   HANDLED_WITH_RESULT if a result is also prepared.
 */
AdminMessageHandleResult ExternalNotificationModule::handleAdminMessageForModule(const meshtastic_MeshPacket &mp,
                                                                                 meshtastic_AdminMessage *request,
                                                                                 meshtastic_AdminMessage *response)
{
    AdminMessageHandleResult result;

    switch (request->which_payload_variant) {
    case meshtastic_AdminMessage_get_ringtone_request_tag:
        LOG_INFO("Client getting ringtone");
        this->handleGetRingtone(mp, response);
        result = AdminMessageHandleResult::HANDLED_WITH_RESPONSE;
        break;

    case meshtastic_AdminMessage_set_ringtone_message_tag:
        LOG_INFO("Client setting ringtone");
        this->handleSetRingtone(request->set_canned_message_module_messages);
        result = AdminMessageHandleResult::HANDLED;
        break;

    default:
        result = AdminMessageHandleResult::NOT_HANDLED;
    }

    return result;
}

void ExternalNotificationModule::handleGetRingtone(const meshtastic_MeshPacket &req, meshtastic_AdminMessage *response)
{
    LOG_INFO("*** handleGetRingtone");
    if (req.decoded.want_response) {
        response->which_payload_variant = meshtastic_AdminMessage_get_ringtone_response_tag;
        strncpy(response->get_ringtone_response, rtttlConfig.ringtone, sizeof(response->get_ringtone_response));
    } // Don't send anything if not instructed to. Better than asserting.
}

void ExternalNotificationModule::handleSetRingtone(const char *from_msg)
{
    int changed = 0;

    if (*from_msg) {
        changed |= strcmp(rtttlConfig.ringtone, from_msg);
        strncpy(rtttlConfig.ringtone, from_msg, sizeof(rtttlConfig.ringtone));
        LOG_INFO("*** from_msg.text:%s", from_msg);
    }

    if (changed) {
        nodeDB->saveProto(rtttlConfigFile, meshtastic_RTTTLConfig_size, &meshtastic_RTTTLConfig_msg, &rtttlConfig);
    }
}

#define SMALL &FreeMonoBold9pt7b
#define MEDIUM &FreeSansBold12pt7b
#define MEDLAR &FreeSansBold18pt7b
#define LARGE &FreeSansBold24pt7b
#define gray 0x6B6D
#define blue 0x0AAD
#define orange 0xC260
#define purple 0x604D
#define green 0x1AE9
void ExternalNotificationModule::displayWind(const meshtastic_MeshPacket &mp)
{

    auto &p = mp.decoded;
    char msg[70] = "";
    sprintf(msg, "%s", p.payload.bytes);
    if (strcmp(msg, last_data) == 0)
        return; // don't re-display duplicate info

    // fade out the screen animation.

    for (int i = 255; i > 0; i--) {
        analogWrite(TFT_BL, i);
        delay(10); // Adjust delay to control the wipe speed
    }

    m_sprite.fillSprite(TFT_BLACK);

    strcpy(last_data, msg);

    String s = String(msg);
    String parts[20];
    if (s.indexOf("DUALWIND") > -1) { // WE HAVE DUALWIND, PARSE hackpack
        int maxParts = 20;
        int partIndex = 0;
        int startIndex = 0;
        int delimiterIndex = s.indexOf('#');

        while (delimiterIndex != -1 && partIndex < maxParts) {
            parts[partIndex++] = s.substring(startIndex, delimiterIndex);
            // Serial.println(s.substring(startIndex, delimiterIndex));
            startIndex = delimiterIndex + 1;
            delimiterIndex = s.indexOf('#', startIndex);
        }
        parts[partIndex] = s.substring(startIndex);
        // Serial.println(parts[partIndex]);
        int colon = parts[1].indexOf(':');
        String hour = parts[1].substring(0, colon);
        String minute = parts[1].substring(colon + 1, colon + 3);
        LOG_INFO("minute:");
        LOG_INFO(minute.c_str());
        // dual style, HASH style
        // DUALWIND#23:49:#Kanaha + Pauwela#NE#41#19#22#1.6f,11s,S172#0814N0.0~1542H2.7#Kihei#N#357#11#19#1.6f,11s,S172#0814N0.0
        // ~1542H2.7
        m_sprite.fillSprite(TFT_BLACK);

        // LABEL
        // display_label(m_data->get("label"),m_data->get("label2"));
        int x = 20;
        if (s.length() > 10)
            x = 5; // move the x coord over if long name
        m_sprite.setCursor(x, 30);
        m_sprite.setFreeFont(MEDIUM);
        m_sprite.setTextSize(1);
        m_sprite.print(parts[2]);
        // second set
        m_sprite.setCursor(x + 360, 30);
        m_sprite.print(parts[9]);
        // also draw a line down the middle for separation
        m_sprite.fillRect(310, 0, 4, 180, gray);

        // STAMP
        m_sprite.setFreeFont(MEDIUM);
        m_sprite.setCursor(570, 19);
        m_sprite.print(hour + ":" + minute);

        // VELOCITY
        String avg = parts[5];
        String gust = parts[6];
        if (avg.toInt() < 15)
            m_sprite.setTextColor(TFT_BLUE);
        if (avg.toInt() >= 15)
            m_sprite.setTextColor(TFT_CYAN);
        if (avg.toInt() >= 25)
            m_sprite.setTextColor(TFT_GREEN);
        if (avg.toInt() >= 30)
            m_sprite.setTextColor(TFT_MAGENTA);
        if (avg.toInt() >= 35)
            m_sprite.setTextColor(TFT_RED);
        int y = 100;
        // if (m_data->get("aux1").length() <= 1) y += 25;
        m_sprite.setCursor(120, y);
        m_sprite.setFreeFont(MEDLAR);
        m_sprite.setTextSize(2);
        m_sprite.print(avg);
        m_sprite.setFreeFont(&FreeSans12pt7b);
        m_sprite.print('g');
        m_sprite.setFreeFont(MEDLAR);
        m_sprite.print(gust);

        // second set
        String avg2 = parts[12];
        String gust2 = parts[13];
        if (avg2.toInt() < 15)
            m_sprite.setTextColor(TFT_BLUE);
        if (avg2.toInt() >= 15)
            m_sprite.setTextColor(TFT_CYAN);
        if (avg2.toInt() >= 25)
            m_sprite.setTextColor(TFT_GREEN);
        if (avg2.toInt() >= 30)
            m_sprite.setTextColor(TFT_MAGENTA);
        if (avg2.toInt() >= 35)
            m_sprite.setTextColor(TFT_RED);
        m_sprite.setCursor(120 + 330, y);
        // m_sprite.setFreeFont(LARGE);
        // m_sprite.setTextSize(1);
        m_sprite.print(avg2);
        m_sprite.setFreeFont(&FreeSans12pt7b);
        m_sprite.print('g');
        m_sprite.setFreeFont(MEDLAR);
        m_sprite.print(gust2);

        // DIRECTION
        y = 75;
        if (parts[7].length() <= 1)
            y += 25;
        String card = parts[3];
        String deg = parts[4];
        m_sprite.setTextColor(TFT_RED);
        if (card.equals("N"))
            m_sprite.setTextColor(TFT_CYAN);
        if (card.equals("NE"))
            m_sprite.setTextColor(TFT_GREEN);
        if (card.equals("ENE"))
            m_sprite.setTextColor(TFT_YELLOW);
        m_sprite.setFreeFont(MEDLAR);
        m_sprite.setTextSize(1);
        m_sprite.setCursor(5, y);
        m_sprite.print(card);
        m_sprite.setCursor(5, y + 32);
        m_sprite.print(deg); // for now we'll use * for degree symbol
        // m_sprite.setFreeFont(&FreeArial9full);
        // m_sprite.setCursor(m_sprite.getCursorX(),m_sprite.getCursorY()-10);
        // m_sprite.setTextSize(1);
        // m_sprite.print("°");

        // now do second set
        String card2 = parts[10];
        String deg2 = parts[11];
        m_sprite.setFreeFont(MEDLAR);
        m_sprite.setTextSize(1);
        m_sprite.setCursor(5 + 320, y);
        m_sprite.print(card2);
        m_sprite.setCursor(5 + 320, y + 32);
        m_sprite.print(deg2); // for now we'll use * for degree symbol
        // m_sprite.setFreeFont(&FreeArial9full);
        // m_sprite.setCursor(m_sprite.getCursorX(),m_sprite.getCursorY()-10);
        // m_sprite.setTextSize(1);
        // m_sprite.print("°");

        // AUX 1'S
        String s = parts[7];
        String s2 = parts[14];
        y = 145;
        m_sprite.setTextColor(TFT_GREENYELLOW);
        m_sprite.setFreeFont(SMALL);
        m_sprite.setTextSize(1);
        m_sprite.setCursor(10, y);
        m_sprite.print(s);
        m_sprite.setCursor(10 + 320, y);
        m_sprite.print(s2);

        // AUX 2'S
        s = parts[8];
        s2 = parts[15];
        y = 170;
        m_sprite.setTextColor(TFT_CYAN);
        m_sprite.setFreeFont(SMALL);
        m_sprite.setTextSize(1);
        m_sprite.setCursor(10, y);
        m_sprite.print(s);
        m_sprite.setCursor(10 + 320, y);
        m_sprite.print(s2);
        LOG_DEBUG("OUT OF DUALWIND DISPLAY");
    } else { // SINGLE WIND, parse with spaces or hash map

        // scope level variables for single display
        String dir;
        int degree;
        int avg;
        int gust;

        // char avg_g_gust[10]; // Adjust the size according to your needs

        String aux1 = "."; // Assuming aux1 can be a maximum of 23 characters
        String aux2 = "."; // Assuming aux2 can be a maximum of 23 characters
        String timestamp;
        String label;

        if (s.indexOf("WIND#") > -1) {
            // WIND#23:49:#Kanaha + Pauwela#NE#41#19#22#1.6f,11s,S172#0814N0.0~1542H2.7
            int maxParts = 20;
            int partIndex = 0;
            int startIndex = 0;
            int delimiterIndex = s.indexOf('#');

            while (delimiterIndex != -1 && partIndex < maxParts) {
                parts[partIndex++] = s.substring(startIndex, delimiterIndex);
                // Serial.println(s.substring(startIndex, delimiterIndex));
                startIndex = delimiterIndex + 1;
                delimiterIndex = s.indexOf('#', startIndex);
            }
            parts[partIndex] = s.substring(startIndex);
            // Serial.println(parts[partIndex]);
            int colon = parts[1].indexOf(':');
            String hour = parts[1].substring(0, colon);
            String minute = parts[1].substring(colon + 1, colon + 3);
            LOG_INFO("minute:");
            LOG_INFO(minute.c_str());
            // dual style, HASH style
            // DUALWIND#23:49:#Kanaha +
            // Pauwela#NE#41#19#22#1.6f,11s,S172#0814N0.0~1542H2.7#Kihei#N#357#11#19#1.6f,11s,S172#0814N0.0 ~1542H2.7

            m_sprite.fillSprite(TFT_BLACK);

            // String hour = parts[1].substring(0, colon);
            // String minute = parts[1].substring(colon + 1, colon + 3);
            timestamp = parts[1];
            dir = parts[3];
            degree = parts[4].toInt();
            avg = parts[5].toInt();
            gust = parts[6].toInt();
            aux1 = parts[7];
            aux2 = parts[8];
            label = parts[2];
        } else { // parse on spaces
            // String msg = "NE 51 20g25 , AUX1_AUX2 - 2021-06-29T16:10:07"; // Example message

            // Split the string based on spaces and other delimiters
            int firstSpace = s.indexOf(' ');
            dir = s.substring(0, firstSpace);

            int secondSpace = s.indexOf(' ', firstSpace + 1);
            degree = s.substring(firstSpace + 1, secondSpace).toInt();

            int gIndex = s.indexOf('g', secondSpace + 1);
            avg = s.substring(secondSpace + 1, gIndex).toInt();

            int thirdSpace = s.indexOf(' ', gIndex + 1);
            gust = s.substring(gIndex + 1, thirdSpace).toInt();

            // Continue to extract AUX1 and AUX2
            int commaIndex = s.indexOf(',', thirdSpace + 1);
            int underscoreIndex = s.indexOf('_', commaIndex + 1);
            aux1 = s.substring(commaIndex + 2, underscoreIndex);

            int dashIndex = s.indexOf('-', underscoreIndex + 1);
            aux2 = s.substring(underscoreIndex + 1, dashIndex - 1);

            // Parse the timestamp
            int tIndex = s.indexOf('T');
            if (tIndex != -1) {
                timestamp = s.substring(tIndex + 1, tIndex + 6); // Extract the time
            }
        }

        // Draw single display
        // clear the sprite
        m_sprite.fillSprite(TFT_BLACK);

        if (label.isEmpty()) {
            // Get the Channels object (assuming it's a singleton or a global instance)
            label = devicestate.owner.long_name;
            Channels channels;

            // Iterate through the channels
            for (int i = 0; i < channels.getNumChannels(); i++) {
                meshtastic_Channel &ch = channels.getByIndex(i);

                // Check if the channel has downlink enabled
                if (ch.settings.downlink_enabled) {
                    // Return the name of the channel
                    label = String(channels.getName(i));
                }
            }
        }

        int y_offset = 0;
        // we can move all fields down and display a long label at the very top.
        if (aux2[0] == '.') {
            y_offset = 22;
        }

        // DISPLAY LABEL normall but shorten if too long TODO
        m_sprite.setFreeFont(&FreeMonoBold12pt7b);
        m_sprite.setCursor(250, 16);
        m_sprite.print(label); // maximum 6

        // DISPLAY THE TIMESTAMP
        m_sprite.setFreeFont(MEDIUM);
        m_sprite.setCursor(570, 19);
        m_sprite.setTextWrap(false);
        m_sprite.print(timestamp);

        // DISPLAY VELOCITY
        // m_sprite.print(avg_g_gust);
        if (avg < 15)
            m_sprite.setTextColor(TFT_BLUE);
        if (avg >= 15)
            m_sprite.setTextColor(TFT_CYAN);
        if (avg >= 25)
            m_sprite.setTextColor(TFT_GREEN);
        if (avg >= 30)
            m_sprite.setTextColor(TFT_MAGENTA);
        if (avg >= 35)
            m_sprite.setTextColor(TFT_RED);
        int y = 150;
        // if (m_data->get("aux1").length() <= 1) y += 25;
        m_sprite.setCursor(270, y);
        m_sprite.setFreeFont(LARGE);
        m_sprite.setTextSize(3);
        m_sprite.print(avg);
        m_sprite.setFreeFont(&FreeSans12pt7b);
        m_sprite.print('g');
        m_sprite.setFreeFont(LARGE);
        m_sprite.print(gust);

        // DISPLAY DIR
        // m_sprite.print(dir);
        // m_sprite.setCursor(5, 60 + y_offset);
        // m_sprite.print(degree);
        y = 60 + y_offset;
        // if (m_data->get("aux1").length() <= 1) y += 25;
        // if (m_config->gets("api_data_url").indexOf("maui") > -1 || m_config->gets("color_profile").equals("Maui") ) {
        m_sprite.setTextColor(TFT_RED);
        if (dir.equals("N") == 0)
            m_sprite.setTextColor(TFT_CYAN);
        if (dir.equals("NE") == 0)
            m_sprite.setTextColor(TFT_GREEN);
        if (dir.equals("ENE") == 0)
            m_sprite.setTextColor(TFT_YELLOW); // } else {
        // 	m_sprite.setTextColor(TFT_GREEN);
        // }
        m_sprite.setFreeFont(LARGE);
        m_sprite.setTextSize(1);
        m_sprite.setCursor(5, y);
        m_sprite.print(dir);
        m_sprite.print(" ");
        // m_sprite.setCursor(5,y+45);
        m_sprite.print(degree); // for now we'll use * for degree symbol
        // m_sprite.setFreeFont(&FreeArial9full);
        m_sprite.setCursor(m_sprite.getCursorX(), m_sprite.getCursorY() - 10);
        m_sprite.setTextSize(2);
        m_sprite.print("°");

        // DISPLAY AUX1
        // m_sprite.print(aux1);
        m_sprite.setTextColor(TFT_GREENYELLOW);
        m_sprite.setFreeFont(MEDIUM);
        m_sprite.setTextSize(1);
        m_sprite.setCursor(10, 120 + y_offset);
        m_sprite.print(aux1);

        if (!aux1.isEmpty() && !aux1.equals(".")) {
            //  DISPLAY AUX2
            // m_sprite.print(aux2);
            m_sprite.setTextColor(TFT_CYAN);
            m_sprite.setFreeFont(MEDIUM);
            m_sprite.setTextSize(1);
            m_sprite.setCursor(10, 160);
            m_sprite.print(aux2);
        }
    }

    // m_sprite.display();
    lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t *)m_sprite.getPointer());
    // fade in the screen
    for (int i = 0; i < 256; i++) {
        analogWrite(TFT_BL, i);
        delay(10); // Adjust delay to control the wipe speed
    }
}
void ExternalNotificationModule::displayText(const String msg)
{

    if (msg.equals("fade_out")) {
        // just fade out and then reboot.
        for (int i = 255; i > 0; i--) {
            analogWrite(TFT_BL, i);
            delay(2); // Adjust delay to control the wipe speed
        }
        m_sprite.fillSprite(TFT_BLACK);
        lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t *)m_sprite.getPointer());
        return;
    }
    // m_sprite.clearBuffer();
    // lcd_fill(0, 0, 180, 640, 0x00);
    // auto &p = mp.decoded;
    // static char msg[256];
    // sprintf(msg, "%s", p.payload.bytes);
    // if (strcmp(msg, last_data) == 0)
    //     return; // don't re-display duplicate info
    // strcpy(last_data, msg);

    // parse the wind string
    // NE 51 20g25 , AUX1_AUX2 - 2021-06-29T16:10:07

    // DISPLAY Text
    m_sprite.setFreeFont(&FreeMonoBold12pt7b);
    m_sprite.setCursor(10, 20);
    // m_sprite.setTextColor(EPD_BLACK);
    m_sprite.print(msg);

    // m_sprite.display();
    lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t *)m_sprite.getPointer());
}
