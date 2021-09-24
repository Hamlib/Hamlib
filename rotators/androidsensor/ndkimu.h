#ifndef NDKIMU_H
#define NDKIMU_H

#include <dlfcn.h>
#include <GLES2/gl2.h>

#include <android/looper.h>
#include <android/sensor.h>

#include <cstdint>
#include <cassert>
#include <string>


static const int LOOPER_ID_USER = 3;
static const int SENSOR_HISTORY_LENGTH = 100;
static const int SENSOR_REFRESH_RATE_HZ = 100;
static constexpr int32_t SENSOR_REFRESH_PERIOD_US = int32_t(1000000 / SENSOR_REFRESH_RATE_HZ);
static const float SENSOR_FILTER_ALPHA = 0.1f;

class NdkImu
{
public:
    NdkImu(char kPackageName[] = NULL);
    ~NdkImu();
    static ASensorManager *AcquireASensorManagerInstance(char kPackageName[]);
    static bool getRotationMatrix(float R[], size_t Rlength, float I[], size_t Ilength, float gravity[], float geomagnetic[]);
    static float *getOrientation(float R[], size_t Rlength, float valuesBuf[] = NULL);

private:
    ASensorManager *sensorManager;
    const ASensor *accelerometer;
    ASensorEventQueue *accelerometerEventQueue;
    const ASensor *geomagnetic;
    ASensorEventQueue *geomagneticEventQueue;
    ALooper *looper;

    struct SensorData {
        GLfloat x;
        GLfloat y;
        GLfloat z;
    };
    SensorData sensorAccData[SENSOR_HISTORY_LENGTH*2];
    SensorData sensorAccDataFilter;
    SensorData sensorMagData[SENSOR_HISTORY_LENGTH*2];
    SensorData sensorMagDataFilter;

    int sensorAccDataIndex;
    int sensorMagDataIndex;

public:
    void update();
    float *getAccData(float accDataBuf[] = NULL);
    float *getMagData(float magDataBuf[] = NULL);
};

#endif // NDKIMU_H
