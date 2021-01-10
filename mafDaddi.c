//// DISCLAIMER: this code is not certified in any way for vehicle/roadway purposes - use at your own risk or do not use.
//Initializing LED Pin
#include "Wire.h"
////For real time/speed and 0-6V i/o, I need 2 x PCF8591 (one A-in, one A-out)
//To use two PCF8591 modules from same vendor 
//cut the "pad" ground path with an exacto (between PCF8591 pin 5 & 6) A0  will "1" and address will go up
//0x90 OEM PCF8591, 0x92 CUT PCF8591
#define PCF8591I (0x92 >> 1) // IC bus address for INPUT DAC
#define PCF8591O (0x90 >> 1) // IC bus address for OUPUT DAC
byte adc0, adc1, adc2, adc3, aout, bmin, bmed, bmax;

////POC code demonstrates the basic params for a MAF modifier
//N.B. - the cheap ebay PCF8591 boards are limited to 8Vcc (6Vcc safer).
//And will in/out a 4.96V signal to 4.16V, which is limiting as that's the stock read
//at .81 VE for maf at for motors around 2.5L@6500rpm (no boost)
//test LOW(0) codes as 1.5V using ebay pcb's in the below algo's at 4.96Vcc/Vref
//and  LOW(0) codes as 2.4V using ebay pcb's in the below algo's at 7.25Vcc/Vref
//This level of voltage loss can be remedied by cutting the rear trace from D1 to R4!
//for deeper hacks, see the official nxt pdf section on Vref:Vgrd span/deltas


//will 
//This POC version of the program will fade the LED

//** I would ilke to make a logic switch that determines if we are on usb or on 12v header power!
//That would set these, so we have no lag in car (and can set serial on by NOT using 12v header)
////Serial verbosity control:
//bool dbgMode = 0; //0 == False
bool dbgMode = 1; //1 == True

////SHOW EFFECTS ON THE LED?
bool ledMode = 0; //0 == False
//bool ledMode = 1; //1 == True

int mapper = 0;
int waver = 1;
int modder = 1;
int PWMOUT = 6;
int sensorPin = A0;    // select the input pin for the potentiometer
int ledPin = 13;      // select the pin for the LED
int logicHighRefPin = 8; //reads as 4.97v wrtg on uno, leading the PCF8591 by almost 0.9v :/ (when PCF8591 vcc is 4.97v)
int BenchTestPin = 7;      // select the pin for the LED  ATTN: JUMPER pin 7 to 3.3v to see a waveform instead of the "REAL" derived from AIN
int doBenchTest = 0; //~Bool/false on init
int ledState = LOW;
int ledCycle = 100;
int serialCycle = 100;
int offset = 11; //need to calibrate this for various voltage levels - 3.3v, 4.2v, 4.94v, 5.5v, 6.25v to offset the component losses and NOT have a weaker wave form
int modstep = 0;
int squarestep = 1; //MUST be greater than 0 or you mute the test wave!

unsigned long ledTime = 0;
unsigned long serialTime = 0;


