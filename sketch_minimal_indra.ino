#include <MsTimer2.h>
#include <EEPROM.h>

String callingNumber="";
static boolean motorState = LOW;
boolean gsmWorking = false;
static byte ringNumber = 0;
static byte missedCallAttemptCount = 0;
bool calling = false;
bool FREE = true;

int powerStatus = 0;
int powerStatusOld = 0;

struct numberToCall{
    boolean missedCallEnabled;
    String NumberToRemind;
}numToCall;
void sendCREG(){
  if( FREE)
    Serial.print("AT+CREG=?\r");
    else
    Serial.println("Not FREE");
}
void writeIntoMemory(){
  Serial.println("Fn:WriteInMemory");
  Serial.flush();
  delay(50);
  Serial.print("AT+CPBW=1\r");
  response();
  delay(500);
  String command="AT+CPBW=1,\"+";
  command += numToCall.NumberToRemind;
  command += "\",129,\"JIM\";\r";
  Serial.print(command);
  response();
  
}
void cleanMemory(){
  Serial.println("Fn:cleanMemory");
  Serial.flush();
  Serial.print("AT+CPBW=1\r"); // delete the phone book entry;
  response();
  return;
}
boolean readFromMemory(void){
  String in = "";
  Serial.print("AT+CPBR=1\r");
}
void response1(void) {
  if(Serial.available()){
    String input;
    while(Serial.available()){
       char c = (char)Serial.read();
       if(c!='\n')
       input += (char)c;
    }
    Serial.println(input);
    if(input.endsWith("OK")){
      gsmWorking = true;
        Serial.flush();
      return;
    } else if (input.endsWith("ERROR")) {
      gsmWorking = false;
        Serial.flush();
      return;
    }
    Serial.println("1-->"+input);
  }
  Serial.flush();
}
void response (void){
  int count = 0;
  Serial.println();
  while(1)
  {
    if(Serial.available())
    {
      char data =Serial.read();
      Serial.println(data);
      switch(data){
        case '0':
        case 'K':
          Serial.println("OK");
          gsmWorking = true;
          return;
          break;
        case '4':
        case 'R':
          Serial.println("GSM Not Working");
          gsmWorking = false;
          return;
          break;
        case '3':
          Serial.println("No Carrier");
          return;
          break;       
      }
    }
    count++;
    delay(10);
    if(count == 1000){
      Serial.println("GSM not Found");
      break;
    }
  }
}
void gsmConfig() {
  Serial.println("Fn:gsmConfig");
  Serial.flush();
  delay(2000);
  Serial.print ("AT\r"); //WAKE UP THE MODULE
  response(); delay(500);
  
  Serial.print ("ATV1\r"); // set coded response format
  response(); delay(500);
  
  String command ="AT+CPBS=";
  command += '"';
  command += "SM";
  command += '"';
  command += '\r';
  Serial.print(command);  // SET THE CURRENT PHONE BOOK AS SIM
  response(); delay(500);

  Serial.print ("ATE0\r"); //TURN ECHO MODE OFF
  response(); delay(1000);
  
  Serial.print ("AT+CLIP=1\r"); // TURN ON CALLING LINE IDENTIFICATION
  response(); delay(500);

  Serial.print ("AT+COLP=1\r"); // cONFIGURE RESPONSE TO ATD
  response(); delay(500);

  Serial.print ("AT+MORING=1\r");
  response(); delay(500);
  
  Serial.print ("ATH\r"); // DISCONNECT ALL ACTIVE CONNECTIONS
  response(); delay(500);
  
  //Serial.print ("ATS0=2\r"); // AUTO ANSWER THE CALL AFTER TWO RINGS
  //response(); delay(500);
  
  Serial.print ("AT+DDET=1\r"); // TURN ON INCOMING DTMF DETECTION
  response(); delay(500);
  
  //rial.print ("ATV1\r");  // SET THE RESPONSE MODE TO TEXT
  //sponse(); delay(500);
 
  Serial.println("Fn:gsmConfig:complete");
  Serial.flush();
}
void giveMissedCall() {
  Serial.println("Fn:giveMissedCall");
  Serial.flush();
  if(numToCall.missedCallEnabled) {
    String command = "ATD+";
    command += numToCall.NumberToRemind;
    command += ";\r";
    Serial.print(command);
    FREE = false;
    calling = true;
  } else {
    Serial.println("No number configured to call");
  }
}
void dropCall(){
  Serial.print("ATH\r");
  //response();
  FREE = true;
  calling = false;
  
}
void setup() {
  // start serial
  Serial.begin(9600);
  delay(30000);

  Serial.println("Fn:Setup");
  Serial.flush();
  
  // Set GPIOs
  pinMode (5, OUTPUT);
  digitalWrite (5, LOW);
  
  // Align power status
  // alignPowerState();
  
  // Start configuring GSM module
  gsmConfig();

  // Read the previous settings
  boolean b = readFromMemory();

  powerStatus = analogRead(A2);
  powerStatusOld = powerStatus;
  
  Serial.println("Setup complete");
  
  MsTimer2::set(40000, sendCREG); //60,000 Ms
  MsTimer2::start();
  //giveMissedCall();
}
void talkBack(String resp,int del){
  //Serial.println("Fn: talkBack");
  Serial.flush();
  delay(100);
  String command = "AT+CTTS=2,";
  command += resp;
  command += '\r';
  Serial.print(command);
  response();
  delay(del);
  Serial.print("AT+CTTS=0\r");
  response();
}
void loop/*workOnGsmInput*/(){
  if(Serial.available()){
    //Serial.println("Fn:WorkOnGsmInput: Received inputs");
    String input;
    while(Serial.available()){
      char c = (char)Serial.read();
      delay(20);
      if(c!='\n')
      input += (char)c;
    } // While Serial.available
    input.trim(); // trim any leading and trailing whitespace characters
    Serial.println("2-->"+input);

    // All characters collected take action now
    if (input.startsWith("RING")) { // Then its an incoming call
      FREE = false;
      Serial.println("RING identified");
      ++ringNumber;
      // extract calling number
      if ((input.length() > 17 ) &&(ringNumber == 1)){ // we have a meaningful input
        // Format is --> [+CLIP: "919818080066",129,"".. .. .. ] 
        int index = input.indexOf('"');
        char ch;
        callingNumber =""; // clear the calling number
        ++index;  // increment the number
        ch = (char)input[index];
        do{          
          Serial.print(ch);
          Serial.println();
          callingNumber += ch;
          ++index;
          ch = (char)input[index];
        } 
        while(ch != '"');
        Serial.println("Calling number is :" + callingNumber);
      } // if length
    } // if RING
    else if (input.startsWith("+DTMF")) { // The these are incoming DTMF tones
      Serial.println("DTMF detected");
      Serial.println(input[7]);
      switch(input[7]){
        case '1':
          if(analogRead(A2)> 470){
            // Say Power OK
            talkBack("Power Ok",1400);
          } else {
            talkBack("no Power",1000);
          }
        break;
        case '2':
          if(analogRead(A2) > 470) {
            if(!motorState){
              motorState = HIGH;
              digitalWrite(5,motorState);
              talkBack("Motor On",1000);
            } else {
              talkBack ("already on",1000);
            }
          } else {
            talkBack("No Power",1000);
          }
        break;
        case '3':
          if(analogRead(A2)>470) {
            if(motorState){
              motorState = LOW;
              digitalWrite(5,motorState);
              talkBack("Motor Off",1000);
            } else {
              talkBack ("already Off",1000);
            }
          } else {
            talkBack("No Power",1200);
            if(!numToCall.missedCallEnabled) {
              talkBack("Press 5 for power notice",2670);
            }
          }
        break;
        case '5':
          if (callingNumber.length()) {
            numToCall.missedCallEnabled = true;
            numToCall.NumberToRemind = callingNumber;
            Serial.println("Number to be called is :" + numToCall.NumberToRemind);
            talkBack("Power notice on",2000);
            talkBack("Thank   YOU",1200);
            dropCall();
            writeIntoMemory();
          } else {
            talkBack ("Try again",1000);
          }
        break;
        case '6':
          if(numToCall.missedCallEnabled) {
            numToCall.missedCallEnabled = false;
            numToCall.NumberToRemind  = "";
            cleanMemory();
            talkBack("Power notice off",2000);
            talkBack("Thank   YOU", 1000);  
            dropCall();            
          } else {
            talkBack("Thank   YOU", 1000);            
          }          
        break;
        //case '7':
        //  readFromMemory();
        //break;
        case '#':
        talkBack("press 1 for power status",3000);
        talkBack("Press 2 for Motor On",3000);
        talkBack("Press 3 for motor off",3000);
        talkBack("Press 5 for power Notice",3000);
        talkBack("press 6 for no notice",3000);
        talkBack("press hash for Menus",3000);
        break;
        default:
        talkBack("invalid entry press hash for menu", 4000);
        break;
      } // switch case
    } // if dtmf 
    else if (input.startsWith("+CPBR:")){
      int index = input.indexOf('"');
        char ch;
        String str =""; // clear the calling number
        ++index;  // increment the number
        ch = (char)input[index];
        do{          
          Serial.print(ch);
          Serial.println();
          str += ch;
          ++index;
          ch = (char)input[index];
        } 
        while(ch != '"');
        Serial.println("Str number is :" + str);
        numToCall.NumberToRemind = str;
        numToCall.missedCallEnabled = true;
        Serial.println(numToCall.missedCallEnabled);
        //giveMissedCall();     
    }
    else if (input.startsWith("MO RING")){
      delay(5000);
      dropCall();
    }
    else if ((input.startsWith("NO CARRIER")) ||
             (input.startsWith("NO ANSWER"))  ||
             (input.startsWith("BUSY"))       ||
             (input.startsWith("NO DIALTONE"))){
      if((missedCallAttemptCount < 5)&&(calling)){
        missedCallAttemptCount++;
        giveMissedCall();
      } else {
        Serial.println("MAX RETRY Expired");
        FREE = true;
        calling = false;
      }
    }
    else if (input.startsWith("+CREG")) {
      powerStatus = analogRead(A2);
      if((powerStatus > 450) && ((powerStatus - powerStatusOld) > 100)){
        powerStatusOld = powerStatus;
        giveMissedCall();
      }powerStatusOld = powerStatus;
      Serial.println(powerStatusOld);
      if(powerStatus <450){
        motorState = LOW;
        digitalWrite(5,motorState);
      }
    }
    if (ringNumber == 2){        
      Serial.print("ATA\r");
      ringNumber = 0;
      response();delay(1000);
      talkBack("HELLOW",650);
      if(analogRead(A2)){
        talkBack("Power OK",1400);
        delay(500);
        talkBack("Press hash for menu",2000);
      }else{
        talkBack("No Power", 1400);
        delay(500);
        if(!numToCall.missedCallEnabled) {
              talkBack("Press 5 for power notice",2670);
        }
      }
    }
  } // if Serial.available
}
