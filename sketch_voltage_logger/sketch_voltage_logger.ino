
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

#define BLE_LOCAL_NAME "VoltageLogger"

const int SENSOR_READ_VOLT = 2;

const float FPS = 6.f;
const float ONE_FRAME_MS = (1.f / FPS) * 1000.f;



#if XIAO
class DataSendCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.println("*********");
      Serial.print("value: ");
      for (int i = 0; i < value.length(); i++)
        Serial.print(value[i]);

      Serial.println();
      Serial.println("*********");
    }
  }
  /*
    void onRead(BLECharacteristic *pCharacteristic) {
      // pCharacteristic->setValue(String("Hello"));
    }
    */
};

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;

void bleSetup() {
  BLEDevice::init(BLE_LOCAL_NAME);

  pServer = BLEDevice::createServer();

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY  //  | BLECharacteristic::PROPERTY_INDICATE
  );

  pCharacteristic->setCallbacks(new DataSendCallbacks());
  // pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

#else

BLEService voltageLoggerService(SERVICE_UUID);  // create service
// create switch characteristic and allow remote device to read and write
BLECharacteristic loggerCharacteristic(LOGGER_CHARACTERISTIC_UUID, 16, BLERead | BLEWrite | BLENotify);
BLECharacteristic commandCharacteristic(COMMAND_CHARACTERISTIC_UUID, 4, BLERead | BLEWrite);


void bleSetup() {

  // begin initialization
  while (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
  }

  BLE.setLocalName(BLE_LOCAL_NAME);
  BLE.setAdvertisedService(voltageLoggerService);

  voltageLoggerService.addCharacteristic(loggerCharacteristic);
  voltageLoggerService.addCharacteristic(commandCharacteristic);
  BLE.addService(voltageLoggerService);

  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  commandCharacteristic.setEventHandler(BLEWritten, characteristicWritten);

  // start advertising
  BLE.advertise();

  Serial.println(("Bluetooth® device active, waiting for connections..."));
}

LSM6DS3 IMU(I2C_MODE, 0x6A);

bool calibFlag = false;
bool stopFlag = true;

void gyroSetup() {

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

  if (pCommandValue && pCommandValue[0] == 1) {
    Serial.println("Calib Flag On.");
    calibFlag = true;
  }

  if (pCommandValue && pCommandValue[1] == 1) {
    Serial.println("Stop.");
    stopFlag = true;
  } else if (pCommandValue && pCommandValue[1] == 2) {
    Serial.println("Start.");
    calibFlag = true;
    stopFlag = false;
  }

}

#endif

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
    float resultVolt = (sumAnalogRead / (float)readCount) - calibVolt;
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
        addAngle(currentMillis, inX, inY, inZ);
      }
      else
      {
        addAngle(currentMillis, inX, inY, inZ);
        x = 0.f;
        y = 0.f;
        z = 0.f;
      }
    }

    return true;

  }

  void addAngle(float currentMillis, float inX, float inY, float inZ)
  {
    float deltaMillis = currentMillis - cacheMillis;
    cacheMillis = currentMillis;

    x += (inX - calibX) * deltaMillis * scale;
    y += (inY - calibY) * deltaMillis * scale;
    z += (inZ - calibZ) * deltaMillis * scale;
  }

  void addAngleForQuat(float currentMillis, float inX, float inY, float inZ)
  {
    float deltaMillis = currentMillis - cacheMillis;
    cacheMillis = currentMillis;

    float tempX = (inX - calibX) * deltaMillis * scale;
    float tempY = (inY - calibY) * deltaMillis * scale;
    float tempZ = (inZ - calibZ) * deltaMillis * scale;

    Quaternion mulQuat;
    static float toRad = (M_PI / 180.f);
    mulQuat = mulQuat.from_euler_rotation_approx(toRad * tempX, toRad * tempY, toRad * tempZ);

    quat *= mulQuat;
    quat.normalize();
  }

  static void quatToYPR(const Quaternion& quat, float& roll, float& pitch, float& yaw) {
    float w = quat.a;
    float x = quat.b;
    float y = quat.c;
    float z = quat.d;
    static const float toRad = (180.f / M_PI);
    roll = atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)) * toRad;
    pitch = asin(2 * (w * y - x * z)) * toRad;
    yaw = atan2(2 * (w * z + x * y), 1 - 2 * (z * z + y * y)) * toRad;
  }
};

AngleCache currentAngle;
VoltCache voltCache;

void readVolt() {

  voltCache.addVolt();
}

void readAngle() {

  // currentAngle.addAngle(millis(), IMU.readFloatGyroX(), IMU.readFloatGyroY(), IMU.readFloatGyroZ());
  currentAngle.addAngleForQuat(millis(), IMU.readFloatGyroX(), IMU.readFloatGyroY(), IMU.readFloatGyroZ());
}

void setup() {

  Serial.begin(115200);

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

    readAngle();

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
      if (BLE.connected() && stopFlag == false)
      {
        if (calibFlag)
        {
          currentAngle.calib();
          voltCache.calib();
          calibFlag = false;
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
          // sumAnalogRead0 = 0;
          // char* writeValue = (char*)&averageValue;

          loggerCharacteristic.writeValue((size_t*)&sendValue[0], 16);
        }
      }

#endif
      volt_millis_buf = millis();
    }
  }
}