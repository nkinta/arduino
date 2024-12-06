
#define XIAO false
#if XIAO

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#else

#include <ArduinoBLE.h>
#include <LSM6DS3.h>

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

int readCount = 0;
float sumAnalogRead0 = 0;
int count = 0;

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
  Serial.begin(9600);
  while (!Serial)
    ;

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

bool resetFlag = false;

void gyroSetup() {

  while (IMU.begin() != 0) {
    Serial.println("Failed to initialize IMU!");
  }

  Serial.print("Accelerometer sample rate = ");
  Serial.println("Hz");
}

void blePeripheralConnectHandler(BLEDevice central) {
  // central connected event handler
  Serial.print("Connected: ");
  Serial.println(central.address());
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  Serial.print("Disconnected: ");
  // BLE.advertise();
  // Serial.print("Advertise: ");
  Serial.println(central.address());
}

void characteristicWritten(BLEDevice central, BLECharacteristic characteristic) {
  // central wrote new value to characteristic, update LED
  Serial.print("Characteristic event, written: ");

  const uint8_t* pCommandValue = characteristic.value();

  if (pCommandValue && pCommandValue[0] == 1) {
    resetFlag = true;
  } else {
    resetFlag = false;
  }

}

#endif




void readVolt() {

  float analogReadValue0 = analogRead(SENSOR_READ_VOLT);

  readCount++;
  sumAnalogRead0 += analogReadValue0;
}

class AngleCache
{
  public:
  AngleCache(){};
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
    Serial.println("Calib Start.");
    while(calibLoop())
    ;

    Serial.println("Calib End.");
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
        Serial.println(deltaMillis);
        calibX = x / ((calibEndMillis - calibStartMillis) * scale);
        calibY = y / ((calibEndMillis - calibStartMillis) * scale);
        calibZ = z / ((calibEndMillis - calibStartMillis) * scale);

        x = 0.f;
        y = 0.f;
        z = 0.f;

        Serial.println(calibX);
        Serial.println(calibY);
        Serial.println(calibZ);

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
};

AngleCache currentAngle;

float sendValue[4] = {0.f, 0.f, 0.f, 0.f};

void readAngle() {

  currentAngle.addAngle(millis(), IMU.readFloatGyroX(), IMU.readFloatGyroY(), IMU.readFloatGyroZ());

}

void setup() {
  Serial.begin(115200);

  pinMode(SENSOR_READ_VOLT, INPUT);

  bleSetup();
  gyroSetup();

  // currentAngle.calib();

}

void loop() {

  static unsigned long volt_millis_buf = 0;

  delay(1000);

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
      if (!BLE.connected()) {
        BLE.advertise();
      }
/*
      if (resetFlag)
      {
        currentAngle.calib();
        resetFlag = false;
      }
*/
      {
        sendValue[1] = currentAngle.x;
        sendValue[2] = currentAngle.y;
        sendValue[3] = currentAngle.z;
      }
      {
        float averageValue = sumAnalogRead0 / (float)readCount;
        sendValue[0] = averageValue;
        // sumAnalogRead0 = 0;
        // char* writeValue = (char*)&averageValue;

        loggerCharacteristic.writeValue((size_t*)&sendValue[0], 16);
      }
#endif
      volt_millis_buf = millis();
    }
  }
}