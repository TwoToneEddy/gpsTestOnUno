/*
ubx wiring:

Green = GPS Rx  -> Ard Tx
Blue  = GPS Tx  -> Ard Rx

ubx commands:

Sleep:
B5 62 06 57 08 00 01 00 00 00 50 4F 54 53 AC 85

Wake:
B5 62 06 57 08 00 01 00 00 00 20 4E 55 52 7B C3


*/

#include <Arduino.h>

#include <SoftwareSerial.h>

// Connect the GPS RX/TX to arduino pins 3 and 5
SoftwareSerial gpsPort = SoftwareSerial(10,11);

const unsigned char gpsSleepCommand[] = {0xB5, 0x62, 0x06, 0x57, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x50, 0x4F, 0x54, 0x53, 0xAC, 0x85};
const unsigned char gpsWakeCommand[] = {0xB5, 0x62, 0x06, 0x57, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x4E, 0x55, 0x52, 0x7B, 0xC3};

const unsigned char UBX_HEADER[] = { 0xB5, 0x62 };

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

void gpsSleep(){
  gpsPort.write(gpsSleepCommand,sizeof(gpsSleepCommand));
}

void gpsWake(){
  gpsPort.write(gpsWakeCommand,sizeof(gpsWakeCommand));
}

void setup() 
{
  Serial.begin(9600);
  gpsPort.begin(9600);
  Serial.println("Awake");
}

void loop() {
  

  if(Serial.available()){
    char c = Serial.read();
    Serial.print("Got: ");
    Serial.println(c);
    if(c=='s'){
      Serial.println("Sleeping...");
      gpsSleep();
    }
    if(c=='w'){
      Serial.println("Waking...");
      gpsWake();
    }

  }


  if ( processGPS() ) {
    Serial.print("http://maps.google.com/?q=");Serial.print(posllh.lat/10000000.0f,8);Serial.print(",");Serial.print(posllh.lon/10000000.0f,8);Serial.println();

    Serial.print(" hAcc: ");    Serial.print(posllh.hAcc/1000.0f);
    Serial.print(" vAcc: ");    Serial.print(posllh.vAcc/1000.0f);
    Serial.println();
  }
  
    

}