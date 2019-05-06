// LCD_ST7032 - Version: Latest
//#include <LCD_ST7032.h>

#define READ_SIZE 200

const float Vin = 5.04;
const int LampPin = 6;
const int FanPin = 7;
const int THeatPin = 4;
const int BHeatPin = 5;
const int BuzzerPin = 9;
const char Top = 255;
//LCD_ST7032 lcd;

unsigned long phaseStart;
unsigned long soakSeconds = 100;
long positionLeft = 0;
float soakStartTemp = 140.0;
float soakMaxTemp = 160.0;
unsigned long reflowSeconds = 80;
float reflowStartTemp = 180.0;
float reflowPeakMinTemp = 205.0;
float reflowPeakMaxTemp = 220.0;
String message = "Press Start";
double tcTop;
double tcBott;
double ptTop;
double ptBott;
bool startFromSerial = LOW;
bool stopFromSerial = LOW;

void setup()
{
  Serial.begin(115200);
  Serial3.begin(115200);
  pinMode(LampPin, OUTPUT);
  pinMode(FanPin, OUTPUT);
  pinMode(THeatPin, OUTPUT);
  pinMode(BHeatPin, OUTPUT);
  pinMode(BuzzerPin, OUTPUT);
  digitalWrite(LampPin, LOW);
  digitalWrite(FanPin, LOW);
  digitalWrite(THeatPin, LOW);
  digitalWrite(BHeatPin, LOW);
}

double tcToTemp(int analog)
{
  double voltage = (Vin * analog) / 1024;
  return (voltage - 1.41) * 270;
}

double ptToTemp(int analog)
{
  double voltage = (Vin * analog) / 1024;
  return (2.43 - voltage) * 335;
}

void updateDataAndDisplay()
{
  tcTop = tcToTemp(analogRead(0));
  tcBott = tcToTemp(analogRead(1));
  ptTop = ptToTemp(analogRead(2));
  ptBott = ptToTemp(analogRead(3));

  String serialOut = "time:" + String(millis() / 1000) + ";tT:" + String(tcTop, 2) + ";pT:" + String(ptTop, 2) + ";tB:" + String(tcBott, 2) + ";pB:" + String(ptBott, 2) + ";phase:" + message;
  String lcdLine2 = String(Top + ":" + String(ptTop, 2) + "C" + String(tcTop, 2) + "C_:" + String(ptBott, 2) + "C" + String(tcBott, 2) + "C");

  Serial.println(serialOut);
  Serial3.println(serialOut);

//  lcd.setCursor(0, 0); //LINE 1 ADDRESS 0
//  lcd.print(message);
//  lcd.setCursor(1, 0); //LINE 2 ADDRESS 0
//  lcd.print(lcdLine2);
}

void readFromSerial()
{
  //start: Start
  //stop: stop
  //settings: soakSeconds:100;soakStartTemp:140;soakMaxTemp:160;reflowSeconds:80;reflowStartTemp:180;reflowMinTemp:205;reflowMaxTemp:220;

  char input[READ_SIZE + 1];
  byte size = Serial.readBytesUntil('\n', input, READ_SIZE);
  input[size] = 0;

  if (strstr(input, "Start") == input)
  {
    startFromSerial = HIGH;
    return;
  }

  char *type = strtok(input, ";");
  while (type != 0)
  {
    // Split the command in two values
    char *separator = strchr(type, ':');
    if (separator != 0)
    {
      // Actually split the string in 2: replace ':' with 0
      *separator = 0;
      ++separator;
      if (strstr(type, "soakSeconds"))
      {
        soakSeconds = atol(separator);
      }
      else if (strstr(type, "soakStartTemp"))
      {
        soakStartTemp = atof(separator);
      }
      else if (strstr(type, "soakMaxTemp"))
      {
        soakMaxTemp = atof(separator);
      }
      else if (strstr(type, "reflowSeconds"))
      {
        reflowSeconds = atol(separator);
      }
      else if (strstr(type, "reflowStartTemp"))
      {
        reflowStartTemp = atof(separator);
      }
      else if (strstr(type, "reflowMinTemp"))
      {
        reflowPeakMinTemp = atof(separator);
      }
      else if (strstr(type, "reflowMaxTemp"))
      {
        reflowPeakMaxTemp = atof(separator);
      }
    }
    // Find the next command in input string
    type = strtok(0, ";");
  }
}

void checkSerialForAbort()
{
  char input[READ_SIZE + 1];
  byte size = Serial.readBytesUntil('\n', input, READ_SIZE);
  input[size] = 0;

  if (strstr(input, "Stop") == input)
  {
    stopFromSerial = HIGH;
  }
}