void setup() {
  //Declaring LED pin as output
  pinMode(PWMOUT, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(logicHighRefPin, OUTPUT);
  pinMode(BenchTestPin, INPUT);
  
  doBenchTest = digitalRead(BenchTestPin);
  digitalWrite(logicHighRefPin, HIGH);

  //DAC shtuff
  Serial.begin(115200); //Go full speed!!! 9600 makes for a slower duty cycle in the event of any output
  //Serial.begin(9600); //Go full speed!!! 9600 makes for a slower duty cycle in the event of any output
  Wire.begin();

  bmin = 0;
  bmed = 128;
  bmax = 255;

  //Set a vref value on the input boards AOUT for debug purposes
  Wire.beginTransmission(PCF8591I); // ping PCF8591IT
  Wire.write(0x40); // control byte - turn on DAC (binary 1000000)
  Wire.write(bmax); //  send to DAC for AOUT pin (255 is high -- vcc less approx 3.1Vout at 3.3vcc, and 4.5Vout at 5.20Vcc)
  Wire.endTransmission(); // end tranmission
  Serial.println();
  Serial.print("BOOT TEST IN PROGRESS"); 
  Serial.println();
  Serial.print("  ##      ##            ####      ##                  ##      ##      "); Serial.println();
  Serial.print("  ####  ####  ####    ##    ##    ####    ####        ##      ##  **  "); Serial.println(); 
  Serial.print("  ##  ##  ##      ##  ##          ##  ##      ##      ##      ##      "); Serial.println(); 
  Serial.print("  ##      ##    ####  ####        ##  ##    ####    ####    ####  ##  "); Serial.println(); 
  Serial.print("  ##      ##  ##  ##  ##          ##  ##  ##  ##  ##  ##  ##  ##  ##  "); Serial.println(); 
  Serial.print("  ##      ##  ##  ##  ##          ####    ##  ##  ##  ##  ##  ##  ##  "); Serial.println(); 
  Serial.print("  ##      ##    ####  ##          ##        ####    ####    ####  ##  ,"); Serial.println(); 
  Serial.print("'I'd tap that MAF! (R)'"); Serial.println(); 
  Serial.print(doBenchTest);
  Serial.print("BOOT TEST COMPLETE (starting in approx 1 second)"); 
  Serial.println();
  //SADLY the high led test dies after Input samples are taken...
  delay(1250);



}
void loop() {
  unsigned long currentMillis = millis();

  ////GET INPUTS
  Wire.beginTransmission(PCF8591I); // ping PCF8591IT
  Wire.write(0x04); // control byte - go read mode
  Wire.endTransmission();
  ///Wire.requestFrom(PCF8591I, 5); // What does this 5 mean, ktb?! multiplex load?
  Wire.requestFrom(PCF8591I, 5); // What does this 5 mean, ktb?! multiplex load?
  adc0=Wire.read(); //Necessary trick per one dev (John Baxell)

  //...into byte variables
  //driven by pins UNLESS you set the Jumpers on the header
  adc0=Wire.read(); //J5/P5 Photoresitor 255 is DARK, 0 is very bright - level adj. as a function of VR1
  adc1=Wire.read(); //J4/P4 Thermistor 255 is room temp ~20C, 0 is very hot (boiling?!)  - level adj. as a function of VR1
  adc2=Wire.read(); //AIN1 This is analog input, high volts like 100% of vcc will be 255, ground 0.
  adc3=Wire.read(); //J6/P6 This is the VR1 varistor, with fully CCW being 255, CW 0



  //** ktb I can measure this by comparing ain to aout with o-scope or voltmeter for  a quick "differential" value - this will 
  //liner stepped offset, BUT maybe this should be more dynamic?
  modstep = 16; //could be an algo or return of a function
  //This really needs to be a map function from an include. Or added to one.  Could handle bytes that way, too.
  if (adc3 >= 224){
    offset = modstep * 3;}
  else if (adc3 >= 192){
    offset = modstep * 2;}
  else if (adc3 >= 160){
    offset = modstep * 1;}
  else if (adc3 >= 128){
    offset = 1;}
  else if (adc3 >= 64){
    offset = 1;} // this is kind of a waste BUT achieves a good safe dead zone, for now (could make base 9 vs bae 8-> base 7
  else if (adc3 >= 96){
    offset = modstep * -1;} //DANGER, byte(0 + -1) = 255 and we do NOT Want that!) ktb attn pls *** (After testing in engine off mode)
  else if (adc3 >= 32){
    offset = modstep * -2;} //DANGER, byte(0 + -1) = 255 and we do NOT Want that!) ktb attn pls *** (After testing in engine off mode)
  else if (adc3 >= 0){
    offset = modstep * -3;} //DANGER, byte(0 + -1) = 255 and we do NOT Want that!) ktb attn pls *** (After testing in engine off mode)
  else {
    offset = 0; //safe bail out
  }


  //squarestep = offset; // way to harsh
  //squarestep = offset / 4;
  squarestep = 1; //MUST be greater than 0 or you mute the test wave!
  if(waver == 255){
    modder = -1 * squarestep;
  }
  else if (waver == 1) {
    modder = 1 * squarestep;
  }
  //no point in adding offset, you'd just squish the test wave
  waver += modder;

  ////Let's  dynamically mod some values!
  ////mapper = waver / -2; //or vr1 Rather
  //mapper = adc3 / 10 ; //set this to the varistor!
  //aout = adc3 + mapper ; //let the varistor drive the AOUT value ;P
  
  //////*** ktb this value needs to be ain, not ad3
  ////aout = adc3 + offset ; //let the varistor drive the AOUT value ;P
  ///aout = adc2 + offset ;
  aout = adc2;
  //over-rides aout
  if (doBenchTest){
    aout = waver ; //let the varistor drive the AOUT value ;P
    //delay(10);
  }
   
  ////LOUSY attempt but goes high/low for now
  ///(k but pin 13 isn't pwm so this gonna be lousy)
  //ledCycle = aout;
  //REFLECT output on a co-witness/LED
  if (currentMillis - ledTime >= ledCycle) {
    // save the last time you blinked the LED
    ledTime = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    analogWrite(ledPin, aout);
    //digitalWrite(ledPin, ledState);
  }

  //Provide alternate (lower/econo MafLevel)
  analogWrite(PWMOUT, aout);



  ////SET OUTPUTS
  //Init. write mode to Analog OUT
  Wire.beginTransmission(PCF8591O); // ping PCF8591T
  Wire.write(0x40); // control byte - turn on DAC (binary 1000000)
  Wire.write(aout); //  send to DAC for AOUT pin (255 is high -- vcc less approx 3.1Vout at 3.3vcc, and 4.5Vout at 5.20Vcc)
  Wire.endTransmission(); // end tranmission



  if (currentMillis - serialTime >= serialCycle) {
    serialTime = currentMillis;
    if (dbgMode){
      Serial.print(adc0);
      Serial.print(" ,");
      Serial.print(adc1); 
      Serial.print(" ,");
      Serial.print(adc2); 
      Serial.print(" ,");
      Serial.print(adc3); 
      Serial.print(" ,");
      Serial.print(aout); 
      Serial.print(" ,");
      Serial.print(mapper); 
      Serial.print(" ,");
      Serial.print(waver); 
      Serial.println();
    }
    else {
      Serial.print(adc1); 
      Serial.print(" ,");
      Serial.print(adc2); 
      Serial.print(" ,");
      Serial.print(aout); 
      Serial.println();
    }

  }


  //!! ktb please look in to a watchdog type reset to help prevent a dead-sticked MAF if the module gets hungup!

} //end loop
