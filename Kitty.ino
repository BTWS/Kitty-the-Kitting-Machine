// SPECIAL THANKS TO:
// --PinChangeInt Library--
// https://github.com/GreyGnome/PinChangeInt
// -- HobbyComponents (HCARDU0035)--
// http://forum.hobbycomponents.com/viewtopic.php?f=93&t=1323
// -- Adafruit NeoPixel 
// https://github.com/adafruit/Adafruit_NeoPixel --
// -- YourDuino Example: Relay Control 1.10 --
// https://arduino-info.wikispaces.com/ArduinoPower
// -- Arduino Lesson 4. Eight LEDs and a Shift Register --
// https://learn.adafruit.com/adafruit-arduino-lesson-4-eight-leds/overview
// -- TimerOne --
// http://playground.arduino.cc/Code/Timer1

//----------------
// -- LIBRARIES --
//----------------
#include <PinChangeInt.h>       // Get rising and falling edge functionality in software
#include <Adafruit_NeoPixel.h>  
#include <Wire.h>               // Communication with RPi
#include <TimerOne.h>

/*------------NEOPIXELS------------*/
#define ARDUINOPIN 11
#define NUMPIXELS 50
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, ARDUINOPIN, NEO_GRB + NEO_KHZ800);
int modulo = 0;

/*--------------MOTORS--------------*/
#define RELAY_ON 0
#define RELAY_OFF 1

byte MotorID[] = {2,3,4,5,6,7};

/*------------SHIFT REGISTERS------------*/
// Rotate Left Through Carry
#define ROL(X)  (X << 1) | (X >> 7)
// Rotate Right Through Carry
#define ROR(X)  (X >> 1) | (X << 7)

//int motorPin1 = 9;

int dataPin = 8; //bovenste op schema
int latchPin = 9; //2de gele
int clockPin = 10; //onderste oranje

byte leds = 0B00010001; //pattern 0b00000001

//volatile byte signalCounter = 0; //Track signal for motor driver

/*--------------Counters--------------*/
//int counterPins[] = {2,3};              // Arduino Uno internal interrupt pins //(only 2 counters)
int counterPins[] = {13,14,15,16,17,18};   // Arduino Uno external interrupt pins (using Library almost all pins)
//int counterPins[] ={2,3,18,19,20,21}; // Arduino Mega internal interrupt pins //(only 6 counters)
volatile unsigned int counter = 0;      // Make variables used int interrupt routines volatile                                      

/*--------------I2C--------------*/
#define SLAVE_ADDRESS 0x04

/*--------------DATA--------------*/
int number = 0;           // Received I2C data
int mode = 0;             // Starting in Idle
#define IDLEMODE 0        //Do nothing
#define RECEIVINGMODE 1   //Receiving
#define DONE 2            //Done Receiving

int type = 0; // Starting in no type
#define NOTYPE 0
#define LEDTYPE 1
#define PARTTYPE 2

int i = 0; // Number of parameters received

char ledArray[6] = {'0','0','0','0','0','0'};   // L, ID[0], ID[1], R, G, B      //Example: L42123 = Turn led 42 on with low Red, moderate Green and high Blue
char partArray[4]  = {'0','0','0','0'};  // P, ID, Number[0], Number[1] //Example: P416 = Turn motor 4 until there are 16 screws

#define DEBUG 1 // Set 1 if you want debug info

