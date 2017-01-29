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

#define CURRENT_SENSE 0        // input pin for current measurement
#define OUTPUT_VOLTAGE_SENSE 1 // input pin for output voltage measurement
#define INPUT_VOLTAGE_SENSE 2  // input pin for input voltage measurement

#define ZERO_CURRENT_OFFSET 0  // to set the zero for current sensor

#define RELAY_OUT_1 6  // relay between solar panel 1 and input
#define RELAY_OUT_2 7  // relay between solar panel 2 and input
#define RELAY_1_ON()   digitalWrite(RELAY_OUT_1, HIGH)
#define RELAY_2_ON()   digitalWrite(RELAY_OUT_2, HIGH)
#define RELAY_1_OFF()  digitalWrite(RELAY_OUT_1, LOW)
#define RELAY_2_OFF()  digitalWrite(RELAY_OUT_2, LOW)

#define SHORT_WAIT_TIME   3000 // wait time for main loop when isCurrentlyCharging
#define LONG_WAIT_TIME   30000 // wait time for main loop when not isCurrentlyCharging

const float MINIMUM_INPUT_VOLTAGE = 10.5;   // minimal input voltage required to attempt to charge
const float DESIRED_BATTERY_VOLTAGE = 14.5; // stop isCurrentlyCharging when this output voltage is attained
const float CURRENT_SCALE = 14.0;           // current to voltage convertion rate 66mv/a for ACS712 30A
const float INPUT_VOLTAGE_SCALE = 10.2;     // resistor divider for measuring input voltage relative to +5v
const float OUTPUT_VOLTAGE_SCALE = 4.95;    // resistor divider for measuring output voltage relative to +5v
const float MAX_CURRENT = 10.0;             // total charge controller or battery sink current capacity

boolean sendMeasurements = true;            // report voltage and current measurements as well as watts produced on serial port
boolean isCurrentlyCharging = false;
boolean extraRelayOn = false;

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

void setup() {
  pinMode(LCD_BACKLIGHT, OUTPUT); 
  pinMode(RELAY_OUT_1, OUTPUT);
  pinMode(RELAY_OUT_2, OUTPUT);

  Serial.begin(9600);
  Serial.println(F("Starting"));

  digitalWrite(LCD_BACKLIGHT, HIGH);
  digitalWrite(RELAY_OUT_1, LOW);
  digitalWrite(RELAY_OUT_2, LOW);

  lcd.begin(16, 2);
}

float measureInputVoltage(){
  int voltReading = analogRead(INPUT_VOLTAGE_SENSE);
  return 5 * INPUT_VOLTAGE_SCALE * (voltReading+1) / 1024.0;
}

float measureOutputVoltage(){
  int voltReading = analogRead(OUTPUT_VOLTAGE_SENSE);
  return 5 * OUTPUT_VOLTAGE_SCALE * (voltReading+1) / 1024.0;
}

float measureCurrent(){
  int currentReading = analogRead(CURRENT_SENSE);
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

void measure(float &inputVoltage, float &current, float &outputVoltage, float &watts){
  delay(100);
  inputVoltage = measureInputVoltage();
  outputVoltage = measureOutputVoltage();
  current = measureCurrent();
  watts = calculatePower(current, outputVoltage);
}

void sendMeasurementValues(float inputVoltage, float current, float outputVoltage, float watts){
  Serial.print(inputVoltage);
  Serial.println('V');
  Serial.print(current);
  Serial.println('A');
  Serial.print(outputVoltage);
  Serial.println('V');
  Serial.print(watts);
  Serial.println('W');
  Serial.println();
}

void displayInputVoltage(float voltage){
  lcd.setCursor(0, 0);
  lcd.print(roundAndAdjust(voltage, "V", 3));
}

void displayCurrent(float current){
  lcd.setCursor(8, 0);
  lcd.print(roundAndAdjust(current, "A", 2));
}

void displayOutputVoltage(float voltage){
  lcd.setCursor(0, 1);
  lcd.print(roundAndAdjust(voltage, "V", 3));
}

void displayPower(float watts){
  lcd.setCursor(8, 1);
  lcd.print(roundAndAdjust(watts, "W", 2));
}

boolean openRelays(){
    Serial.println("Enabling inputs");
    RELAY_1_ON();
    RELAY_2_ON();
    extraRelayOn = true;
    return true;
}

boolean closeRelays(){
    Serial.println("Disabling inputs");
    RELAY_1_OFF();
    RELAY_2_OFF();
    extraRelayOn = false;
    return false;
}

void closeExtraRelay(){
    if (extraRelayOn){
      Serial.println("Closing extra relay");
      RELAY_2_OFF();
      extraRelayOn = false;
    }
}

void openExtraRelay(){
    if (!extraRelayOn){
      Serial.println("Opening extra relay");
      RELAY_2_ON();
      extraRelayOn = true;
    }
}

void displayMeasurements(float inputVoltage, float current, float outputVoltage, float watts){
  if (sendMeasurements){
    sendMeasurementValues(inputVoltage, current, outputVoltage, watts);
  }
  lcd.clear();
  displayInputVoltage(inputVoltage);
  displayOutputVoltage(outputVoltage);
  displayCurrent(current);
  displayPower(watts);
}

void loop() {
    float inputVoltage;
    float outputVoltage;
    float current;
    float watts;
    int sleepTime;
    
    measure(inputVoltage, current, outputVoltage, watts);
    displayMeasurements(inputVoltage, current, outputVoltage, watts);
    
    if (outputVoltage < DESIRED_BATTERY_VOLTAGE){
      if (isCurrentlyCharging){
        Serial.println(F("charging"));
      }else{
        Serial.println(F("battery needs charging"));
      }
      if ((inputVoltage > MINIMUM_INPUT_VOLTAGE) && (inputVoltage > outputVoltage)) {
          if (!isCurrentlyCharging){
            isCurrentlyCharging = openRelays();
            measure(inputVoltage, current, outputVoltage, watts);
            displayMeasurements(inputVoltage, current, outputVoltage, watts);
          }
          if ((current >  MAX_CURRENT)){
            Serial.println(F("current is too high"));
            isCurrentlyCharging = closeRelays();
            measure(inputVoltage, current, outputVoltage, watts);
            displayMeasurements(inputVoltage, current, outputVoltage, watts);
          }else if ((current >  (MAX_CURRENT / 2))){
            Serial.println(F("current is good"));
            closeExtraRelay();
            measure(inputVoltage, current, outputVoltage, watts);
            displayMeasurements(inputVoltage, current, outputVoltage, watts);
          }else{
            Serial.println(F("current is low"));
            openExtraRelay();
            measure(inputVoltage, current, outputVoltage, watts);
            displayMeasurements(inputVoltage, current, outputVoltage, watts);
          }   
      }else{
          Serial.println(F("not enough voltage to charge"));
          if (isCurrentlyCharging){
            isCurrentlyCharging = closeRelays();
          }
      }
    } else {
      Serial.println(F("battery charged"));
      if (isCurrentlyCharging){
        isCurrentlyCharging = closeRelays();
      }
    }
    if(isCurrentlyCharging){
      LCD_BACKLIGHT_ON();
      sleepTime = SHORT_WAIT_TIME;
    }else{
      LCD_BACKLIGHT_OFF();
      sleepTime = LONG_WAIT_TIME;
    }
    
  delay(sleepTime);
}

