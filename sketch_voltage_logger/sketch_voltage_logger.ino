
#define XIAO false
#if XIAO

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#else

#include <ArduinoBLE.h>
#include <LSM6DS3.h>
#include <Math.h>
#include "Quaternion.h"
// #include "Vector.h"

#endif

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "a9438aed-c675-4794-8cf5-0f993db8d262"
#define LOGGER_CHARACTERISTIC_UUID "c0ddd532-528b-4860-ac19-f093a27df43a"
#define COMMAND_CHARACTERISTIC_UUID "66b4f1b1-9e00-440c-a9a4-6bc688872af1"
#define COMMAND_CHARACTERISTIC_UUID "66b4f1b1-9e00-440c-a9a4-6bc688872af1"
#define READDATA_CHARACTERISTIC_UUID "9f601454-b36a-446d-92dc-839d4393f954"

#define BLE_LOCAL_NAME "VoltageLogger"

const int SENSOR_READ_VOLT = 0;

const float FPS = 60.f;

const float ONE_FRAME_MS = (1.f / FPS) * 1000.f;

const float TO_VOLT = (3.3f / 1024.f) * 2.f;

LSM6DS3 IMU(I2C_MODE, 0x6A);

class ReadVoltCache
{
  public:

  static constexpr int DATA_CHUNK_MAX = 64;
  static constexpr int READ_DATA_MAX = 244;
  static constexpr int FLOAT_READ_DATA_MAX = READ_DATA_MAX / 4; // 61
  static constexpr int FLOAT_DATA_MAX = FLOAT_READ_DATA_MAX * DATA_CHUNK_MAX;

  int voltdataCount{0};

  int startVoltdataCount{0};

  int endVoltdataCount{0};

  float voltData[FLOAT_DATA_MAX] = {0.f};

  void setReadChunk(int inReadChunkCount)
  {
    readChunkCount = inReadChunkCount;
  }

  int getReadChunk()
  {
    return readChunkCount;
  }

  void setVolt(float volt)
  {
    /*
    if (voltData[voltdataCount] < 0.1f && volt > 1.f)
    {
      startVoltdataCount = voltdataCount;
    }
    else if (volt < 0.1f && voltData[voltdataCount] > 1.f)
    {
      endVoltdataCount = voltdataCount; 
    }
    */

    voltData[voltdataCount] = volt;
    voltdataCount = (voltdataCount + 1) % FLOAT_DATA_MAX;
  }

  bool isReadData()
  {
    if (readChunkCount >= 0)
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  size_t* getWritePointer()
  {
    if (readChunkCount >= 0)
    {
      const int startChunk = startVoltdataCount / FLOAT_READ_DATA_MAX;
      const int currentChunk = startChunk + readChunkCount;
      return (size_t*)&voltData[currentChunk * FLOAT_READ_DATA_MAX];
    }
    else
    {
      return (size_t*)&voltData[0];
    }
  }

  void reset()
  {
    for (int i = 0; i < FLOAT_DATA_MAX; ++i)
    {
      voltData[i] = 0.f;
    }

    voltdataCount = 0;
    startVoltdataCount = 0;
    endVoltdataCount = 0;
  }

  private:
  int readChunkCount{0};

};

class FlagSet{
  public:
  bool calibFlag = false;
  bool startFlag = false;
  bool gyroActiveFlag = true;

