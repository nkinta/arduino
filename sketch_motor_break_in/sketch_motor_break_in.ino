
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// int enA = 12;
// int inA1 = 11;
// int inA2 = 10;

int enA = 11;
int inA1 = 12;
int inA2 = 13;

int enB = 10;
int inB1 = 8;
int inB2 = 9;

int readA = 6;
int readB = 7;

int VAG = 0;
int VAV = 1;

int VBG = 2;
int VBV = 3;

float INCREASE_MULTIPLE = 2.0;

float INCREASE_MSEQ = 50.f;
long INCREASE_SEQ = 2;

long ROTATIONSEQ = 50;
int LOGSLEEP = 1000;
int SLEEPSEQ = 5;

int STARTPOWER = 80;

int POWER_A = 255;
int POWER_B = 255;

int CHARSIZEX = 6;
int CHARSIZEY = 8;

int countA = 0;
int countB = 0;

enum MotorStatus{
    STAT_START,
    STAT_PLUS_INCREASE,
    STAT_PLUS_STAY,
    STAT_PLUS_STOP,
    STAT_MINUS_INCREASE,
    STAT_MINUS_STAY,
    STAT_MINUS_STOP
};

int POWER = 255;

float STAY_TIME = 120000;

float STOP_TIME = 5000;

MotorStatus motorStatus = STAT_START;

float currentStatusStartTime = 0.f;
float currentPrintStartTime = 0.f;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  pinMode(enA, OUTPUT);
  pinMode(inA1, OUTPUT);
  pinMode(inA2, OUTPUT);
  
  pinMode(VAG, INPUT);
  pinMode(VAV, INPUT);
  
  pinMode(enB, OUTPUT);
  pinMode(inB1, OUTPUT);
  pinMode(inB2, OUTPUT);
  
  pinMode(VBG, INPUT);
  pinMode(VBV, INPUT);

  pinMode(readA, INPUT);
  pinMode(readB, INPUT);
  
  Serial.begin(9600);


  setupDisplay();
  
}

void setupDisplay(void) {

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(500); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
}


void drawStatusCount(int count) {
  display.fillRect(54, 0, CHARSIZEX * 2, CHARSIZEY * 1, BLACK);
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(54,0);             // Start at top-left corner
  display.println(count);
  
  display.display();
  // delay(2000);
}

void drawStatus(char* Message) {
  Serial.println( Message );
  
  display.fillRect(0, 0, CHARSIZEX * 2 * 8, CHARSIZEY * 1, BLACK);
  display.setCursor(0,0);
  display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
  display.println(Message);

  // display.setTextSize(2);             // Draw 2X-scale text
  // display.setTextColor(SSD1306_WHITE);
  // display.print(F("0x")); display.println(0xDEADBEEF, HEX);

  display.display();
}

void drawVolt(int index , float volt) {

  Serial.println( volt );
  
  display.fillRect(64 * index, 8, CHARSIZEX * 8, CHARSIZEY * 1, BLACK);
  display.setCursor(64 * index,8);
  display.setTextColor(SSD1306_WHITE); // Draw 'inverse' text
  display.println(volt);
  
  display.display();
}

void drawVoltA(float volt) {
  drawVolt(0, volt);
}

void drawVoltB(float volt) {
  drawVolt(1, volt);
}

void startMotorPlus() {
  // Serial.println("plus");
  digitalWrite(inB1, LOW);
  digitalWrite(inB2, HIGH);
  digitalWrite(inA1, LOW);
  digitalWrite(inA2, HIGH);
}

void startMotorMinus() {
  // Serial.println("minus");
  digitalWrite(inB1, HIGH);
  digitalWrite(inB2, LOW);
  digitalWrite(inA1, HIGH);
  digitalWrite(inA2, LOW);
}

void stopMotor() {
  // Serial.println("stop");
  digitalWrite(inB1, LOW);
  digitalWrite(inB2, LOW);
  digitalWrite(inA1, LOW);
  digitalWrite(inA2, LOW);
}

void increasePower()
{

  float volumeA = analogRead(readA) / 1024.f;
  float volumeB = analogRead(readB) / 1024.f;

  int TOTALNUM = POWER_A - STARTPOWER;
  float IncreaseDuration = (float)(INCREASE_SEQ * 1000) / (float)(TOTALNUM);
  int IncreaseTotal = 0;

  float powerA = 0;
  float powerB = 0;
  for (int i = 0; i < TOTALNUM; ++i)
  {
    powerA = (STARTPOWER + i) * volumeA;
    powerB = (STARTPOWER + i) * volumeB;
    analogWrite(enB, powerA);
    analogWrite(enA, powerB);
    delay(IncreaseDuration);
    IncreaseTotal += IncreaseDuration;
    // drawStatusCount(IncreaseTotal / 1000);
    // drawStatusCount(powerA);

    float time = millis();

  }
  analogWrite(enB, powerB);
  analogWrite(enA, powerA);
}

