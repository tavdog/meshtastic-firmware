#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "DFRobotLarkSensor.h"
#include "TelemetrySensor.h"
#include "gps/GeoCoord.h"
#include <DFRobot_LarkWeatherStation.h>
#include <string>

DFRobotLarkSensor::DFRobotLarkSensor() : TelemetrySensor(meshtastic_TelemetrySensorType_DFROBOT_LARK, "DFROBOT_LARK") {}

int32_t DFRobotLarkSensor::runOnce()
{
    LOG_INFO("Init sensor: %s\n", sensorName);
    if (!hasSensor()) {
        return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
    }

    lark = DFRobot_LarkWeatherStation_I2C(nodeTelemetrySensorsMap[sensorType].first, nodeTelemetrySensorsMap[sensorType].second);

    if (lark.begin() == 0) // DFRobotLarkSensor init
    {
        LOG_DEBUG("DFRobotLarkSensor Init Succeed\n");
        status = true;
    } else {
        LOG_ERROR("DFRobotLarkSensor Init Failed\n");
        status = false;
    }
    return initI2CSensor();
}

void DFRobotLarkSensor::setup() {}

float wind_vel_sum = 0;
float wind_vel_max = 0;
float wind_vel_min = 100;
float dir_sum_sin = 0.0;
float dir_sum_cos = 0.0;
uint16_t num_readings = 0;
bool DFRobotLarkSensor::getMetrics(meshtastic_Telemetry *measurement)
{
    // calculate sums and max and min (gust,lull)
    float vel_inst = lark.getValue("Speed").toFloat();
    if (wind_vel_max < vel_inst)
        wind_vel_max = vel_inst;
    if (wind_vel_min > vel_inst)
        wind_vel_min = vel_inst;
    wind_vel_sum = wind_sum + vel_inst;
    double radians = toRadians(GeoCoord::bearingToDegrees(lark.getValue("Dir").c_str()));
    dir_sum_sin += sin(radians);
    dir_sum_cos += cos(radians);
    num_readings++;

    measurement->variant.environment_metrics.temperature = lark.getValue("Temp").toFloat();
    measurement->variant.environment_metrics.relative_humidity = lark.getValue("Humi").toFloat();
    measurement->variant.environment_metrics.wind_speed = lark.getValue("Speed").toFloat();
    measurement->variant.environment_metrics.wind_direction = GeoCoord::bearingToDegrees(lark.getValue("Dir").c_str());
    measurement->variant.environment_metrics.barometric_pressure = lark.getValue("Pressure").toFloat();

    LOG_INFO("Temperature: %f\n", measurement->variant.environment_metrics.temperature);
    LOG_INFO("Humidity: %f\n", measurement->variant.environment_metrics.relative_humidity);
    LOG_INFO("Wind Speed: %f\n", measurement->variant.environment_metrics.wind_speed);
    LOG_INFO("Wind Direction: %d\n", measurement->variant.environment_metrics.wind_direction);
    LOG_INFO("Barometric Pressure: %f\n", measurement->variant.environment_metrics.barometric_pressure);

    return true;
}

bool DFRobotLarkSensor::getMetricsAverge(meshtastic_Telemetry *measurement)
{
    measurement->variant.environment_metrics.temperature = lark.getValue("Temp").toFloat();
    measurement->variant.environment_metrics.relative_humidity = lark.getValue("Humi").toFloat();
    measurement->variant.environment_metrics.barometric_pressure = lark.getValue("Pressure").toFloat();
    measurement->variant.environment_metrics.wind_speed_max = wind_vel_max;
    measurement->variant.environment_metrics.wind_speed_min = wind_vel_min;

    double avgSin = wind_sum_sin / numReadings;
    double avgCos = wind_sum_cos / numReadings;

    double avgRadians = atan2(avgSin, avgCos);
    double avgDegrees = toDegrees(avgRadians);

    if (avgDegrees < 0) {
        avgDegrees += 360.0;
    }
    measurement->variant.environment_metrics.wind_direction = avgDegrees;
    measurement->variant.environment_metrics.wind_speed = wind_vel_sum / num_readings;

    LOG_INFO("Temperature: %f\n", measurement->variant.environment_metrics.temperature);
    LOG_INFO("Humidity: %f\n", measurement->variant.environment_metrics.relative_humidity);
    LOG_INFO("Wind Speed Average: %f\n", measurement->variant.environment_metrics.wind_speed);
    LOG_INFO("Wind Speed Max: %d\n", measurement->variant.environment_metrics.wind_speed_max);
    LOG_INFO("Wind Speed Min: %d\n", measurement->variant.environment_metrics.wind_speed_min);
    LOG_INFO("Wind Direction Average: %d\n", measurement->variant.environment_metrics.wind_direction);
    LOG_INFO("Barometric Pressure: %f\n", measurement->variant.environment_metrics.barometric_pressure);

    // reset counters
    wind_vel_sum = 0;
    wind_vel_max = 0;
    wind_vel_min = 100;
    dir_sum_sin = 0.0;
    dir_sum_cos = 0.0;
    num_readings = 0;

    return true;
}

#endif