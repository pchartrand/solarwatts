#include <LiquidCrystal.h>

#define LCD_BACKLIGHT_PIN      10
#define LCD_BACKLIGHT_OFF()    digitalWrite( LCD_BACKLIGHT_PIN, LOW )
#define LCD_BACKLIGHT_ON()     digitalWrite( LCD_BACKLIGHT_PIN, HIGH )

const int currentIn = 0; //input pin for current measurement
const int voltIn = 1; //input pin for voltage measurement
const int inputRelay = 6; //relay between solar panel and input
const int outputRelay = 7; // relay between output and battery

const int currentOffset = 3;// to set the zero for current sensor
const int delayTime = 3000; //sleep time between measurements
const unsigned int sleepTime = 60000; //sleep time when voltag is low
const float underVoltage = 10.5
; //do not allow battery to go lower than (depends on battery type);
const float overVoltage = 13.5;  //do not allow battery to go higher than
const float currentScale = 14.0; //66mv/a for ACS712 30A
const float voltageScale = 4.95; //resistor divider for measuring voltage > +5v of arduino
const float currentMax = 7.0; //charge controller sink current capacity
const float minPower = 0.5; //minimal power required to wake up the arduino
boolean inputEnabled = false;
boolean outputEnabled = false;
boolean sleeping = false;

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

void setup() {
  pinMode(inputRelay, OUTPUT);
  pinMode(outputRelay, OUTPUT);
  Serial.begin(9600);
  Serial.println("Starting\n");
  lcd.begin(16, 2);
  lcd.print("Bonjour\n");
  digitalWrite( LCD_BACKLIGHT_PIN, HIGH );
  pinMode( LCD_BACKLIGHT_PIN, OUTPUT ); 
}

float getVolts(){
  int voltReading = analogRead(voltIn);
  return 5 * voltageScale * (voltReading+1) / 1024.0;
}

float getAmps(){
  int currentReading = analogRead(currentIn);
  return (currentReading - 512 + currentOffset) / currentScale;
}

String roundAndAdjust(float val, String units, int precision){
  String rv;
  int decimalPlaces = precision - int(log(val));
  if(decimalPlaces < 0) {
    decimalPlaces = 0; 
  }
  rv = String(val, decimalPlaces);
  return rv + units;
}

void sendValues(float current, float voltage, float watts){
  Serial.print(current);
  Serial.println('A');
  Serial.print(voltage);
  Serial.println('V');
  Serial.print(watts);
  Serial.println('W');
  Serial.println();
}

void displayVoltage(float voltage){
  lcd.setCursor(0, 0);
  lcd.print(roundAndAdjust(voltage, "V", 3));
}

void displayCurrent(float current){
  lcd.setCursor(8, 0);
  lcd.print(roundAndAdjust(current, "A", 2));
}

void displayPower(float watts){
  lcd.setCursor(0, 1);
  lcd.print(roundAndAdjust(watts, "W", 2));
}

boolean checkInputVoltage(float voltage){
  boolean voltageOk = true;
  String lcdPrompt = "   ";
  lcd.setCursor(8, 1);
  if (voltage > overVoltage){
    Serial.println("voltage too high");
    lcdPrompt = "Vi+";
    voltageOk = false;
  }
  lcd.print(lcdPrompt);
  return voltageOk;
}

boolean checkCurrent(float current){
  boolean currentOk = true;
  String lcdPrompt = "   ";
  lcd.setCursor(11, 1);
  if (current > currentMax){
    Serial.println("too much current");
    lcdPrompt = "I+";
    currentOk = false;
  }
  lcd.print(lcdPrompt);
  return currentOk;
}

boolean checkOutputVoltage(float voltage){
   boolean voltageOk = true;
  String lcdPrompt = "   ";
  lcd.setCursor(13, 1);
  if (voltage < underVoltage){
    voltageOk = false;
    Serial.println("voltage too low");
    lcdPrompt = "Vo-";
  }
  lcd.print(lcdPrompt);
  return voltageOk;
}

void manageInput(boolean enable){
  if (!inputEnabled && enable){
    Serial.println("Enabling input");
    digitalWrite(inputRelay, HIGH);
    inputEnabled = true;
  }else if (inputEnabled && !enable){
    Serial.println("Disabling input");
    digitalWrite(inputRelay, LOW);
    inputEnabled = false;
  }
}

void manageOutput(boolean enable){
  if (!outputEnabled && enable){
    Serial.println("Enabling output");
    digitalWrite(outputRelay, HIGH);
    outputEnabled = true;
  }else if (outputEnabled && !enable){
    Serial.println("Disabling output");
    digitalWrite(outputRelay, LOW);
    outputEnabled = false;
  }
}

void wait(float watts){
  if(abs(watts)  > minPower){
    delay(delayTime);  
    if(sleeping == true){
      Serial.println("waking up");
      LCD_BACKLIGHT_ON();
      lcd.display();
    }
    sleeping = false;
  }else{
    if(sleeping == false){
      Serial.println("Going to sleep");
      lcd.noDisplay();
      LCD_BACKLIGHT_OFF();
    }
    sleeping = true;
    delay(sleepTime);
  }
}

void loop() {
  float voltage = getVolts();
  float current = getAmps();
  float watts = current * voltage;
  boolean vin;
  boolean iin;
  boolean inputEnable;
  boolean outputEnable;
  sendValues(current, voltage, watts);
  lcd.clear();
  displayVoltage(voltage);
  displayCurrent(current);
  displayPower(watts);
  vin = checkInputVoltage(voltage);
  iin = checkCurrent(current);
  if ((vin == false) || (iin == false)){
    inputEnable = false;
  }else{
    inputEnable = true;
  }

  manageInput(inputEnable);  

  outputEnable = checkOutputVoltage(voltage);
  manageOutput(outputEnable);
  
  wait(watts);

}