void loggingVoltage(int num){
  for (int i = 0 ; i < num; ++i) {
    float vcountBV = 0;
    float vcountBG = 0;
    float vcountAV = 0;
    float vcountAG = 0;
    for (int j = 0 ; j < 100; ++j) {
      // vcountBV += analogRead( VBV );
      // vcountBG += analogRead( VBG );
      // vcountAV += analogRead( VAV );
      // vcountAG += analogRead( VAG );
      delay( 10 );
    }
    float vcountB = vcountBV - vcountBG;
    float voltB = vcountB * 5.0 / 100.0 / 1024.0;
    float vcountA = vcountAV - vcountAG;
    float voltA = vcountA * 5.0 / 100.0 / 1024.0;

    float volumeA = analogRead( readA );
    float volumeB = analogRead( readB );

    drawVoltB(voltB);
    drawVoltA(voltA);
    drawStatusCount(i);
    
    Serial.println( volumeA );
    Serial.println( volumeB );
    // delay( LOGSLEEP );
    
  }
}

void demoOne() {

  // Serial.println( time );
  
  startMotorMinus();
  drawStatus("IncMinus");
  increasePower();
  
  drawStatus("Minus");
  loggingVoltage(ROTATIONSEQ);

  stopMotor();
  drawStatus("Sleep");
  
  loggingVoltage(SLEEPSEQ);

  startMotorPlus();
  drawStatus("IncPlus");

  increasePower();
  drawStatus("Plus");
  loggingVoltage(ROTATIONSEQ);

  stopMotor();
  drawStatus("Sleep");
   
  loggingVoltage(SLEEPSEQ);
}

void writePower(int writePinNo, int analogReadPinNo, float timeRate)
{

  float volume = analogRead(analogReadPinNo) / 1024.f;

  analogWrite(writePinNo, POWER * volume * timeRate);

}

void printPower(int index, int voltRead1, int voltRead2)
{
   float volume1 = analogRead(voltRead1);
   float volume2 = analogRead(voltRead2);

  drawVolt(index, (volume1 - volume2) / 1024.f * 3.f * 4.6f);
  
}


void myLoop() {
  
   float currentTime = millis();

   float currentStatusTime = currentTime - currentStatusStartTime;

   float currentPrintTime = currentTime - currentPrintStartTime;
  if (currentPrintTime > 500.f)
  {

    printPower(0, VAG, VAV);
    printPower(1, VBG, VBV);

    currentPrintStartTime = currentTime;
  }

  if (STAT_START == motorStatus)
  {
    if (currentStatusTime > 1000.f)
    {
      startMotorPlus();
      drawStatus("PlusInc");
      motorStatus = STAT_PLUS_INCREASE;
      currentStatusStartTime = currentTime;
    }
  }
  else if (STAT_PLUS_INCREASE == motorStatus)
  {
    writePower(enA, readA, INCREASE_MULTIPLE * currentStatusTime / INCREASE_MSEQ);
    writePower(enB, readB, INCREASE_MULTIPLE * currentStatusTime / INCREASE_MSEQ);
    if (currentStatusTime > INCREASE_MSEQ)
    {
      drawStatus("PlusStay");
      motorStatus = STAT_PLUS_STAY;
      currentStatusStartTime = currentTime;
    }
  }
  else if (STAT_PLUS_STAY == motorStatus)
  {
    writePower(enA, readA, 1.f);
    writePower(enB, readB, 1.f);
    if (currentStatusTime > STAY_TIME)
    {
      writePower(enA, readA, 0.f);
      writePower(enB, readB, 0.f);
      stopMotor();
      drawStatus("PlusStop");
      motorStatus = STAT_PLUS_STOP;
      currentStatusStartTime = currentTime;
    }
  }
  else if (STAT_PLUS_STOP == motorStatus)
  {
    if (currentStatusTime > STOP_TIME)
    {
      startMotorMinus();
      drawStatus("MinusInc");
      motorStatus = STAT_MINUS_INCREASE;
      currentStatusStartTime = currentTime;
    }
  }
  else if (STAT_MINUS_INCREASE == motorStatus)
  {
    writePower(enA, readA, INCREASE_MULTIPLE * currentStatusTime / INCREASE_MSEQ);
    writePower(enB, readB, INCREASE_MULTIPLE * currentStatusTime / INCREASE_MSEQ);
    if (currentStatusTime > INCREASE_MSEQ)
    {
      drawStatus("MinusStay");
      motorStatus = STAT_MINUS_STAY;
      currentStatusStartTime = currentTime;
    }
  }
  else if (STAT_MINUS_STAY == motorStatus)
  {
      writePower(enA, readA, 1.f);
      writePower(enB, readB, 1.f);
    if (currentStatusTime > STAY_TIME)
    {
      stopMotor();
      writePower(enA, readA, 0.f);
      writePower(enB, readB, 0.f);
      drawStatus("MinusStop");
      motorStatus = STAT_MINUS_STOP;
      currentStatusStartTime = currentTime;
    }
  }
  else if (STAT_MINUS_STOP == motorStatus)
  {
    if (currentStatusTime > STOP_TIME)
    {
      startMotorPlus();
      drawStatus("PlusInc");
      motorStatus = STAT_PLUS_INCREASE;
      currentStatusStartTime = currentTime;
    }
  }
}


void loop() {

  Serial.println("demo 1");

  while (true)
  {
    myLoop();
  }

  Serial.println("demo 1");
}
