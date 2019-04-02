
// Remote soil moisture 4-sensor node based on Vinduino R3
// Using LM-20 LoRa module, PCF8563 RTC, DS18B20
// Added galvanic voltage bias compensation
// Date April 10, 2016
// Reinier van der Lee, www.vanderleevineyard.com
// For Vinduino3_PV@OpenVino.org

#include <Wire.h>                 // I2C communication to RTC
#include "LowPower.h"             // Needed for sleep mode
#include <math.h>                 // Conversion equation from resistance to %
#include <OneWire.h>
#include <DallasTemperature.h>

//set up temp sensor
#define ONE_WIRE_BUS 12 
OneWire ourWire(ONE_WIRE_BUS);
DallasTemperature sensors(&ourWire);

#define PCF8563address 0x51       // Format data for RTC
byte timercontrol, timervalue;    // for interrupt timer

//Use pin 2 for wake up from sleep mode interrupt
const int wakeUpPin = 2;

typedef struct {        // Structure to be used in percentage and resistance values matrix to be filtered (have to be in pairs)
  int moisture;
  int resistance;
} values;

char loraMac [] = "V55XYQX29JTFWNVR"; // ThingSpeak API key

// Setting up format for reading 4 soil sensors
#define NUM_READS 10   // Number of sensor reads for filtering

const long knownResistor = 4750;  // Value of R9 and R10 in Ohms, = reference for sensor

unsigned long supplyVoltage;      // Measured supply voltage
unsigned long sensorVoltage;      // Measured sensor voltage
int tension1;
int tension2;
int tension3;
int tension4;
int zeroCalibration = 115;        // calibrate sensor resistace to zero when input is short circuited
                                  // basically this is compensating for the mux switch resistance

values valueOf[NUM_READS];        // Calculated  resistances to be averaged
long buffer[NUM_READS];
int index2;
int i;                            // Simple index variable
int j=0;                          // Simple index variable

void setup() {
  
  // initialize serial communications at 9600 bps:
  Serial.begin(9600);             // initialize LoRa module communications
  Wire.begin();                   // initialize I2C communications

  // Configure wake up pin as input.
  // This will consume few uA of current.
pinMode(wakeUpPin, INPUT);  

// setting up the LM-210 LoRa module control pins:
  pinMode(3, OUTPUT); // module P1 pin
  pinMode(4, OUTPUT); // module P2 pin

  // P1=0 and P2=0 : module active mode (Mode 1)
  // P1=0 and P2=1 : module wake-up mode (Mode 2)
  // P1=1 and P2=0 : module power saving mode (Mode 3)
  // P1=1 and P2=1 : module set-up mode (Mode 4)
  
// setting up the sensor interface
  // initialize digital pins D5, D6 as an high impedance input.
  // Pin 5,6 are for driving the soil moisture sensor
  pinMode(5, INPUT);    
  pinMode(6, INPUT); 
  // Pin 7 is for enabling Mux switches
  pinMode(7, OUTPUT); 
  // Pin 8,9 are for selecting sensor 1-4
  pinMode(8, OUTPUT);  // Mux input A
  pinMode(9, OUTPUT);  // Mux input B 
 
  // Pin 13 is used as WiFi radio and aux power enable
 pinMode(13, OUTPUT);  


  digitalWrite(3, LOW); // module P1 pin
  digitalWrite(4, LOW); // module P2 pin

  soilsensors(); 

  delay (1000);

  
}


void loop() 
{

  digitalWrite(3, HIGH); // module P1 pin
  digitalWrite(4, HIGH); // module P2 pin
  // P1 (strap)=1 and P2=1 : module power setup mode (Mode 4)
    
   settimerPCF8563();      // set the timer delay and alarm
  
   digitalWrite(13, LOW); // LED OFF

   // Allow wake up pin to trigger interrupt on low.
   attachInterrupt(0, wakeUp, LOW);
    
   // Enter power down state with ADC and BOD module disabled.
   // Wake up when wake up pin is low.
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);     

   // Disable external pin interrupt on wake up pin.
  detachInterrupt(0); 

    soilsensors(); 

    