void setup (void)
{
  Serial.begin(9600); // Debugging
  mode = IDLEMODE;
  
  // Initialize motorPins
  for( int i = 0; i < sizeof(MotorID); i++) //sizeof(byte)
  {
    digitalWrite(MotorID[i], RELAY_OFF);
  }
  for( int i = 0; i < sizeof(MotorID); i++) //sizeof(byte)
  {
    pinMode(MotorID[i], OUTPUT);
  }
  
  // Initialize counterPins
  for(int i = 0; i< sizeof(counterPins)/sizeof(int); i++)
  {
    pinMode(counterPins[i], INPUT_PULLUP);  // Configure the pin as an input, and turn on the pullup resistor.
    attachPinChangeInterrupt(counterPins[i], blink, RISING);
  }
  
  // Neopixels
  pixels.begin(); // This initializes the NeoPixel library.
  
  // Init i2C and declare callbacks
  //Wire.begin(SLAVE_ADDRESS);
  //Wire.onReceive(receiveData);
  //Wire.onRequest(sendData);
  
  // Timer1 setup
  Timer1.initialize(15000); // set a timer of length 2500 microseconds
  Timer1.attachInterrupt( timerIsr ); // attach the service routine here
  
   pinMode(latchPin, OUTPUT);
   pinMode(dataPin, OUTPUT);  
   pinMode(clockPin, OUTPUT);
   
   colorWipe(pixels.Color(255, 0, 0), 50); // Red
   colorWipe(pixels.Color(0, 255, 0), 50); // Green
   colorWipe(pixels.Color(0, 0, 255), 50); // Blue
   
   turnAllPixelsOff();
  
  Serial.println("Setup Completed");
}

// I2C receive data routine
void serialEvent(){ //receiveData(int byteCount)
  while(Serial.available()) //Wire.available
  {
    number = Serial.read(); //Wire.read
    Serial.print("Received: ");
    //Serial.println(number, BIN);
    Serial.println((char)number);
    
    // Set mode
    if( (char)number == 'L')
    {
      type = LEDTYPE;
      //Serial.println("LED MODE detected");
    }
    else if( (char)number == 'P')
    {
      type = PARTTYPE;
      //Serial.println("Part MODE detected");
    } 
    else if (type != LEDTYPE || type != PARTTYPE)
    {
      if(mode = IDLEMODE)
      {
        Serial.println("Idle MODE");
      }
      else if(mode = RECEIVINGMODE)
      {
        Serial.println("Receiver MODE");
      }
      else
      {
        Serial.println("Invalid MODE");
      }
       
    }
    
    // Count 
    if(type == LEDTYPE)
    {
      if(i < sizeof(ledArray))
      {
        mode = RECEIVINGMODE;
        ledArray[i] = (char)number;
        Serial.print("ledArray[i]:  ");
        Serial.println((char)number);
        i++;
      }
      if(i >= sizeof(ledArray))
      {
        Serial.println("Everything received");
        Serial.print("The i was... ");Serial.println(i);
        mode = DONE;
        i = 0; // Set back to normal
      }
    }
    else if(type == PARTTYPE)
    {
      if(i < sizeof(partArray))
      {
        mode = RECEIVINGMODE;
        partArray[i] = (char)number;
        i++;
      }
      if(i >= sizeof(partArray))
      {
        mode = DONE;
        i = 0; // Set back to normal
      }
    }   
  }
}

void loop()
{
  
  //Check if there is a valid ID
  if(mode == DONE && type == LEDTYPE) //Array[0] = L
  {
    setLeds((convC2N(ledArray[1])*10 + convC2N(ledArray[2])), convC2N(ledArray[3]), convC2N(ledArray[4]),convC2N(ledArray[5]));
  }
  if(mode == DONE && type == PARTTYPE) //Array[0] = M
  {
    countParts(convC2N(partArray[1]),(convC2N(partArray[2]) *10 + convC2N(partArray[3])));
  }
  else if(mode == RECEIVINGMODE)
  {
    Serial.println("Receiving..."); 
    if(DEBUG)
    {
      printCharArray(ledArray, 6);
      printCharArray(partArray, 4);
    }
  }
  else if( mode == DONE)
  {
    Serial.println("Receiving DONE");    
  }
  else if(mode == IDLEMODE)
  {   
    Serial.println("Everything is done here."); 
    delay(1000);
  }  
  else
  {
    if(DEBUG)
    {
      printCharArray(ledArray, 6);
      printCharArray(partArray, 4);
    }
  }
  delay(1000);
}


