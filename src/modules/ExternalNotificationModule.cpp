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
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>

#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#define NEOMATRIX
// ######NEO_MATRIX_BLOCK######################################
#ifdef NEOMATRIX
#define M_WIDTH     32
#define M_HEIGHT    8
#define NUM_LEDS    (M_WIDTH * M_HEIGHT)
#define DATA_PIN 2
Adafruit_NeoMatrix m_matrix = Adafruit_NeoMatrix(M_WIDTH, M_HEIGHT, DATA_PIN,
		NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
		NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
		NEO_GRB + NEO_KHZ800);
// Color definitions
#define BLACK    0x0000
#define BLUE     0x001F
#define RED      0xF800
#define GREEN    0x07E0
#define CYAN     0x07DD
#define MAGENTA  0xF81F
#define YELLOW   0xFFE0 
#define WHITE    0xFFFF
#define UPSIDE_DOWN 0
// directions codes.  

//const unsigned short m_N[256] = {};
uint8_t  m_NE1[4] = { 5, 1, 5,0 }; // 
uint8_t  m_NE2[4] = { 5, 1, 6,0 }; // 
uint8_t  m_NE3[4] = { 5, 1, 7,0 }; // 
uint8_t  m_NE4[4] = { 5, 1, 7,1 }; // 
uint8_t  m_NE5[4] = { 5, 1, 7,2 }; //

// since the center square is always in the same spot we only need to keep track of the second square's location.
uint8_t  m_N[4] =   {3,1,0,0}; // N = !
uint8_t  m_NNE[4] = {4,1,0,0}; // #
uint8_t  m_NW [4] = {1,1,0,0}; // $
uint8_t  m_NNW[4] = {2,1,0,0}; // %
uint8_t  m_E  [4] = {5,3,0,0}; // ^
uint8_t  m_ENE[4] = {5,2,0,0}; // &
uint8_t  m_ESE[4] = {5,4,0,0}; // *
uint8_t  m_WSW[4] = {1,4,0,0}; // (
uint8_t  m_S  [4] = {3,5,0,0}; // )
uint8_t  m_SE [4] = {5,5,0,0}; // _
uint8_t  m_SSE[4] = {4,5,0,0}; // +
uint8_t  m_SW [4] = {1,5,0,0}; // 
uint8_t  m_SSW[4] = {2,5,0,0}; // 
uint8_t  m_W  [4] = {1,3,0,0}; // 
uint8_t  m_WNW[4] = {1,2,0,0}; // 

#endif