delay (1000);

}

void soilsensors(){

// Select sensor 1, and enable MUX
  digitalWrite(8, LOW); 
  digitalWrite(9, LOW); 
  digitalWrite(7, LOW); 
  measureSensor();
  
  unsigned long read1 = average();
double tension1 = average();  //use this line to display uncalibrated sensor resistance
if (read1 > 35000){tension1=255;}
if (read1 > 1000 && read1 <=35000){tension1=-0.000000000003*pow(read1,3)+0.0000002*pow(read1,2)+(0.0037*read1)+6.73;}
if (read1 <= 1000){tension1=(read1-400)/60;}
if (read1 < 400){tension1=0;}
read1 = tension1;

// Select sensor 2, and enable MUX
  digitalWrite(8, LOW); 
  digitalWrite(9, HIGH); 
  digitalWrite(7, LOW); 
  measureSensor();
  unsigned long read2 = average();
double tension2 = average();  //use this line to display uncalibrated sensor resistance
if (read2 > 35000){tension2=255;}
if (read2 >1000 && read2 <=35000){tension2=-0.000000000003*pow(read2,3)+0.0000002*pow(read2,2)+(0.0037*read2)+6.73;}
if (read2<=1000){tension2=(read2-400)/60;}
if (read2 <400){tension2=0;}
read2 = tension2;

    // Select sensor 3, and enable MUX
  digitalWrite(8, HIGH); 
  digitalWrite(9, LOW); 
  digitalWrite(7, LOW); 
  measureSensor();
  unsigned long read3 = average();
double tension3 = average();  //use this line to display uncalibrated sensor resistance
if (read3 > 35000){tension3=255;}
if (read3 >1000 && read3 <=35000){tension3=-0.000000000003*pow(read3,3)+0.0000002*pow(read3,2)+(0.0037*read3)+6.73;}
if (read3<=1000){tension3=(read3-400)/60;}
if (read3 <400){tension3=0;}
read3 = tension3;

  // Select sensor 4, and enable MUX
  digitalWrite(8, HIGH); 
  digitalWrite(9, HIGH); 
  digitalWrite(7, LOW); 
  measureSensor();
  unsigned long read4 = average();
double tension4 = average();  //use this line to display uncalibrated sensor resistance
if (read4 > 35000){tension4=255;}
if (read4 >1000 && read4 <=35000){tension4=-0.000000000003*pow(read4,3)+0.0000002*pow(read4,2)+(0.0037*read4)+6.73;}
if (read4<=1000){tension4=(read4-400)/60;}
if (read4 <400){tension4=0;}
read4 = tension4; 

  float Vsys = analogRead(3)*0.00647;   // read the battery voltage



 digitalWrite(4, LOW); // module P2 pin
 digitalWrite(3, LOW); // module P1 pin
 digitalWrite(13, HIGH); // LED ON
   delay (50);

  //Read DS18B20 sensor
  float temp;
  float humidity;
  sensors.requestTemperatures();      // Send the command to get temperatures

  // temp = (sensors.getTempFByIndex(0)); // Degrees F
  temp = (sensors.getTempCByIndex(0)); // Degrees C
  humidity = 0;

if (Serial.available() > 0)
  {
  int randNumber=random(5000, 10000);
  delay (randNumber);}

  //Print/send results
  Serial.print("$");
  Serial.print(",");
  Serial.print(loraMac);
  Serial.print(",");
  Serial.print(read1);
  Serial.print(",");
  Serial.print(read2);
  Serial.print(",");
  Serial.print(read3);
  Serial.print(",");
  Serial.print(read4);
  Serial.print(",");
  Serial.print(Vsys,2);
  Serial.print(",");
  Serial.print(temp);
  Serial.print(",");
  Serial.print(humidity);
  Serial.println(",!");
  delay (1000);
  

  digitalWrite(13, LOW); //  LED off

  return;
}

