#include <LiquidCrystal.h>

#define LCD_D7 2
#define LCD_D6 3
#define LCD_D5 4
#define LCD_D4 5
#define LCD_E 11
#define LCD_RS 12
#define LCD_BACKLIGHT 10
#define LCD_BACKLIGHT_OFF()    digitalWrite( LCD_BACKLIGHT, LOW )
#define LCD_BACKLIGHT_ON()     digitalWrite( LCD_BACKLIGHT, HIGH )

#define CURRENT_SENSE_ONE 0        // input pin for output current measurement (panel 1)
#define OUTPUT_VOLTAGE_SENSE 1     // input pin for output voltage measurement
#define INPUT_VOLTAGE_SENSE_ONE 2  // input pin for input voltage measurement (panel 1)

#define CURRENT_SENSE_TWO 3        // input pin for output current measurement (panel 2)
#define INPUT_VOLTAGE_SENSE_TWO 4  // input pin for input voltage measurement (panel 2)

#define ZERO_CURRENT_OFFSET 0  // to set the zero for output current sensor

#define RELAY_OUT_ONE 6  // relay between solar panel 1 and input
#define RELAY_OUT_TWO 7  // relay between solar panel 2 and input
#define RELAY_ONE_ON()   digitalWrite(RELAY_OUT_ONE, HIGH)
#define RELAY_TWO_ON()   digitalWrite(RELAY_OUT_TWO, HIGH)
#define RELAY_ONE_OFF()  digitalWrite(RELAY_OUT_ONE, LOW)
#define RELAY_TWO_OFF()  digitalWrite(RELAY_OUT_TWO, LOW)

#define SHORT_WAIT_TIME   3000 // wait time for main loop when isCurrentlyCharging
#define LONG_WAIT_TIME   90000 // wait time for main loop when not isCurrentlyCharging

const float MINIMUM_INPUT_VOLTAGE = 10.5;   // minimal input voltage required to attempt to charge
const float MAXIMUM_BATTERY_VOLTAGE = 14.5; // stop charging when this output voltage is attained
const float MINIMUM_VOLTAGE_DIFFERENCE = 4.0;// minumum voltage difference between input and output
const float CURRENT_SCALE = 14.0;           // output current to voltage convertion rate 66mv/a for ACS712 30A
const float INPUT_VOLTAGE_SCALE = 10.2;     // resistor divider for measuring input voltage relative to +5v
const float OUTPUT_VOLTAGE_SCALE = 4.95;    // resistor divider for measuring output voltage relative to +5v
const float MAX_CURRENT = 20.0;             // total charge controller or battery sink current capacity
const float MIN_CURRENT = 0.2;              // 1% of max output current

boolean sendMeasurements = true;            // report voltage and current measurements as well as power produced on serial port
boolean relayOneOn = false;
boolean relayTwoOn = false;

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

void setup() {
  pinMode(LCD_BACKLIGHT, OUTPUT); 
  pinMode(RELAY_OUT_ONE, OUTPUT);
  pinMode(RELAY_OUT_TWO, OUTPUT);

  Serial.begin(9600);
  Serial.println(F("Starting"));

  LCD_BACKLIGHT_ON();
  RELAY_ONE_OFF();
  RELAY_TWO_OFF();

  lcd.begin(16, 2);
}

float measureinputVoltage(int voltageSense){
  int voltReading = analogRead(voltageSense);
  return 5 * INPUT_VOLTAGE_SCALE * (voltReading+1) / 1024.0;
}

float measureOutputVoltage(){
  int voltReading = analogRead(OUTPUT_VOLTAGE_SENSE);
  return 5 * OUTPUT_VOLTAGE_SCALE * (voltReading+1) / 1024.0;
}

float measureCurrent(int outputCurrentSense){
  int currentReading = analogRead(outputCurrentSense);
  return (currentReading - 512 + ZERO_CURRENT_OFFSET) / CURRENT_SCALE;
}

