/*
ubx wiring:

Green = GPS Rx  -> Ard Tx
Blue  = GPS Tx  -> Ard Rx

ubx commands:

Sleep:
B5 62 06 57 08 00 01 00 00 00 50 4F 54 53 AC 85

Wake:
B5 62 06 57 08 00 01 00 00 00 20 4E 55 52 7B C3

Disable NMEA messages:

B5 62 06 01 03 00 F0 00 00 FA 0F
B5 62 06 01 03 00 F0 01 00 FB 11
B5 62 06 01 03 00 F0 02 00 FC 13
B5 62 06 01 03 00 F0 03 00 FD 15
B5 62 06 01 03 00 F0 04 00 FE 17
B5 62 06 01 03 00 F0 05 00 FF 19

Poll POSLLH:
B5 62 01 02 00 00 03 0A

*/

#include <Arduino.h>

#include <SoftwareSerial.h>

#define LOOP_DELAY  1000
#define GPS_LOCK_MSG_LIMIT  180  
#define HORIZONTAL_ACC_THRESHOLD  10
#define STAY_AWAKE_CYCLE 60


// Connect the GPS RX/TX to arduino pins 3 and 5
SoftwareSerial gpsPort = SoftwareSerial(10,11);

const unsigned char gpsConfigCommands[6][11]= {{0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x00, 0x00, 0xFA, 0x0F},
                                               {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x01, 0x00, 0xFB, 0x11},
                                               {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x02, 0x00, 0xFC, 0x13},
                                               {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x03, 0x00, 0xFD, 0x15},
                                               {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x04, 0x00, 0xFE, 0x17},
                                               {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x05, 0x00, 0xFF, 0x19}};
const unsigned char gpsSleepCommand[] = {0xB5, 0x62, 0x06, 0x57, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x50, 0x4F, 0x54, 0x53, 0xAC, 0x85};
const unsigned char gpsWakeCommand[] = {0xB5, 0x62, 0x06, 0x57, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x4E, 0x55, 0x52, 0x7B, 0xC3};
const unsigned char gpsPOSLLH_CMD[]={0xB5, 0x62, 0x01, 0x02, 0x00, 0x00, 0x03, 0x0A};
const unsigned char UBX_HEADER[] = { 0xB5, 0x62 };
int gpsConfigured,gpsMessageCounter,gpsLock,gpsPositionRequest,gpsAwake,gpsAwakeCounter;

struct NAV_POSLLH {
  unsigned char cls;
  unsigned char id;
  unsigned short len;
  unsigned long iTOW;
  long lon;
  long lat;
  long height;
  long hMSL;
  unsigned long hAcc;
  unsigned long vAcc;
};

NAV_POSLLH posllh;

void calcChecksum(unsigned char* CK) {
  memset(CK, 0, 2);
  for (int i = 0; i < (int)sizeof(NAV_POSLLH); i++) {
    CK[0] += ((unsigned char*)(&posllh))[i];
    CK[1] += CK[0];
  }
}

bool processGPS() {
  static int fpos = 0;
  static unsigned char checksum[2];
  const int payloadSize = sizeof(NAV_POSLLH);

  while ( gpsPort.available() ) {
    byte c = gpsPort.read();
    if ( fpos < 2 ) {
      if ( c == UBX_HEADER[fpos] ){
        fpos++;
      }
      else{
        fpos = 0;
      }
    }
    else {
      if ( (fpos-2) < payloadSize )
        ((unsigned char*)(&posllh))[fpos-2] = c;

      fpos++;

      if ( fpos == (payloadSize+2) ) {
        calcChecksum(checksum);
      }
      else if ( fpos == (payloadSize+3) ) {
        if ( c != checksum[0] )
          fpos = 0;
      }
      else if ( fpos == (payloadSize+4) ) {
        fpos = 0;
        if ( c == checksum[1] ) {
          return true;
        }
      }
      else if ( fpos > (payloadSize+4) ) {
        fpos = 0;
      }
    }
  }
  return false;
}

void gpsWake(){
  Serial.println("GPS waking up!");
  gpsPort.write(gpsWakeCommand,sizeof(gpsWakeCommand));
  gpsAwake = 1;
  gpsAwakeCounter = 0;
  delay(500);
  gpsPort.flush();
  delay(100);
}

void gpsSleep(){
  Serial.println("GPS going to sleep...");
  gpsPort.write(gpsSleepCommand,sizeof(gpsSleepCommand));
  gpsAwake = 0;
  gpsAwakeCounter = 0;
  gpsLock = 0;
  delay(500);
  gpsPort.flush();
  delay(100);
}


// Write all configure commands
void gpsConfigure(){
  for(int i = 0; i < sizeof(gpsConfigCommands)/sizeof(gpsConfigCommands[0]); i++)
    gpsPort.write(gpsConfigCommands[i],sizeof(gpsConfigCommands[i]));
  delay(500);
  gpsSleep();
  gpsConfigured = 1;
}



void setup() 
{
  Serial.begin(9600);
  gpsPort.begin(9600);
  Serial.println("Awake");
  gpsConfigured = 0;
  gpsMessageCounter = 0;
  gpsLock = 0;
  gpsPositionRequest = 0;
  gpsAwake = 1;
}

void loop() {

  if (!gpsConfigured){
    delay(1000);
    gpsConfigure();
  }
    

  
  if(Serial.available()){
    char c = Serial.read();
    if(c=='p'){
      Serial.println("Got position command, waiting for lock...");
      gpsPositionRequest = 1;
    }else{
      Serial.flush();
    }
  }

  // Get GPS position
  if(gpsPositionRequest){
    gpsAwakeCounter = 0;
    if(!gpsAwake)
      gpsWake();

    gpsPort.write(gpsPOSLLH_CMD,sizeof(gpsPOSLLH_CMD));
    delay(100);


    if(processGPS()&&((posllh.hAcc/1000.0f <= HORIZONTAL_ACC_THRESHOLD)||(gpsMessageCounter>GPS_LOCK_MSG_LIMIT))){
      gpsLock = 1;
      gpsMessageCounter = 0;
      gpsPositionRequest = 0;
      Serial.print("http://maps.google.com/?q=");Serial.print(posllh.lat/10000000.0f,8);Serial.print(",");Serial.print(posllh.lon/10000000.0f,8);Serial.println();

      Serial.print(" hAcc: ");    Serial.print(posllh.hAcc/1000.0f);
      Serial.print(" vAcc: ");    Serial.print(posllh.vAcc/1000.0f);
      Serial.println();
    }

    if(!gpsLock)
      gpsMessageCounter++;
  }

  if(gpsLock && gpsAwake)
    gpsAwakeCounter++;

  if(gpsAwakeCounter > STAY_AWAKE_CYCLE)
    gpsSleep();

  
  delay(1000);

}