  void setState(const uint8_t* pCommandValue)
  {
    if (pCommandValue && pCommandValue[0] == 1) {
      Serial.println("Calib Flag On.");
      calibFlag = true;
    }

    if (pCommandValue && pCommandValue[1] == 1) {
      Serial.println("Start.");
      calibFlag = true;
      startFlag = true;
    } else if (pCommandValue && pCommandValue[1] == 2) {
      Serial.println("Stop.");
      startFlag = false;
    }
    
    if (pCommandValue && pCommandValue[2] == 1) {
      Serial.print("Gyro Status ");
      Serial.println("On.");
      gyroActiveFlag = true;
    } else if (pCommandValue && pCommandValue[2] == 2) {
      Serial.print("Gyro Status ");
      Serial.println("Off.");
      gyroActiveFlag = false;
    }
  };
};

FlagSet flagSet;

ReadVoltCache readVoltCache;

BLEService voltageLoggerService(SERVICE_UUID);  // create service
// create switch characteristic and allow remote device to read and write
BLECharacteristic loggerCharacteristic(LOGGER_CHARACTERISTIC_UUID, BLERead | BLEWrite | BLENotify, 16);
BLECharacteristic commandCharacteristic(COMMAND_CHARACTERISTIC_UUID, BLERead | BLEWrite, 4);
BLECharacteristic readdataCharacteristic(READDATA_CHARACTERISTIC_UUID, BLERead | BLEWrite | BLENotify, ReadVoltCache::READ_DATA_MAX);

void bleSetup() {

  // begin initialization
  while (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
  }

  BLE.setLocalName(BLE_LOCAL_NAME);
  BLE.setAdvertisedService(voltageLoggerService);

  voltageLoggerService.addCharacteristic(loggerCharacteristic);
  voltageLoggerService.addCharacteristic(commandCharacteristic);
  voltageLoggerService.addCharacteristic(readdataCharacteristic);

  BLE.addService(voltageLoggerService);

  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  commandCharacteristic.setEventHandler(BLEWritten, characteristicWritten);
  // readdataCharacteristic.writeValue((size_t*)&voltData[0], 16);

  // start advertising
  BLE.advertise();

  Serial.println(("Bluetooth® device active, waiting for connections..."));
}

void gyroSetup() {

  Serial.println("IMU Settings.");

  char s[60];
  sprintf(s, "%d %d %d %d %d %d %d %d %d",
    IMU.settings.gyroEnabled,
    IMU.settings.gyroFifoEnabled,
    IMU.settings.accelEnabled,
    IMU.settings.accelFifoEnabled,
    IMU.settings.tempEnabled,
    IMU.settings.gyroSampleRate,
    IMU.settings.accelSampleRate,
    IMU.settings.gyroBandWidth,
    IMU.settings.accelBandWidth
  );
  Serial.println(s);

/*
  IMU.writeRegister(LSM6DS3_ACC_GYRO_CTRL2_G,  0x8C);
  IMU.writeRegister(LSM6DS3_ACC_GYRO_CTRL1_XL, 0x8A);
  IMU.writeRegister(LSM6DS3_ACC_GYRO_CTRL7_G,  0x00);
  IMU.writeRegister(LSM6DS3_ACC_GYRO_CTRL8_XL, 0x09);
*/
  while (IMU.begin() != 0) {
    Serial.println("Failed to initialize IMU!");
  }

  Serial.print("IMU Initailized.");

}

void blePeripheralConnectHandler(BLEDevice central) {
  // central connected event handler
  Serial.print("Connected: ");
  Serial.println(central.address());
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  Serial.println("Disconnected: ");
  BLE.advertise();
  Serial.print("Advertise: ");

  Serial.println(central.address());
}

void characteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // central wrote new value to characteristic, update LED

  const uint8_t* pCommandValue = characteristic.value();

  flagSet.setState(pCommandValue);

  if (!(pCommandValue && pCommandValue[3] < 0)) {
    Serial.print("ReadData Flag Status ");
    Serial.println(pCommandValue[3]);

    readVoltCache.setReadChunk(pCommandValue[3]);
  }
  else
  {
    Serial.print("ReadData Flag Status ");
  }

}


class VoltCache
{
  public:
  int readCount = 0;
  float sumAnalogRead = 0.f;
  float calibVolt = 0.f;

  float cacheMillis = 0.f;
  float startMillis = 0.f;

  const float calibStartMillis = 100.f;
  const float calibEndMillis = 200.f;

  void addVolt() {
    float analogReadValue0 = analogRead(SENSOR_READ_VOLT);
    readCount++;
    sumAnalogRead += analogReadValue0;
  }

