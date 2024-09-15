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
#include <AXS15231B.h>
#include <Arduino.h>

TFT_eSPI m_lcd = TFT_eSPI(640, 180);
TFT_eSprite m_sprite = TFT_eSprite(&m_lcd);

// #ifdef OLD_EPD
// #include "Adafruit_ThinkInk.h"
// Adafruit_SSD1675 display = Adafruit_SSD1675(250, 122, EPD_DC, -1, EPD_CS, -1, -1);
// #elif defined(LILYGOT5)
// #include <GxDEPG0213BN/GxDEPG0213BN.h>
// #include <GxIO/GxIO.h>
// #include <GxIO/GxIO_SPI/GxIO_SPI.h>
// #define display() update()
// #define clearBuffer() fillScreen(GxEPD_WHITE)
// #define EPD_BLACK GxEPD_BLACK
// GxIO_Class m_io = GxIO_Class(SPI, EPD_CS, EPD_DC, EPD_RSET);
// GxEPD_Class display = GxEPD_Class(m_io, EPD_RSET, EPD_BUSY);
// #else
// #include "Adafruit_ThinkInk.h"
// ThinkInk_213_Mono_BN display = ThinkInk_213_Mono_BN(EPD_DC, -1, EPD_CS, -1, -1);
// #endif

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
                setExternalOff(i);
                externalTurnedOn[i] = 0;
                LOG_INFO("%d ", i);
            }
            LOG_INFO("\n");
            isNagging = false;
            return INT32_MAX; // save cycles till we're needed again
        }

        // If the output is turned on, turn it back off after the given period of time.
        if (isNagging) {
            if (externalTurnedOn[0] + (moduleConfig.external_notification.output_ms ? moduleConfig.external_notification.output_ms
                                                                                    : EXT_NOTIFICATION_MODULE_OUTPUT_MS) <
                millis()) {
                getExternal(0) ? setExternalOff(0) : setExternalOn(0);
            }
            if (externalTurnedOn[1] + (moduleConfig.external_notification.output_ms ? moduleConfig.external_notification.output_ms
                                                                                    : EXT_NOTIFICATION_MODULE_OUTPUT_MS) <
                millis()) {
                getExternal(1) ? setExternalOff(1) : setExternalOn(1);
            }
            if (externalTurnedOn[2] + (moduleConfig.external_notification.output_ms ? moduleConfig.external_notification.output_ms
                                                                                    : EXT_NOTIFICATION_MODULE_OUTPUT_MS) <
                millis()) {
                getExternal(2) ? setExternalOff(2) : setExternalOn(2);
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
        if (moduleConfig.external_notification.use_pwm) {
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
 * Sets the external notification on for the specified index.
 *
 * @param index The index of the external notification to turn on.
 */
void ExternalNotificationModule::setExternalOn(uint8_t index)
{
    externalCurrentState[index] = 1;
    externalTurnedOn[index] = millis();

    switch (index) {
    case 1:
#ifdef UNPHONE
        unphone.vibe(true); // the unPhone's vibration motor is on a i2c GPIO expander
#endif
        if (moduleConfig.external_notification.output_vibra)
            digitalWrite(moduleConfig.external_notification.output_vibra, true);
        break;
    case 2:
        if (moduleConfig.external_notification.output_buzzer)
            digitalWrite(moduleConfig.external_notification.output_buzzer, true);
        break;
    default:
        if (output > 0)
            digitalWrite(output, (moduleConfig.external_notification.active ? true : false));
        break;
    }

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
    drv.go();
#endif
}

void ExternalNotificationModule::setExternalOff(uint8_t index)
{
    externalCurrentState[index] = 0;
    externalTurnedOn[index] = millis();

    switch (index) {
    case 1:
#ifdef UNPHONE
        unphone.vibe(false); // the unPhone's vibration motor is on a i2c GPIO expander
#endif
        if (moduleConfig.external_notification.output_vibra)
            digitalWrite(moduleConfig.external_notification.output_vibra, false);
        break;
    case 2:
        if (moduleConfig.external_notification.output_buzzer)
            digitalWrite(moduleConfig.external_notification.output_buzzer, false);
        break;
    default:
        if (output > 0)
            digitalWrite(output, (moduleConfig.external_notification.active ? false : true));
        break;
    }

#if defined(HAS_NCP5623) || defined(RGBLED_RED) || defined(HAS_NEOPIXEL) || defined(UNPHONE)
    red = 0;
    green = 0;
    blue = 0;
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
#endif

#ifdef T_WATCH_S3
    drv.stop();
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
      concurrency::OSThread("ExternalNotificationModule")
{

    /*
        Uncomment the preferences below if you want to use the module
        without having to configure it from the PythonAPI or WebUI.
    */

    moduleConfig.external_notification.alert_message = true;

    moduleConfig.external_notification.active = true;

    if (moduleConfig.external_notification.enabled) {

        // SPI.end();
        // SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
        axs15231_init();

        LOG_INFO("DOING WINDYTRON_LOGO");
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, LOW); // turn off backlight asap to minimise power on artifacts

        // lcd_setRotation(2);             // 180 degree hardware rotate if you want reset / boot buttons at the bottom
        m_sprite.createSprite(640, 180); // full screen landscape sprite in psram
        m_sprite.setSwapBytes(1);
        lcd_fill(0, 0, 180, 640, 0x00); // clear screen
        digitalWrite(TFT_BL, HIGH);     // turn on backlight

        // m_sprite.setRotation(1); // for upside down
        // logo("blah","blah
        lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t *)&gImage);
        delay(3000);
        LOG_INFO("out of setup");

        // if (!config.m_sprite.flip_screen) {
        //     m_sprite.setRotation(1 + rc);
        // } else {
        //     m_sprite.setRotation(3 + rc);
        // }
        // m_sprite.drawBitmap(0, 0, epd_bitmap_windy_tron_213_bw, 122, 250, EPD_BLACK);
        // if (!config.m_sprite.flip_screen) {
        //     m_sprite.setRotation(2 + rc);
        // } else {
        //     m_sprite.setRotation(0 + rc);
        // }

        // m_sprite.setFreeFont(&FreeMonoBold12pt7b);
        // m_sprite.setTextColor(EPD_BLACK);
        // m_sprite.setCursor(2, 16);
        // if (strlen(devicestate.owner.long_name) < 9)
        //     m_sprite.setCursor(115, 50); // set short name lower inline with the windsock
        // m_sprite.print(devicestate.owner.long_name);
        // // #ifndef LILYGOT5
        // m_sprite.display();
        // // #else
        // // m_sprite.update()
        // // #endif
        // LOG_INFO("DID EPD");

        if (!nodeDB->loadProto(rtttlConfigFile, meshtastic_RTTTLConfig_size, sizeof(meshtastic_RTTTLConfig),
                               &meshtastic_RTTTLConfig_msg, &rtttlConfig)) {
            memset(rtttlConfig.ringtone, 0, sizeof(rtttlConfig.ringtone));
            strncpy(rtttlConfig.ringtone,
                    "24:d=32,o=5,b=565:f6,p,f6,4p,p,f6,p,f6,2p,p,b6,p,b6,p,b6,p,b6,p,b,p,b,p,b,p,b,p,b,p,b,p,b,p,b,1p.,2p.,p",
                    sizeof(rtttlConfig.ringtone));
        }

        LOG_INFO("Initializing External Notification Module\n");

        output = moduleConfig.external_notification.output ? moduleConfig.external_notification.output
                                                           : EXT_NOTIFICATION_MODULE_OUTPUT;

        // Set the direction of a pin
        if (output > 0) {
            LOG_INFO("Using Pin %i in digital mode\n", output);
            pinMode(output, OUTPUT);
        }
        setExternalOff(0);
        externalTurnedOn[0] = 0;
        if (moduleConfig.external_notification.output_vibra) {
            LOG_INFO("Using Pin %i for vibra motor\n", moduleConfig.external_notification.output_vibra);
            pinMode(moduleConfig.external_notification.output_vibra, OUTPUT);
            setExternalOff(1);
            externalTurnedOn[1] = 0;
        }
        if (moduleConfig.external_notification.output_buzzer) {
            if (!moduleConfig.external_notification.use_pwm) {
                LOG_INFO("Using Pin %i for buzzer\n", moduleConfig.external_notification.output_buzzer);
                pinMode(moduleConfig.external_notification.output_buzzer, OUTPUT);
                setExternalOff(2);
                externalTurnedOn[2] = 0;
            } else {
                config.device.buzzer_gpio = config.device.buzzer_gpio ? config.device.buzzer_gpio : PIN_BUZZER;
                // in PWM Mode we force the buzzer pin if it is set
                LOG_INFO("Using Pin %i in PWM mode\n", config.device.buzzer_gpio);
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
        LOG_INFO("External Notification Module Disabled\n");
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
        if (getFrom(&mp) != nodeDB->getNodeNum()) {
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
                    LOG_INFO("externalNotificationModule - Notification Bell\n");
                    isNagging = true;
                    setExternalOn(0);
                    if (moduleConfig.external_notification.nag_timeout) {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                    } else {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                    }
                }
            }

            if (moduleConfig.external_notification.alert_bell_vibra) {
                if (containsBell) {
                    LOG_INFO("externalNotificationModule - Notification Bell (Vibra)\n");
                    isNagging = true;
                    setExternalOn(1);
                    if (moduleConfig.external_notification.nag_timeout) {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                    } else {
                        nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                    }
                }
            }

            if (moduleConfig.external_notification.alert_bell_buzzer) {
                if (containsBell) {
                    LOG_INFO("externalNotificationModule - Notification Bell (Buzzer)\n");
                    isNagging = true;
                    if (!moduleConfig.external_notification.use_pwm) {
                        setExternalOn(2);
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
                    LOG_INFO("DISPLAY_TEXT");
                    displayText(mp);
                }

                isNagging = true;
                setExternalOn(0);
                if (moduleConfig.external_notification.nag_timeout) {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                } else {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                }
            }

            if (moduleConfig.external_notification.alert_message_vibra) {
                LOG_INFO("externalNotificationModule - Notification Module (Vibra)\n");
                isNagging = true;
                setExternalOn(1);
                if (moduleConfig.external_notification.nag_timeout) {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.nag_timeout * 1000;
                } else {
                    nagCycleCutoff = millis() + moduleConfig.external_notification.output_ms;
                }
            }

            if (moduleConfig.external_notification.alert_message_buzzer) {
                LOG_INFO("externalNotificationModule - Notification Module (Buzzer)\n");
                isNagging = true;
                if (!moduleConfig.external_notification.use_pwm && !moduleConfig.external_notification.use_i2s_as_buzzer) {
                    setExternalOn(2);
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
        LOG_INFO("External Notification Module Disabled or muted\n");
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
        LOG_INFO("Client is getting ringtone\n");
        this->handleGetRingtone(mp, response);
        result = AdminMessageHandleResult::HANDLED_WITH_RESPONSE;
        break;

    case meshtastic_AdminMessage_set_ringtone_message_tag:
        LOG_INFO("Client is setting ringtone\n");
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
    LOG_INFO("*** handleGetRingtone\n");
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
        LOG_INFO("*** from_msg.text:%s\n", from_msg);
    }

    if (changed) {
        nodeDB->saveProto(rtttlConfigFile, meshtastic_RTTTLConfig_size, &meshtastic_RTTTLConfig_msg, &rtttlConfig);
    }
}

void ExternalNotificationModule::displayWind(const meshtastic_MeshPacket &mp)
{

    // m_sprite.clearBuffer();
    lcd_fill(0, 0, 180, 640, 0x00);
    auto &p = mp.decoded;
    char msg[70] = "";
    sprintf(msg, "%s", p.payload.bytes);
    if (strcmp(msg, last_data) == 0)
        return; // don't re-display duplicate info
    strcpy(last_data, msg);

    // single style
    // NE 51 20g25 , AUX1_AUX2 - 2021-06-29T16:10:07
    // dual style
    // 16:10 NE 51 20g25 , AUX1_AUX2 -- E 91 30g35 , AUX21_AUX22
    char data[70]; // Adjust the size according to your needs

    // Copy the content of msg to data
    strcpy(data, msg);

    // Tokenize the data using strtok
    char *token = strtok(data, " ");
    char *dir = token;
    token = strtok(NULL, " ");
    int degree = atoi(token);
    token = strtok(NULL, "g");
    int avg = atoi(token);
    token = strtok(NULL, " ");
    int gust = atoi(token);

    char avg_g_gust[10]; // Adjust the size according to your needs
    sprintf(avg_g_gust, " %dg%d ", avg, gust);

    token = strtok(NULL, " "); // throwaway the comma surrounded by spaces
    // Continue tokenization for AUX1 and AUX2
    token = strtok(NULL, "_");
    char aux1[24] = "."; // Assuming aux1 can be a maximum of 23 characters
    if (token != nullptr) {
        strncpy(aux1, token, 23);
        aux1[23] = '\0'; // Null-terminate the aux1 string
    }

    // Tokenize again to get AUX2
    token = strtok(NULL, " -");
    char aux2[24] = "."; // Assuming aux2 can be a maximum of 23 characters
    if (token != nullptr) {
        strncpy(aux2, token, 23);
        aux2[23] = '\0'; // Null-terminate the aux2 string
    }

    int y_offset = 0;
    // we can move all fields down and display a long label at the very top.
    if (aux2[0] == '.') {
        y_offset = 22;
        m_sprite.setFreeFont(&FreeMonoBold12pt7b);
        m_sprite.setCursor(5, 16); // put this at the top because verything else is moved down.
        // m_sprite.setTextColor(EPD_BLACK);
        m_sprite.print(devicestate.owner.long_name);
    } else {
        // DISPLAY LABEL normall but shorten if too long TODO
        m_sprite.setFreeFont(&FreeMonoBold12pt7b);
        m_sprite.setCursor(75, 16);
        // m_sprite.setTextColor(EPD_BLACK);
        if (strlen(devicestate.owner.long_name) < 9) {
            m_sprite.print(devicestate.owner.long_name); // maximum 6
        } else {
            m_sprite.print(devicestate.owner.short_name); // maximum 6
        }
    }
    // DISPLAY THE TIMESTAMP
    // Find the position of the 'T' character
    const char *timeStart = strstr(msg, "T");
    // Create a buffer to store the time
    char timeBuffer[6];
    if (timeStart != nullptr) {
        // Move one position ahead to point to the start of the time (hour)
        timeStart += 1;

        // Create a buffer to store the time
        char timeBuffer[6];

        // Copy the time to the buffer
        strncpy(timeBuffer, timeStart, 5);

        // Null-terminate the buffer
        timeBuffer[5] = '\0';
    }

    m_sprite.setFreeFont(&FreeMonoBold12pt7b);
    m_sprite.setCursor(180, 18 + y_offset);
    // m_sprite.setTextColor(EPD_BLACK);
    m_sprite.setTextWrap(false);
    m_sprite.print(timeBuffer);

    // m_sprite.setFreeFont(&FreeMonoBold12pt7b);
    // m_sprite.setCursor(85, 16);
    // m_sprite.setTextColor(EPD_BLACK);
    // m_sprite.print(s);

    // DISPLAY VELOCITY
    m_sprite.setFreeFont(&FreeMonoBold24pt7b);
    m_sprite.setCursor(60, 60 + y_offset);
    // m_sprite.setTextColor(EPD_BLACK);
    m_sprite.print(avg_g_gust);
    m_sprite.setFreeFont(&FreeMonoBold9pt7b);

    // DISPLAY DIR
    m_sprite.setFreeFont(&FreeMonoBold18pt7b);
    m_sprite.setCursor(5, 30 + y_offset);
    // m_sprite.setTextColor(EPD_BLACK);
    m_sprite.print(dir);
    m_sprite.setCursor(5, 60 + y_offset);
    m_sprite.print(degree);

    // DISPLAY AUX1
    m_sprite.setFreeFont(&FreeMonoBold12pt7b);
    m_sprite.setCursor(10, 90 + y_offset);
    // m_sprite.setTextColor(EPD_BLACK);
    m_sprite.print(aux1);

    //  DISPLAY AUX2
    m_sprite.setFreeFont(&FreeMonoBold12pt7b);
    m_sprite.setCursor(10, 117 + y_offset);
    // m_sprite.setTextColor(EPD_BLACK);
    m_sprite.print(aux2);

    // m_sprite.display();
    lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t *)m_sprite.getPointer());

    // void show_data(String data)  {
    //   // parse the message as json
    //   if (last_message.equals(data)) return;
    //   StaticJsonDocument<512> doc;
    //   deserializeJson(doc, data.c_str());
    //   // json data looks like : {"avg": 7, "gust": 10, "lull": 4, "dir_card": "WSW", "dir_deg": 257, "stamp":
    //   "2023-10-21T20:37:44", "aux1": "2.6f,10s,N357", "aux2": "0907H2.2", "label": "Kanaha"} m_sprite.clearBuffer(); String
    //   stamp = doc["stamp"]; String hour = stamp.substring(stamp.indexOf('T')+1,stamp.indexOf(':')); String min =
    //   stamp.substring(stamp.indexOf(':')+1,stamp.indexOf(':')+3); String avg_g_gust = String(doc["avg"]) + "g" +
    //   String(doc["gust"]); display_location(String(doc["label"]));

    //   display_dir(doc["dir_card"],doc["dir_deg"]);
    //   display_vel(avg_g_gust);
    //   display_time(min.toInt(),hour.toInt());
    //   display_aux1(doc["aux1"]);
    //   display_aux2(doc["aux2"]);
    //   m_sprite.display();
    //   last_message = String(data);
    // }
}
void ExternalNotificationModule::displayText(const meshtastic_MeshPacket &mp)
{

    // m_sprite.clearBuffer();
    lcd_fill(0, 0, 180, 640, 0x00);
    auto &p = mp.decoded;
    static char msg[256];
    sprintf(msg, "%s", p.payload.bytes);
    if (strcmp(msg, last_data) == 0)
        return; // don't re-display duplicate info
    strcpy(last_data, msg);

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