void measureSensor()
{

  for (i=0; i<NUM_READS; i++) 
      {
  
    pinMode(5, OUTPUT); 
    digitalWrite(5, LOW);  
    digitalWrite(5, HIGH); 
    delayMicroseconds(250);
    sensorVoltage = analogRead(0);   // read the sensor voltage
    supplyVoltage = analogRead(1);   // read the supply voltage
    digitalWrite(5, LOW); 
    pinMode(5, INPUT);
    long resistance = (knownResistor * (supplyVoltage - sensorVoltage ) / sensorVoltage)-zeroCalibration ;
    
   addReading(resistance);
   delayMicroseconds(250);

    pinMode(6, OUTPUT); 
    digitalWrite(6, LOW);  
    digitalWrite(6, HIGH); 
    delayMicroseconds(250);
    sensorVoltage = analogRead(1);   // read the sensor voltage
    supplyVoltage = analogRead(0);   // read the supply voltage
    digitalWrite(6, LOW); 
    pinMode(6, INPUT);
    resistance = (knownResistor * (supplyVoltage - sensorVoltage ) / sensorVoltage)-zeroCalibration ;
    
   addReading(resistance);
   delay(100);

      } 
}


// Averaging algorithm
void addReading(long resistance){
  buffer[index2] = resistance;
  index2++;
  if (index2 >= NUM_READS) index2 = 0;
}
  long average(){
  long sum = 0;
  for (int i = 0; i < NUM_READS; i++){
    sum += buffer[i];
  }
  return (long)(sum / NUM_READS);
}


void wakeUp()
{
    // Just a handler for the pin interrupt.
}

// RTC routines below

void PCF8563alarmOff()
// turns off alarm enable bits and wipes alarm registers. 
{
  byte test;
  // first retrieve the value of control register 2
  Wire.beginTransmission(PCF8563address);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(PCF8563address, 1);
  test = Wire.read();

  // set bit 2 "alarm flag" to 0
  test = test - B00000100;

  // now write new control register 2  
  Wire.beginTransmission(PCF8563address);
  Wire.write(0x01);
  Wire.write(test);
  Wire.endTransmission();
}


void checkPCF8563alarm()    // checks if the alarm has been activated
{
  byte test;
  // get the contents from control register #2 and place in byte test;
  Wire.beginTransmission(PCF8563address);
  Wire.write(0x01);
  Wire.endTransmission();
  Wire.requestFrom(PCF8563address, 1);
  test = Wire.read();
  test = test & B00000100 ;// read the timer alarm flag bit
  
  if (test == B00000100)// alarm on?
  {digitalWrite(13, HIGH);   // turn pin 13 and Radio module on (HIGH is the voltage level)
   delay(50);
   PCF8563alarmOff(); }      // turn off the alarm
  
}

void settimerPCF8563()
// this sets the timer from the PCF8563
{
Wire.beginTransmission(PCF8563address);
Wire.write(0x0E); // move pointer to timer control address

Wire.write(0x83); // sends 0x83 Hex 010000011 (binary)
// (0x82 = enable timer,timer clock set to 1 second)
// (0x83 = enable timer,timer clock set to 1 minute)
Wire.endTransmission();
 
Wire.beginTransmission(PCF8563address);
Wire.write(0x0F); // move pointer to timer value address
Wire.write(0x0F); // sends delay time value (0F =15 minutes, 3C = 1 hour)
Wire.endTransmission();

// optional - turns on INT_ pin when timer activated
// will turn off once you run void PCF8563alarmOff()
Wire.beginTransmission(PCF8563address);
Wire.write(0x01); // select control status_register
Wire.write(B00000001); // set Timer INT enable bit
Wire.endTransmission();
}