  void calib()
  {
    sumAnalogRead = 0.f;
    calibVolt = 0.f;
    readCount = 0;

    cacheMillis = 0.f;
    startMillis = 0.f;

    Serial.println("Volt Calib Start.");
    while(calibLoop())
    ;
    Serial.println(calibVolt);
    Serial.println("Volt Calib End.");
  }

  bool calibLoop()
  {
    float currentMillis = millis();
    if (startMillis == 0.f)
    {
      startMillis = currentMillis;
    }
    else
    {
      float deltaMillis = currentMillis - startMillis;

      if (deltaMillis > calibEndMillis)
      {
        calibVolt = getVolt();
        return false;
      }
      else if (deltaMillis > calibStartMillis)
      {
        addVolt();
      }
    }

    return true;
  }

  float getVolt()
  {
    float resultVolt = ((sumAnalogRead / (float)readCount) - calibVolt) * TO_VOLT;
    sumAnalogRead = 0.f;
    readCount = 0;
    return resultVolt;
  }
};

class AngleCache
{
  public:
  AngleCache(){};
  Quaternion quat = Quaternion();

  float x = 0.f;
  float y = 0.f;
  float z = 0.f;

  float calibX = 0.f;
  float calibY = 0.f;
  float calibZ = 0.f;

  float cacheMillis = 0.f;
  float startMillis = 0.f;

  const float calibStartMillis = 1000.f;
  const float calibEndMillis = 3000.f;
  const float scale = 0.001f;

  static constexpr float toRad = (M_PI / 180.f);
  static constexpr float toDeg = (180.f / M_PI);

  void calib()
  {
    quat = Quaternion();

    x = 0.f;
    y = 0.f;
    z = 0.f;

    calibX = 0.f;
    calibY = 0.f;
    calibZ = 0.f;

    cacheMillis = 0.f;
    startMillis = 0.f;

    Serial.println("Angle Calib Start.");
    digitalWrite(LEDB, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
    while(calibLoop())
    ;

    Serial.println("Angle Calib End.");
    digitalWrite(LED_BUILTIN, HIGH);
  }

  bool calibLoop()
  {
    float currentMillis = millis();
    float inX = IMU.readFloatGyroX();
    float inY = IMU.readFloatGyroY();
    float inZ = IMU.readFloatGyroZ();

    if (startMillis == 0.f)
    {
      startMillis = currentMillis;
    }
    else
    {
      float deltaMillis = currentMillis - startMillis;

      if (deltaMillis > calibEndMillis)
      {

        calibX = x / ((calibEndMillis - calibStartMillis) * scale);
        calibY = y / ((calibEndMillis - calibStartMillis) * scale);
        calibZ = z / ((calibEndMillis - calibStartMillis) * scale);

        x = 0.f;
        y = 0.f;
        z = 0.f;

        return false;
      }
      else if (deltaMillis > calibStartMillis)
      {
        addAngle();
      }
      else
      {
        addAngle();
        x = 0.f;
        y = 0.f;
        z = 0.f;
      }
    }

    return true;

  }

  void addAngle()
  {
    float currentMillis = millis();
    float inX = IMU.readFloatGyroX();
    float inY = IMU.readFloatGyroY();
    float inZ = IMU.readFloatGyroZ();

    float deltaMillis = currentMillis - cacheMillis;
    cacheMillis = currentMillis;

    x += (inX - calibX) * deltaMillis * scale;
    y += (inY - calibY) * deltaMillis * scale;
    z += (inZ - calibZ) * deltaMillis * scale;
  }

  void addAngleForQuat()
  {
    float currentMillis = millis();

    // uint8_t tempOutValue[12];
    // IMU.readRegisterRegion(&tempOutValue[0], LSM6DS3_ACC_GYRO_OUTX_L_G, 12);
    // IMU.readRegisterRegion(&tempOutValue[0], LSM6DS3_ACC_GYRO_OUTX_L_XL, 6);

    float inX = IMU.readFloatGyroX();
    float inY = IMU.readFloatGyroY();
    float inZ = IMU.readFloatGyroZ();

    float deltaMillis = currentMillis - cacheMillis;
    cacheMillis = currentMillis;

    float tempX = (inX - calibX) * deltaMillis * scale;
    float tempY = (inY - calibY) * deltaMillis * scale;
    float tempZ = (inZ - calibZ) * deltaMillis * scale;

    Quaternion mulQuat;
    mulQuat = mulQuat.from_euler_rotation_approx(toRad * tempX, toRad * tempY, toRad * tempZ);

    quat *= mulQuat;
    quat.normalize();
  }

  static void quatToYPR(const Quaternion& quat, float& roll, float& pitch, float& yaw) {
    float w = quat.a;
    float x = quat.b;
    float y = quat.c;
    float z = quat.d;

    roll = atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)) * toDeg;
    pitch = asin(2 * (w * y - x * z)) * toDeg;
    yaw = atan2(2 * (w * z + x * y), 1 - 2 * (z * z + y * y)) * toDeg;
  }
};