float calculatePower(float current, float voltage){
  return current * voltage;
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

void measure(int voltageSense, int currentSense, float outputVoltage, float &inputVoltage, float &outputCurrent){
  delay(100);
  inputVoltage = measureinputVoltage(voltageSense);
  outputCurrent = measureCurrent(currentSense);
}

void sendMeasurementValues(
  float inputVoltageOne, 
  float inputVoltageTwo, 
  float outputVoltage, 
  float outputCurrentOne, 
  float outputCurrentTwo, 
  float outputPowerOne,  
  float outputPowerTwo
  ){
  Serial.print(inputVoltageOne);
  Serial.println('V');
  Serial.print(outputCurrentOne);
  Serial.println('A');
  Serial.print(outputPowerOne);
  Serial.println('W');
  Serial.print(inputVoltageTwo);
  Serial.println('V');
  Serial.print(outputCurrentTwo);
  Serial.println('A');
  Serial.print(outputPowerTwo);
  Serial.println('W');
  Serial.print(outputVoltage);
  Serial.println('V');
  Serial.println();
}

void displayVoltage(float voltage, int offset){
  lcd.setCursor(offset, 0);
  lcd.print(roundAndAdjust(voltage, "V", 3));
}

void displayPower(float outputPower, int offset){
  lcd.setCursor(offset, 1);
  lcd.print(roundAndAdjust(outputPower, "W", 2));
}

void displayMeasurements(
  float outputVoltage, 
  float inputVoltageOne, 
  float outputPowerOne, 
  float outputCurrentOne,
  float inputVoltageTwo, 
  float outputCurrentTwo,
  float outputPowerTwo
  ){
  if (sendMeasurements){
    sendMeasurementValues(inputVoltageOne, inputVoltageTwo, outputVoltage, outputCurrentOne, outputCurrentTwo, outputPowerOne, outputPowerTwo);
  }
  lcd.setCursor(0,0);
  lcd.print(F("               "));
  displayVoltage(inputVoltageOne, 0);
  displayVoltage(inputVoltageTwo, 5);
  displayVoltage(outputVoltage, 10);
  lcd.setCursor(0, 1);
  lcd.print(F("               "));
  displayPower(outputPowerOne, 0);
  displayPower(outputPowerOne, 5);
  displayPower((outputPowerOne + outputPowerTwo), 10);
}

int manageBacklight(){
    int sleepTime;
    if((relayOneOn) ||(relayTwoOn)){
      LCD_BACKLIGHT_ON();
      sleepTime = SHORT_WAIT_TIME;
    }else{
      LCD_BACKLIGHT_OFF();
      sleepTime = LONG_WAIT_TIME;
    }
    return sleepTime;
}


boolean tryInput(
  int relayOut, 
  float outputVoltage, 
  float &inputVoltage,
  float &outputCurrent
  ){
  boolean relayOn = false;
  if (inputVoltage > outputVoltage + MINIMUM_VOLTAGE_DIFFERENCE){
    Serial.println(F("input voltage seems ok before load"));
    digitalWrite(relayOut, HIGH);
    relayOn = true;
    measure(INPUT_VOLTAGE_SENSE_ONE, CURRENT_SENSE_ONE, outputVoltage, inputVoltage, outputCurrent);
  }else{
    Serial.println(F("input voltage is too low"));
  }
  return relayOn;
}

void loop() {
    float outputVoltage;
    float inputVoltageOne;
    float outputCurrentOne;
    float outputPowerOne;
    float inputVoltageTwo;
    float outputCurrentTwo;
    float outputPowerTwo;
    int sleepTime;
    outputVoltage = measureOutputVoltage();
    
    measure(INPUT_VOLTAGE_SENSE_ONE, CURRENT_SENSE_ONE, outputVoltage, inputVoltageOne, outputCurrentOne);
    outputPowerOne = calculatePower(outputCurrentOne, (inputVoltageOne - outputVoltage));

    measure(INPUT_VOLTAGE_SENSE_TWO, CURRENT_SENSE_TWO, outputVoltage, inputVoltageTwo, outputCurrentTwo);
    outputPowerTwo = calculatePower(outputCurrentTwo, (inputVoltageTwo - outputVoltage));

    displayMeasurements(inputVoltageOne, inputVoltageTwo, outputVoltage, outputPowerOne, outputPowerTwo, outputCurrentOne, outputCurrentTwo);
    if (outputVoltage < MAXIMUM_BATTERY_VOLTAGE){
      if (relayOneOn || relayTwoOn){
        Serial.println(F("charging"));
      }else{
        Serial.println(F("battery needs charging"));
      }
      
      if (inputVoltageOne > MINIMUM_INPUT_VOLTAGE) {
          if (!relayOneOn){
            relayOneOn = tryInput(RELAY_OUT_ONE, outputVoltage, inputVoltageOne, outputCurrentOne);
            outputPowerOne = calculatePower(outputCurrentOne, (inputVoltageOne - outputVoltage));
          }
          if ((outputCurrentOne >  MAX_CURRENT)){
            Serial.println(F("output current from panel one is too high"));
            RELAY_ONE_OFF();
            relayOneOn = false;
            measure(INPUT_VOLTAGE_SENSE_ONE, CURRENT_SENSE_ONE, outputVoltage, inputVoltageOne, outputCurrentOne);
            outputPowerOne = calculatePower(outputCurrentOne, (inputVoltageOne - outputVoltage));
          }else if (outputCurrentOne > MIN_CURRENT) {
            Serial.println(F("output current from panel one is ok"));
            measure(INPUT_VOLTAGE_SENSE_ONE, CURRENT_SENSE_ONE, outputVoltage, inputVoltageOne, outputCurrentOne);
            outputPowerOne = calculatePower(outputCurrentOne, (inputVoltageOne - outputVoltage));
          } else{
            Serial.println(F("output current from panel one is to low"));
            if (relayOneOn){
              RELAY_ONE_OFF();
              relayOneOn = false;
            }
          }  
      }else{
          Serial.println(F("not enough voltage from panel one to charge"));
          if (relayOneOn){
              RELAY_ONE_OFF();
              relayOneOn = false;
            }
      }

      if (inputVoltageTwo > MINIMUM_INPUT_VOLTAGE) {
          if (!relayTwoOn){
            relayTwoOn = tryInput(RELAY_OUT_TWO, outputVoltage, inputVoltageTwo, outputCurrentTwo);
            outputPowerTwo = calculatePower(outputCurrentTwo, (inputVoltageTwo - outputVoltage));
          }
          if ((outputCurrentTwo >  MAX_CURRENT)){
            Serial.println(F("output current from panel two is too high"));
            RELAY_TWO_OFF();
            relayTwoOn = false;
            measure(INPUT_VOLTAGE_SENSE_TWO, CURRENT_SENSE_TWO, outputVoltage, inputVoltageTwo, outputCurrentTwo);
            outputPowerTwo = calculatePower(outputCurrentTwo, (inputVoltageTwo - outputVoltage));
          }else if (outputCurrentOne > MIN_CURRENT) {
            Serial.println(F("output current from panel two is ok"));
            measure(INPUT_VOLTAGE_SENSE_TWO, CURRENT_SENSE_TWO, outputVoltage, inputVoltageOne, outputCurrentOne);
            outputPowerTwo = calculatePower(outputCurrentTwo, (inputVoltageTwo - outputVoltage));
          } else{
            Serial.println(F("output current from panel two is to low"));
            if (relayTwoOn){
              RELAY_TWO_OFF();
              relayTwoOn = false;
            }
          }  
      }else{
          Serial.println(F("not enough voltage from panel two to charge"));
          if (relayTwoOn){
            RELAY_TWO_OFF();
            relayTwoOn = false;
          }
      }
      displayMeasurements(inputVoltageOne, inputVoltageTwo, outputVoltage, outputPowerOne, outputPowerTwo, outputCurrentOne, outputCurrentTwo);
      
    } else {
      Serial.println(F("battery charged"));
      if (relayOneOn){
        RELAY_ONE_OFF();
        relayOneOn = false;
      }
      if (relayTwoOn){
        RELAY_TWO_OFF();
        relayTwoOn = false;
      }
    }
    sleepTime = manageBacklight();
    
  delay(sleepTime);
}

