#include "ndkimu.h"

#include <cmath>

NdkImu::NdkImu(char kPackageName[])
{
    sensorManager = AcquireASensorManagerInstance(kPackageName);
    assert(sensorManager != NULL);
    accelerometer = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ACCELEROMETER);
    geomagnetic = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_MAGNETIC_FIELD);
    assert(accelerometer != NULL);
    looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    assert(looper != NULL);
    accelerometerEventQueue = ASensorManager_createEventQueue(sensorManager, looper,
                                                              LOOPER_ID_USER, NULL, NULL);
    geomagneticEventQueue = ASensorManager_createEventQueue(sensorManager, looper,
                                                            LOOPER_ID_USER, NULL, NULL);
    assert(accelerometerEventQueue != NULL);
    auto status = ASensorEventQueue_enableSensor(accelerometerEventQueue,
                                                 accelerometer);
    assert(status >= 0);

    status = ASensorEventQueue_setEventRate(accelerometerEventQueue,
                                            accelerometer,
                                            SENSOR_REFRESH_PERIOD_US);
    assert(status >= 0);

    status = ASensorEventQueue_enableSensor(geomagneticEventQueue,
                                            geomagnetic);
    assert(status >= 0);

    status = ASensorEventQueue_setEventRate(geomagneticEventQueue,
                                            geomagnetic,
                                            SENSOR_REFRESH_PERIOD_US);
    assert(status >= 0);
}

NdkImu::~NdkImu()
{
    auto status = ASensorEventQueue_disableSensor(accelerometerEventQueue,
                                                  accelerometer);
    assert(status >= 0);

    status = ASensorEventQueue_disableSensor(geomagneticEventQueue,
                                             geomagnetic);
    assert(status >= 0);
}

ASensorManager *NdkImu::AcquireASensorManagerInstance(char kPackageName[])
{
    typedef ASensorManager *(*PF_GETINSTANCEFORPACKAGE)(const char *name);
    void* androidHandle = dlopen("libandroid.so", RTLD_NOW);
    PF_GETINSTANCEFORPACKAGE getInstanceForPackageFunc = (PF_GETINSTANCEFORPACKAGE)
            dlsym(androidHandle, "ASensorManager_getInstanceForPackage");
    if (getInstanceForPackageFunc) {
        return getInstanceForPackageFunc(kPackageName);
    }

    typedef ASensorManager *(*PF_GETINSTANCE)();
    PF_GETINSTANCE getInstanceFunc = (PF_GETINSTANCE)
            dlsym(androidHandle, "ASensorManager_getInstance");
    // by all means at this point, ASensorManager_getInstance should be available
    assert(getInstanceFunc);
    return getInstanceFunc();
}

/*
 * https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/hardware/SensorManager.java
 * blob: 9e78a6bb476061b4e20378ba20a00a2761e1ed7e
 * line 1174
 */
bool NdkImu::getRotationMatrix(float R[], size_t Rlength, float I[], size_t Ilength, float gravity[], float geomagnetic[])
{
    // TODO: move this to native code for efficiency
    float Ax = gravity[0];
    float Ay = gravity[1];
    float Az = gravity[2];
    const float normsqA = (Ax * Ax + Ay * Ay + Az * Az);
    const float g = 9.81f;
    const float freeFallGravitySquared = 0.01f * g * g;
    if (normsqA < freeFallGravitySquared) {
        // gravity less than 10% of normal value
        return false;
    }
    const float Ex = geomagnetic[0];
    const float Ey = geomagnetic[1];
    const float Ez = geomagnetic[2];
    float Hx = Ey * Az - Ez * Ay;
    float Hy = Ez * Ax - Ex * Az;
    float Hz = Ex * Ay - Ey * Ax;
    const float normH = (float) std::sqrt(Hx * Hx + Hy * Hy + Hz * Hz);
    if (normH < 0.1f) {
        // device is close to free fall (or in space?), or close to
        // magnetic north pole. Typical values are  > 100.
        return false;
    }
    const float invH = 1.0f / normH;
    Hx *= invH;
    Hy *= invH;
    Hz *= invH;
    const float invA = 1.0f / (float) std::sqrt(Ax * Ax + Ay * Ay + Az * Az);
    Ax *= invA;
    Ay *= invA;
    Az *= invA;
    const float Mx = Ay * Hz - Az * Hy;
    const float My = Az * Hx - Ax * Hz;
    const float Mz = Ax * Hy - Ay * Hx;
    if (R != NULL) {
        if (Rlength == 9) {
            R[0] = Hx;     R[1] = Hy;     R[2] = Hz;
            R[3] = Mx;     R[4] = My;     R[5] = Mz;
            R[6] = Ax;     R[7] = Ay;     R[8] = Az;
        } else if (Rlength == 16) {
            R[0]  = Hx;    R[1]  = Hy;    R[2]  = Hz;   R[3]  = 0;
            R[4]  = Mx;    R[5]  = My;    R[6]  = Mz;   R[7]  = 0;
            R[8]  = Ax;    R[9]  = Ay;    R[10] = Az;   R[11] = 0;
            R[12] = 0;     R[13] = 0;     R[14] = 0;    R[15] = 1;
        }
    }
    if (I != NULL) {
        // compute the inclination matrix by projecting the geomagnetic
        // vector onto the Z (gravity) and X (horizontal component
        // of geomagnetic vector) axes.
        const float invE = 1.0f / (float) std::sqrt(Ex * Ex + Ey * Ey + Ez * Ez);
        const float c = (Ex * Mx + Ey * My + Ez * Mz) * invE;
        const float s = (Ex * Ax + Ey * Ay + Ez * Az) * invE;
        if (Ilength == 9) {
            I[0] = 1;     I[1] = 0;     I[2] = 0;
            I[3] = 0;     I[4] = c;     I[5] = s;
            I[6] = 0;     I[7] = -s;     I[8] = c;
        } else if (Ilength == 16) {
            I[0] = 1;     I[1] = 0;     I[2] = 0;
            I[4] = 0;     I[5] = c;     I[6] = s;
            I[8] = 0;     I[9] = -s;     I[10] = c;
            I[3] = I[7] = I[11] = I[12] = I[13] = I[14] = 0;
            I[15] = 1;
        }
    }
    return true;
}

