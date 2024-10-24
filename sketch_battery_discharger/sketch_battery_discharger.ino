
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MsTimer2.h>
#include <EEPROM.h>

const int in0V = 0;
const int in1V = 1;
const int inCV = 2;

const float FPS = 60.f;
const float ONE_FRAME_MS = (1.f / FPS) * 1000.f;
const float ACTIVE_FRAME = 270.f;
const float SUSPEND_FRAME = 30.f;
const float ACTIVE_RATE = ACTIVE_FRAME /(ACTIVE_FRAME + SUSPEND_FRAME);

int PWM_OUT = 6;

int BUTTON_L = 8;
int BUTTON_C = 7;
int BUTTON_R = 4;

int ACTIVE_POWER = 120;
int ACTIVE_POWER_MAX = 255;
int ACTIVE_POWER_MIN = 50;

int LONG_PUSH_FRAME_COUNT = 60;
int SHORT_PUSH_FRAME_COUNT = 20;

int POWER_ADDRESS = 0x0000;
int IS_ACTIVE_ADDRESS = 0x0001;
int TARGET_VOLT_ADDRESS = 0x0002;

int currentActivePower = ACTIVE_POWER;

float TARGET_VOLT_MIN = 2.4;
float TARGET_VOLT_MAX = 3.0;
float targetVolt = TARGET_VOLT_MAX;


float currentVolt = 0;

float sumAmp = 0;

int CHARSIZEX = 6;
int CHARSIZEY = 8;
int TEXT_SIZE = 1;

long sumAnalogRead0 = 0;
long sumAnalogRead1 = 0;
long sumAnalogReadR = 0;
int readCount = 0;

long sumSuspendAnalogRead0 = 0;
long sumSuspendAnalogRead1 = 0;
long sumSuspendAnalogReadR = 0;
int suspendReadCount = 0;

int pushButtonL = 0;
int pushButtonC = 0;
int pushButtonR = 0;

int pushButtonCountL = 0;
int pushButtonCountC = 0;
int pushButtonCountR = 0;

int lastPushButtonCountL = 0;
int lastPushButtonCountC = 0;
int lastPushButtonCountR = 0;

int settingMode = 0;

const float VOLT5 = 4.90;

bool activeFlag = true;

bool suspendFlag = false;

float REG = 0.36;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {

  activeFlag = true;

  pinMode(in0V, INPUT);
  pinMode(in1V, INPUT);
  pinMode(inCV, INPUT);

  pinMode(PWM_OUT, OUTPUT);
  
  Serial.begin(9600);

  setupDisplay();
  
  currentActivePower = EEPROM.read(POWER_ADDRESS);
  activeFlag = EEPROM.read(IS_ACTIVE_ADDRESS);
  EEPROM.get(TARGET_VOLT_ADDRESS, targetVolt);
  if (targetVolt < TARGET_VOLT_MIN || targetVolt > TARGET_VOLT_MAX)
  {
    targetVolt = TARGET_VOLT_MAX;
  }
  
  updateAnalogWrite();

  Serial.write("set up end");
}

void updateAnalogWrite()
{

  EEPROM.write(POWER_ADDRESS, currentActivePower);
  
  if (!suspendFlag)
  {
    if (activeFlag)
    {
      analogWrite(PWM_OUT, 0);
      EEPROM.write(IS_ACTIVE_ADDRESS, activeFlag);
    }
    else
    {
      const float diff = constrain(currentVolt - targetVolt, 0.f, 3.f);
      float rate = constrain(diff * 8.f, 0.f, 1.f);
      int power = 0;
      if (rate > 0)
      {
        power = (int)((float)(currentActivePower - ACTIVE_POWER_MIN) * rate) + ACTIVE_POWER_MIN;
      }
        
      drawFloat(rate, 12, 3);
      
      analogWrite(PWM_OUT, power);
      EEPROM.write(IS_ACTIVE_ADDRESS, activeFlag);
    }
  }
  else
  {
    analogWrite(PWM_OUT, 0);
  }
}

void setupDisplay(void) {

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  // display.display();
  delay(500); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(TEXT_SIZE);
}