#ifdef HAS_NCP5623
#include <graphics/RAKled.h>

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
            for (int i = 0; i < 2; i++) {
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
#ifdef HAS_NCP5623
            if (rgb_found.type == ScanI2C::NCP5623) {
                red = (colorState & 4) ? brightnessValues[brightnessIndex] : 0;          // Red enabled on colorState = 4,5,6,7
                green = (colorState & 2) ? brightnessValues[brightnessIndex] : 0;        // Green enabled on colorState = 2,3,6,7
                blue = (colorState & 1) ? (brightnessValues[brightnessIndex] * 1.5) : 0; // Blue enabled on colorState = 1,3,5,7
                rgb.setColor(red, green, blue);

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

#ifdef HAS_NCP5623
    if (rgb_found.type == ScanI2C::NCP5623) {
        red = 0;
        green = 0;
        blue = 0;
        rgb.setColor(red, green, blue);
    }
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

    // moduleConfig.external_notification.alert_message = true;
    // moduleConfig.external_notification.alert_message_buzzer = true;
    // moduleConfig.external_notification.alert_message_vibra = true;
    // moduleConfig.external_notification.use_i2s_as_buzzer = true;

    // moduleConfig.external_notification.active = true;
    // moduleConfig.external_notification.alert_bell = 1;
    // moduleConfig.external_notification.output_ms = 1000;
    // moduleConfig.external_notification.output = 4; // RAK4631 IO4
    // moduleConfig.external_notification.output_buzzer = 10; // RAK4631 IO6
    // moduleConfig.external_notification.output_vibra = 28; // RAK4631 IO7
    // moduleConfig.external_notification.nag_timeout = 300;

    // T-Watch / T-Deck i2s audio as buzzer:
    // moduleConfig.external_notification.enabled = true;
    // moduleConfig.external_notification.nag_timeout = 300;
    // moduleConfig.external_notification.output_ms = 1000;
    // moduleConfig.external_notification.use_i2s_as_buzzer = true;
    // moduleConfig.external_notification.alert_message_buzzer = true;

    if (moduleConfig.external_notification.enabled) {

#ifdef NEOMATRIX 
        m_matrix.begin();
	    m_matrix.setTextWrap(false);

    	set_brightness(8); // set real low during boot up to try to avoid brownouts

        if (UPSIDE_DOWN) m_matrix.setRotation(2);

        m_matrix.print("MauiMesh");
        LOG_INFO("DID Bootup Splass");
#endif

        if (!nodeDB.loadProto(rtttlConfigFile, meshtastic_RTTTLConfig_size, sizeof(meshtastic_RTTTLConfig),
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
    } else {
        LOG_INFO("External Notification Module Disabled\n");
        disable();
    }
}

ProcessMessage ExternalNotificationModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    if (moduleConfig.external_notification.enabled) {
#ifdef T_WATCH_S3
        drv.setWaveform(0, 75);
        drv.setWaveform(1, 56);
        drv.setWaveform(2, 0);
        drv.go();
#endif
        if (getFrom(&mp) != nodeDB.getNodeNum()) {
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
#ifdef NEOMATRIX
                if (mp.from == 0xa3251978) {
                    LOG_INFO("DISPLAY_WIND");
                    displayWind(mp);
                    return ProcessMessage::STOP; // Just display and then stop.
                    
                } else {
                    LOG_INFO("DISPLAY_TEXT");
                    displayText(mp);
                }
#endif
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
        LOG_INFO("External Notification Module Disabled\n");
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
        nodeDB.saveProto(rtttlConfigFile, meshtastic_RTTTLConfig_size, &meshtastic_RTTTLConfig_msg, &rtttlConfig);
    }
}

void ExternalNotificationModule::displayWind(const meshtastic_MeshPacket &mp)
{

    m_matrix.clear();
    auto &p = mp.decoded;
    static char msg[70];
    sprintf(msg, "%s", p.payload.bytes);
    if (strcmp(msg, last_data) == 0 ) return; // don't re-display duplicate info
    strcpy(last_data, msg);
    
    // parse the wind string
    // NE 51 20g25 , AUX1_AUX2 - 2021-06-29T16:10:07
    char data[70];  // Adjust the size according to your needs

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

    char avg_g_gust[10];  // Adjust the size according to your needs
    sprintf(avg_g_gust, " %dg%d ", avg, gust);

    token = strtok(NULL, " ");  // throwaway the comma surrounded by spaces
    // Continue tokenization for AUX1 and AUX2
    token = strtok(NULL, "_");
    char aux1[24] = "-";  // Assuming aux1 can be a maximum of 23 characters
    if (token != nullptr) {
        strncpy(aux1, token, 23);
        aux1[23] = '\0';  // Null-terminate the aux1 string
    }
    
    // Tokenize again to get AUX2
    token = strtok(NULL, " -");
    char aux2[24] = "-";  // Assuming aux2 can be a maximum of 23 characters
    if (token != nullptr) {
        strncpy(aux2, token, 23);
        aux2[23] = '\0';  // Null-terminate the aux2 string
    }


    if (avg <= 15) m_matrix.setTextColor(BLUE);
    if (avg >= 15) m_matrix.setTextColor(CYAN);
    if (avg >= 25) m_matrix.setTextColor(GREEN);
    if (avg >= 30) m_matrix.setTextColor(MAGENTA);
    if (avg >= 35) m_matrix.setTextColor(RED);

    // DISPLAY VELOCITY
    m_matrix.print(avg_g_gust);
    m_matrix.show()();
    delay(2000)
  
    // DISPLAY DIR
    m_matrix.print(dir);
    m_matrix.print(degree);
    m_matrix.show()();  
    delay(2000);

    // DISPLAY AUX1
    m_matrix.print(aux1); // needs animation
    m_matrix.show()();
    
    char *m_dir_color = RED;    // default is red, most cases will be bad, we'll change them for when it's good.
	uint8_t *m_dir = m_N;
    if (strcmp(dir, "N") == 0) {
		m_dir_color = CYAN;
		m_dir = m_N;
	}
	else if (strcmp(dir, "NE") == 0) {
		m_dir_color = GREEN;
		//spl("dir is NE");
		// now lets check for degrees
		if (33 <= degree && degree <= 37) {
			//spl("almost NNE");
			m_dir =  m_NE1;
		}
		else if (38 <= degree && degree <= 42) {
			//spl("more north");
			m_dir =  m_NE2;
		}
		else if (43 <= degree && degree <= 47) {
			//spl("perfect NE");
			m_dir =  m_NE3;
		}
		else if (48 <= degree && degree <= 52) {
			//spl("more east");
			m_dir =  m_NE4;
		}
		else if (51 <= degree && degree <= 56) {
			//spl("almost ENE");
			m_dir =  m_NE5;
		}
		else {
			m_dir =  m_NE3;
			//spl("doing default");

		}
	}
	else if (strcmp(dir, "NNE") == 0) {
		m_dir_color = CYAN;
		m_dir =  m_NNE;
	}
	else if (strcmp(dir, "NW") == 0) {
		m_dir =  m_NW;
	}
	else if (strcmp(dir, "NNW") == 0) {
		m_dir =  m_NNW;
	}
	else if (strcmp(dir, "ENE") == 0) {
		m_dir_color = YELLOW;
		m_dir =  m_ENE;
	}
	else if (strcmp(dir, "E") == 0) {
		m_dir =  m_E;
	}
	else if (strcmp(dir, "ESE") == 0) {
		m_dir =  m_ESE;
	}
	else if (strcmp(dir, "WSW") == 0) {
		m_dir =  m_WSW;
	}
	else if (strcmp(dir, "S") == 0) {
		m_dir =  m_S;
	}
	else if (strcmp(dir, "SE") == 0) {
		m_dir =  m_SE;
	}
	else if (strcmp(dir, "SSE") == 0) {
		m_dir =  m_SSE;
	}
	else if (strcmp(dir, "SW") == 0) {
		m_dir =  m_SW;
	}
	else if (strcmp(dir, "SSW") == 0) {
		m_dir =  m_SSW;
	}
	else if (strcmp(dir, "W") == 0) {
		m_dir =  m_W;
	}
	else if (strcmp(dir, "WNW") == 0) {
		m_dir =  m_WNW;
	}
	else {
		m_dir_color = BLUE;
		m_dir =  m_N;
	}

    m_matrix.drawRect(3, 3, 2, 2, color);    // first draw center square it's always in the same place
	m_matrix.drawRect(d[0], d[1], 2, 2, color);  // next draw the direcction square
	if (!(d[2] == 0 && d[3] == 0)) {   // if we have non zero values for the extra pixel, then draw it
		spl("doing extra dir pixel");
		m_matrix.drawPixel(d[2], d[3], color);  // finally draw the pixel if we are in NE territory
	}
	// call show now or later ?
	m_matrix.show();


}
void ExternalNotificationModule::displayText(const meshtastic_MeshPacket &mp)
{

    auto &p = mp.decoded;
    static char msg[256];
    sprintf(msg, "%s", p.payload.bytes);
    if (strcmp(msg, last_data) == 0 ) return; // don't re-display duplicate info
    strcpy(last_data, msg);
    

#ifdef NEOMATRIX

    m_matrix.clear();
	int slength = len(msg);


	if (slength < 8) {
    	int cposx = M_WIDTH / 2 - width(s) / 2;
		m_matrix.setCursor(cposx, 0);
		m_matrix.print(msg);
		m_matrix.show();
	}
	else {
		m_matrix.setCursor(M_WIDTH, 0);
		m_matrix.print(msg);
		m_matrix.show();
		// animate the scroll to the left
		int x = M_WIDTH;
		int pass = 0;
		while (--x > -width(msg)) {
			m_matrix.fillScreen(0);
			m_matrix.setCursor(x, 0);
			m_matrix.print(msg);
			m_matrix.show();
			delay(50);
		}
	}

#endif

}