void setLeds(byte ID, byte R, byte G, byte B)
{
  if(ID-1 >= NUMPIXELS)
  {
    Serial.println("Index out of range in LED's");
    Serial.print(ID); Serial.print(" is greater than the available number of leds: "); Serial.println(NUMPIXELS);
    
    //Reset ID and go in default state
    ledArray[1] = '0';
    ledArray[2] = '0';
  }
  else
  {
    if(DEBUG){
      Serial.println("Setting leds");
      Serial.print("ID = "); Serial.println(ID);
      Serial.print("R = "); Serial.println(R);
      Serial.print("G = "); Serial.println(G);
      Serial.print("B = "); Serial.println(B);
      } 
    
    int R2 = map(R,0,9,0,255);
    int G2 = map(G,0,9,0,255);
    int B2 = map(B,0,9,0,255);
    
    pixels.setPixelColor(ID-1, pixels.Color(R2,G2,B2));
    pixels.show(); // This sends the updated pixel color to the hardware.
    delay(200); // Delay for a period of time (in milliseconds). 
    //Reset ID and go in default state
    ledArray[1] = '0';
    ledArray[2] = '0';
    mode = IDLEMODE;
    delay(200);
  }
}

void countParts(byte ID, byte howMany)
{
  if(ID-1 >= sizeof(counterPins)/sizeof(int))
  {
   Serial.println("Index out of range in Motors");
    Serial.print(ID); Serial.print(" is greater than the available number of motors: "); Serial.println(sizeof(counterPins)/sizeof(int));
    //Reset ID and go in default state
    partArray[1] = '0';
  }
  else
  {
    
    if(DEBUG){
    Serial.println("Turning motor...");
    Serial.print("ID = "); Serial.println(ID);
    Serial.print("howMany = "); Serial.println(howMany);
    Serial.print("Counter = "); Serial.println(counter);
    }
  
    turnMotorOn(ID-1);
    if( counter >= (int)howMany)
    {
      turnMotorOff(ID-1); //Turn motor off
      //Reset ID and go in default state
      partArray[1] = '0';
      counter = 0; // reset counter
      mode = IDLEMODE;
      Serial.println("Counting done!");
      delay(200);
    }
  }
}

void blink() {   
   //Debounce (don't use delay)
   static unsigned long last_interrupt_time = 0;
   unsigned long interrupt_time = millis();
   // If interrupts come faster than 200ms, assume it's a bounce and ignore
   if (interrupt_time - last_interrupt_time > 200) 
   {
     // Serial.print("Nut detected!");
     counter++;     
   }
   last_interrupt_time = interrupt_time;
}

void turnMotorOn(byte ID)
{
  digitalWrite(MotorID[ID], RELAY_ON);// set the Relay ON
}

void turnMotorOff(byte ID)
{
  digitalWrite(MotorID[ID], RELAY_OFF);// set the Relay ON
}

void turnAllMotorOff(int ID)
{
  for( int i = 0; i < sizeof(MotorID); i++) //sizeof(byte)
  {
    digitalWrite(MotorID[i], RELAY_OFF);
    delay(1000);
  }
}

void timerIsr()
{  
  leds = ROL(leds);
   digitalWrite(latchPin, LOW);
   shiftOut(dataPin, clockPin, LSBFIRST, leds); //'Least Significant Bit' (LSB)
   digitalWrite(latchPin, HIGH);
}

void sendData(){
  if(mode == IDLEMODE)
  {
    Wire.write("DONE");
    Wire.write((char) number);
  }
  else
  {
    Wire.write("BUSY");
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<pixels.numPixels(); i++) {
    modulo = i % 8;
    if((modulo != 1) && (modulo != 5) && (modulo != 7))
     {
      pixels.setPixelColor(i, c);
     }
    pixels.show();
    delay(wait);
  }
}

void turnAllPixelsOff()
{
  for(int i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, pixels.Color(0,0,0));
    pixels.show();
    delay(1);
  }
}


//--HELPFUNCTIONS
void printCharArray(char arr[], int n)
{
  for(int i; i<n; i++)
  {
    Serial.print((char) arr[i]);  
  }
  Serial.println();
}

void printByteArray(byte arr[], int n)
{
  for(int i; i<n; i++)
  {
    Serial.println((char) arr[i]);  
  }
}

byte convC2N(char c) //ConvertChar2Number
{
  return (byte) c - '0';
  //Example of calling the function: printArray(testArray, sizeof(testArray));
}