void updateMain(){
  
  pushButtonCount();

  // drawButton();

  if (lastPushButtonCountC > 0 && lastPushButtonCountC < LONG_PUSH_FRAME_COUNT)
  {
    updateAnalogWrite();
    settingMode = (settingMode + 1) % 2;
  }

  if (settingMode == 0)
  {
    if (lastPushButtonCountL > 0){
      --currentActivePower;
      currentActivePower = constrain(currentActivePower, ACTIVE_POWER_MIN, ACTIVE_POWER_MAX);
      updateAnalogWrite();
    }
  
    if (lastPushButtonCountR > 0)
    {
      ++currentActivePower;
      currentActivePower = constrain(currentActivePower, ACTIVE_POWER_MIN, ACTIVE_POWER_MAX);
      updateAnalogWrite();
    }
  }
  else if (settingMode == 1)
  {
    if (lastPushButtonCountL > 0){
      targetVolt = targetVolt - 0.01;
      targetVolt = constrain(targetVolt, TARGET_VOLT_MIN, TARGET_VOLT_MAX);
      EEPROM.put(TARGET_VOLT_ADDRESS, targetVolt);
    }
  
    if (lastPushButtonCountR > 0)
    {
      targetVolt = targetVolt + 0.01;
      targetVolt = constrain(targetVolt, TARGET_VOLT_MIN, TARGET_VOLT_MAX);
      EEPROM.put(TARGET_VOLT_ADDRESS, targetVolt);
    }
  }

  {
    drawInt(currentActivePower, 1, 3);
    drawFloat(targetVolt, 6, 3);
    drawChar("V", 10, 3);
    
    if (settingMode == 0)
    {
      drawChar(">", 0, 3);
      drawChar("", 5, 3);
    }
    else
    {
      drawChar("", 0, 3);
      drawChar(">", 5, 3);
    }

  }
  
  pushButtonL = 0;
  pushButtonC = 0;
  pushButtonR = 0;
}

void pushButtonCount(){

  if (pushButtonL > 0)
  {
    ++pushButtonCountL;
    if (pushButtonCountL == SHORT_PUSH_FRAME_COUNT)
    {
      lastPushButtonCountL = pushButtonCountL;
    }
  }
  else
  {
    lastPushButtonCountL = pushButtonCountL;
    pushButtonCountL = 0;
  }

  if (pushButtonC > 0)
  {
    ++pushButtonCountC;
    if (pushButtonCountC == LONG_PUSH_FRAME_COUNT)
    {
      lastPushButtonCountC = pushButtonCountC;
      if(activeFlag == true)
      {
        activeFlag = false;
        Serial.write(activeFlag);
      }
      else
      {
        activeFlag = true;
        Serial.write(activeFlag);
      }
      
      updateAnalogWrite();
    }
  }
  else
  {
    lastPushButtonCountC = pushButtonCountC;
    pushButtonCountC = 0;
  }

  if (pushButtonR > 0)
  {
    ++pushButtonCountR;
    if (pushButtonCountR == SHORT_PUSH_FRAME_COUNT)
    {
      lastPushButtonCountR = pushButtonCountR;
    }
  }
  else
  {
    lastPushButtonCountR = pushButtonCountR;
    pushButtonCountR = 0;
  }
  
}

void drawButton(){
  drawInt(pushButtonCountL, 0, 2);
  drawInt(pushButtonCountC, 4, 2);
  drawInt(pushButtonCountR, 8, 2);
}

void readVolt() {

  float analogReadValue0 = analogRead(in0V);
  float analogReadValue1 = analogRead(in1V);
  float analogReadValueC = analogRead(inCV);

  if (!suspendFlag)
  {
    readCount++;
    sumAnalogRead0 += analogReadValue0;
    sumAnalogRead1 += analogReadValue1 - analogReadValue0 - 1;
    sumAnalogReadR += analogReadValue1 - analogReadValueC - 1;
  }
  else
  {
    suspendReadCount++;
    sumSuspendAnalogRead0 += analogReadValue0;
    sumSuspendAnalogRead1 += analogReadValue1 - analogReadValue0 - 1;
    sumSuspendAnalogReadR += analogReadValue1 - analogReadValueC - 1;
    
  }

}

void readButtonPush() {
  if (pushButtonL <= 0) pushButtonL = 1 - digitalRead(BUTTON_L);
  if (pushButtonC <= 0) pushButtonC = 1 - digitalRead(BUTTON_C);
  if (pushButtonR <= 0) pushButtonR = 1 - digitalRead(BUTTON_R);
}