AngleCache currentAngle;
VoltCache voltCache;

void readVolt() {

  voltCache.addVolt();
}

void readAngle() {

  currentAngle.addAngleForQuat();
}

void setup() {

  Serial.begin(115200);
  // while (!Serial);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LEDB, OUTPUT);
  pinMode(SENSOR_READ_VOLT, INPUT);

  digitalWrite(LEDB, LOW);

  bleSetup();

  gyroSetup();

}

float sendValue[4] = {0.f, 0.f, 0.f, 0.f};

void loop() {

  static unsigned long volt_millis_buf = 0;

  while (true) {
    readVolt();

    if (flagSet.gyroActiveFlag) {
      readAngle();
    }

    if ((millis() - volt_millis_buf) > ONE_FRAME_MS) {
#if XIAO
      if (pServer->getConnectedCount() == 0) {
        BLEAdvertising *pAdvertising = pServer->getAdvertising();
        pAdvertising->start();
      }

      if (pCharacteristic) {
        float averageValue = sumAnalogRead0 / (float)readCount;
        sumAnalogRead0 = 0;
        readCount = 0;
        Serial.println(averageValue);
        pCharacteristic->setValue(averageValue);
        pCharacteristic->notify();
      }

#else
      if (BLE.connected())
      {
        if (flagSet.startFlag == true)
        {
        // Serial.println(voltCache.readCount);

          if (flagSet.calibFlag)
          {
            currentAngle.calib();
            voltCache.calib();
            readVoltCache.reset();
            flagSet.calibFlag = false;
          }

          {
            /*
            Serial.print(currentAngle.quat.a);
            Serial.print(", ");
            Serial.print(currentAngle.quat.b);
            Serial.print(", ");
            Serial.print(currentAngle.quat.c);
            Serial.print(", ");
            Serial.println(currentAngle.quat.c);
            */

            AngleCache::quatToYPR(currentAngle.quat, sendValue[1], sendValue[2], sendValue[3]);
            /*
            Serial.print(sendValue[1]);
            Serial.print(", ");
            Serial.print(sendValue[2]);
            Serial.print(", ");
            Serial.println(sendValue[3]);
            */

            /*
            sendValue[1] = currentAngle.x;
            sendValue[2] = currentAngle.y;
            sendValue[3] = currentAngle.z;
            */
          }
          {
            sendValue[0] = voltCache.getVolt();
            readVoltCache.setVolt(sendValue[0]);
            loggerCharacteristic.writeValue((size_t*)&sendValue[0], 16);
          }
        }
        else
        {

          if (readVoltCache.isReadData())
          {
            readdataCharacteristic.writeValue(readVoltCache.getWritePointer(), ReadVoltCache::READ_DATA_MAX); // ReadVoltCache::READ_DATA_MAX
            digitalWrite(LEDB, LOW);
            Serial.println(readVoltCache.getReadChunk());
            readVoltCache.setReadChunk(-1);

          }
          else
          {

          }

        }
      }
#endif
      volt_millis_buf = millis();
    }
  }
}