/*
 * https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/hardware/SensorManager.java
 * blob: 9e78a6bb476061b4e20378ba20a00a2761e1ed7e
 * line 1469
 */
float *NdkImu::getOrientation(float R[], size_t Rlength, float valuesBuf[])
{
    if(valuesBuf == NULL)
        valuesBuf = (float*)malloc(sizeof (float)*3);

    if (Rlength == 9) {
        valuesBuf[0] = (float) std::atan2(R[1], R[4]);
        valuesBuf[1] = (float) std::asin(-R[7]);
        valuesBuf[2] = (float) std::atan2(-R[6], R[8]);
    } else {
        valuesBuf[0] = (float) std::atan2(R[1], R[5]);
        valuesBuf[1] = (float) std::asin(-R[9]);
        valuesBuf[2] = (float) std::atan2(-R[8], R[10]);
    }
    return valuesBuf;
}

void NdkImu::update()
{
    ALooper_pollAll(0, NULL, NULL, NULL);
    ASensorEvent event;
    const float a = SENSOR_FILTER_ALPHA;

    while (ASensorEventQueue_getEvents(accelerometerEventQueue, &event, 1) > 0) {
        sensorAccDataFilter.x = a * event.acceleration.x + (1.0f - a) * sensorAccDataFilter.x;
        sensorAccDataFilter.y = a * event.acceleration.y + (1.0f - a) * sensorAccDataFilter.y;
        sensorAccDataFilter.z = a * event.acceleration.z + (1.0f - a) * sensorAccDataFilter.z;
    }
    sensorAccData[sensorAccDataIndex] = sensorAccDataFilter;
    sensorAccData[SENSOR_HISTORY_LENGTH+sensorAccDataIndex] = sensorAccDataFilter;
    sensorAccDataIndex = (sensorAccDataIndex + 1) % SENSOR_HISTORY_LENGTH;

    while (ASensorEventQueue_getEvents(geomagneticEventQueue, &event, 1) > 0) {
        sensorMagDataFilter.x = a * event.magnetic.x + (1.0f - a) * sensorMagDataFilter.x;
        sensorMagDataFilter.y = a * event.magnetic.y + (1.0f - a) * sensorMagDataFilter.y;
        sensorMagDataFilter.z = a * event.magnetic.z + (1.0f - a) * sensorMagDataFilter.z;
    }
    sensorMagData[sensorMagDataIndex] = sensorMagDataFilter;
    sensorMagData[SENSOR_HISTORY_LENGTH+sensorMagDataIndex] = sensorMagDataFilter;
    sensorMagDataIndex = (sensorMagDataIndex + 1) % SENSOR_HISTORY_LENGTH;
}

float *NdkImu::getAccData(float accDataBuf[])
{
    if(accDataBuf == NULL)
        accDataBuf = (float*)malloc(sizeof (float)*3);
    accDataBuf[0] = sensorAccDataFilter.x;
    accDataBuf[1] = sensorAccDataFilter.y;
    accDataBuf[2] = sensorAccDataFilter.z;
    return accDataBuf;
}

float *NdkImu::getMagData(float magDataBuf[])
{
    if(magDataBuf == NULL)
        magDataBuf = (float*)malloc(sizeof (float)*3);
    magDataBuf[0] = sensorMagDataFilter.x;
    magDataBuf[1] = sensorMagDataFilter.y;
    magDataBuf[2] = sensorMagDataFilter.z;
    return magDataBuf;
}