void waitForStart()
{
  while (!startFromSerial)
  {
    digitalWrite(LampPin, HIGH);
    updateDataAndDisplay();
    readFromSerial();
    delay(500);
  }
  startFromSerial = LOW;
}

void preheat()
{
  phaseStart = millis();
  while (!stopFromSerial && (ptTop < soakStartTemp || ptBott < soakStartTemp))
  {
    message = String("Preheating ") + String(float(millis() - phaseStart) / 1000.0, 1) + String("s");
    updateDataAndDisplay();
    if (tcTop > soakMaxTemp)
    {
      digitalWrite(THeatPin, LOW);
    }
    else
    {
      digitalWrite(THeatPin, HIGH);
    }
    if (tcBott > soakMaxTemp)
    {
      digitalWrite(BHeatPin, LOW);
    }
    else
    {
      digitalWrite(BHeatPin, HIGH);
    }
    delay(200);
    checkSerialForAbort();
  }
}

void soak()
{
  phaseStart = millis();
  while (!stopFromSerial && ((millis() - phaseStart) / 1000) < soakSeconds)
  {
    message = String("Soaking ") + String(float(soakSeconds) - float((millis() - phaseStart) / 1000.0), 1) + String("s");
    updateDataAndDisplay();
    if (tcTop > soakMaxTemp)
    {
      digitalWrite(THeatPin, LOW);
    }
    else
    {
      digitalWrite(THeatPin, HIGH);
    }
    if (tcBott > soakMaxTemp)
    {
      digitalWrite(BHeatPin, LOW);
    }
    else
    {
      digitalWrite(BHeatPin, HIGH);
    }
    delay(200);
    checkSerialForAbort();
  }
}

void rampUp()
{
  phaseStart = millis();
  digitalWrite(THeatPin, HIGH);
  digitalWrite(BHeatPin, HIGH);
  while (!stopFromSerial &&  (ptTop < reflowStartTemp || ptBott < reflowStartTemp))
  {
    message = String("Ramp up ") + String(float(millis() - phaseStart) / 1000.0, 1) + String("s");
    updateDataAndDisplay();
    delay(200);
    checkSerialForAbort();
  }
}

void reflow()
{
  phaseStart = millis();
  while (!stopFromSerial && ((millis() - phaseStart) / 1000) < reflowSeconds)
  {
    message = String("Reflow ") + String(float(reflowSeconds) - float((millis() - phaseStart) / 1000.0), 1) + String("s");
    updateDataAndDisplay();
    if (tcTop > reflowPeakMaxTemp || ptTop > reflowPeakMinTemp)
    {
      digitalWrite(THeatPin, LOW);
    }
    else if (ptTop < reflowStartTemp)
    {
      digitalWrite(THeatPin, HIGH);
    }
    if (tcBott > reflowPeakMaxTemp || ptBott > reflowPeakMinTemp)
    {
      digitalWrite(BHeatPin, LOW);
    }
    else if (ptBott < reflowStartTemp)
    {
      digitalWrite(BHeatPin, HIGH);
    }
    delay(200);
    checkSerialForAbort();
  }
}

void cooldown()
{
  phaseStart = millis();
  digitalWrite(THeatPin, LOW);
  digitalWrite(BHeatPin, LOW);
  message = String("Open door a bit");
  tone(BuzzerPin, 1000);
  while (ptTop > reflowStartTemp && ptBott > reflowStartTemp)
  {
    updateDataAndDisplay();
    delay(200);
  }
  noTone(BuzzerPin);
  digitalWrite(FanPin, HIGH);
  message = String("Cooling");
  while (ptTop > reflowStartTemp && ptBott > reflowStartTemp)
  {
    updateDataAndDisplay();
    delay(200);
  }
  while (ptTop > 40.0 && ptBott > 40.0)
  {
    updateDataAndDisplay();
    delay(200);
  }
  message = String("Reflow complete");
  if (stopFromSerial)
  {
    stopFromSerial = LOW;
    message = "Aborted";
  }
  tone(BuzzerPin, 1000);
  updateDataAndDisplay();
  delay(500);
  noTone(BuzzerPin);
  updateDataAndDisplay();
  delay(500);
  tone(BuzzerPin, 1000);
  updateDataAndDisplay();
  delay(500);
}

void loop()
{
  digitalWrite(LampPin, LOW);
  digitalWrite(FanPin, LOW);
  digitalWrite(THeatPin, LOW);
  digitalWrite(BHeatPin, LOW);
  noTone(BuzzerPin);
  //lcd.begin();
  //lcd.noCursor();
  //lcd.setcontrast(25);
  waitForStart();
  preheat();
  soak();
  rampUp();
  reflow();
  cooldown();
  delay(2000);
}