void drawVoltAll() {
  static const float toVolt = VOLT5 / 1024.f;
  static const float toA = 1.f / REG;
  static const float toMah = 1000.f / 3600.f;

  if (readCount > 0)
  {
    const float volt0 = constrain(((float)sumAnalogRead0 / (float)readCount) * toVolt, 0.f, 10.f);
    const float volt1 = constrain(((float)sumAnalogRead1 / (float)readCount) * toVolt, 0.f, 10.f);
    const float amp = constrain(((float)sumAnalogReadR / (float)readCount) * toVolt * toA, 0.f, 10.f);
    
    drawFloat(volt0, 0, 0);
    drawChar("V", 4, 0);
    drawFloat(volt1, 0, 1);
    drawChar("V", 4, 1);
    drawFloat(amp, 12, 0);
    drawChar("A", 16, 0);
    
    readCount = 0;
    sumAnalogRead0 = 0;
    sumAnalogRead1 = 0;
    sumAnalogReadR = 0;

    
    sumAmp += amp * toMah * ACTIVE_RATE;

    drawFloat(sumAmp, 12, 1);
    drawChar("mAH", 16, 1 );
  }
  
  if (suspendReadCount > 0)
  {
    const float suspendVolt0 = constrain(((float)sumSuspendAnalogRead0 / (float)suspendReadCount) * toVolt, 0.f, 10.f);
    const float suspendVolt1 = constrain(((float)sumSuspendAnalogRead1 / (float)suspendReadCount) * toVolt, 0.f, 10.f);
    const float suspendVoltR = constrain(((float)sumSuspendAnalogReadR / (float)suspendReadCount) * toVolt * toA, 0.f, 10.f);

    currentVolt = suspendVolt1 + suspendVolt0;
  
    drawFloat(suspendVolt0, 6, 0);
    drawChar("V", 10, 0);
    drawFloat(suspendVolt1, 6, 1);
    drawChar("V", 10, 1);
    //drawFloat(suspendVoltR, 6, 0);
  
    suspendReadCount = 0;
    sumSuspendAnalogRead0 = 0;
    sumSuspendAnalogRead1 = 0;
    sumSuspendAnalogReadR = 0;
  }
}

void drawInt(int isButton, float offsetX, float offsetY) {
  display.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
  display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
  display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
  display.println(isButton);
}

void drawFloat(float volt, float offsetX, float offsetY) {

  display.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 5, CHARSIZEY * 1, BLACK);
  display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
  display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
  display.println(String(volt, 2));
}

void drawChar(char* chr, float offsetX, float offsetY) {

  display.fillRect(CHARSIZEX * offsetX, CHARSIZEY * offsetY, CHARSIZEX * 1, CHARSIZEY * 1, BLACK);
  display.setCursor(CHARSIZEX * offsetX, CHARSIZEY * offsetY);
  display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
  display.println(chr);

}

void loop() {

  static unsigned char count = 0;
  static unsigned long millis_buf = 0;

  static unsigned long volt_millis_buf = 0;
  static unsigned long frame_millis_buf = 0;
  static unsigned long suspend_millis_buf = 0;


  while (true)
  {

    readButtonPush();
    
    if (suspendFlag == false)
    {
      if ((millis() - suspend_millis_buf) > ONE_FRAME_MS * 540.f)
      {
        suspend_millis_buf = millis();
        suspendFlag = true;
        updateAnalogWrite();
      }
    }
    else
    {
      if ((millis() - suspend_millis_buf) > ONE_FRAME_MS * 60.f)
      {
        suspend_millis_buf = millis();
        suspendFlag = false;
        updateAnalogWrite();
      }
    }

    readVolt();
    
    //　現在の経過時間-この待機処理を通過した時間が1000(ms)になるまで待機
    if ((millis() - volt_millis_buf) > ONE_FRAME_MS * FPS)
    {
      volt_millis_buf = millis();

      drawVoltAll();
    }
    
    if ((millis() - frame_millis_buf) > ONE_FRAME_MS)
    {
      frame_millis_buf = millis();

      updateMain();
      display.display();
    }
    

    



    
  }


}
