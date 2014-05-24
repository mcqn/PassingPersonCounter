// PassingPersonCounter
// (c) Copyright 2014 MCQN Ltd
// 
// Arduino sketch to periodically log PIR-sensed movements to a Xively feed.

#include <JeeLib.h>
#include <SPI.h>
#include <GSM.h>
#include <HttpClient.h>
#include <Xively.h>

//============================================================
// IMPORTANT!
// To get this code to work you'll need to rename the file
// XivelyKeys.h.example to XivelyKeys.h and add the right
// Xively feed ID and Xively key into that to access a feed
// that you've created in Xively
//============================================================
#include "XivelyKeys.h"

// Your phone SIM PIN Number
#define PINNUMBER ""

// Your phone APN data
#define GPRS_APN       "eseye.com" // replace your GPRS APN
#define GPRS_LOGIN     ""    // replace with your GPRS login
#define GPRS_PASSWORD  "" // replace with your GPRS password

// Define the strings for our datastream IDs
char sensorId[] = "Movement";
char batteryId[] = "Battery";
char chargingId[] = "Charge";
XivelyDatastream datastreams[] = {
  XivelyDatastream(batteryId, strlen(batteryId), DATASTREAM_FLOAT),
  XivelyDatastream(chargingId, strlen(chargingId), DATASTREAM_INT),
  XivelyDatastream(sensorId, strlen(sensorId), DATASTREAM_INT)
};
// Finally, wrap the datastreams into a feed
XivelyFeed feed(xivelyFeedId, datastreams, 3 /* number of datastreams */);

volatile int movementCount =0;
volatile unsigned long movementStart =0;
int lastMovementCount =0;
unsigned long lastMovementTime =0;
unsigned long lastReportTime =0;

// How long it'd take one person to pass by the sensor
const unsigned long kPassingTime =30*1000;

// How long we should sleep between reporting results (IN MINUTES!)
const unsigned long kReportInterval =2*60UL;
// boilerplate for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup() {
#if 0
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.println("Let's go!");
#endif
  pinMode(1, INPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(4, OUTPUT);
  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);
  digitalWrite(4, HIGH);
  delay(1000);
  digitalWrite(12, LOW);
  digitalWrite(4, LOW);

  attachInterrupt(3, movementDetected, CHANGE);
}

void loop() {
  // Spend time gathering data first, so if we wake up because
  // the sun has come out, there's maybe a chance that the battery
  // will charge up enough before we try to use the modem :-)

  // loseSomeTime can only sleep for up to just over a minute (65535
  // milliseconds) so to get decent sleep intervals, we'll loop through
  // a number of times
  for (int i =0; i < kReportInterval; i++) {
    lastReportTime = millis();
    while (Sleepy::loseSomeTime((60*1000)-(millis()-lastReportTime)) == 0)
    {
      // It was interrupted by the movement trigger, go back to sleep
#if 0
      digitalWrite(4, HIGH);
      delay(300);
      digitalWrite(4, LOW);
#endif
    }
  }

  digitalWrite(13, LOW);
  delay(1000);
  digitalWrite(13, HIGH);
  
  if (movementCount > lastMovementCount)
  {
    int diff = movementCount - lastMovementCount;
    lastMovementCount = movementCount;
#if 1
    for (int i=0; i < diff; i++)
    {
      digitalWrite(13, LOW);
      delay(300);
      digitalWrite(13, HIGH);
      delay(200);
    }
    
    reportMovementCount(diff);
#else
    unsigned long movementTime = millis();
    
    Serial.print(movementTime);
    Serial.print(" (");
    Serial.print(movementTime-lastMovementTime);
    Serial.print("): movementCount is ");
    Serial.println(lastMovementCount);
    lastMovementTime = movementTime;
#endif
  }
}

void movementDetected()
{
  // The PIR sensor is active low
  if (digitalRead(1))
  {
    digitalWrite(12, LOW);
    // Find out how long we've been active, and use that to guide
    // how many people we think have passed
    movementCount++;
    if (millis()-movementStart < 10*60*1000UL)
    {
      movementCount += (millis()-movementStart)/kPassingTime;
    }
    else
    {
      // it's over 10 minutes, reckon it's someone loitering
      // Probably at least two people then :-)
      movementCount++;
    }    
  }
  else
  {
    digitalWrite(12, HIGH);
    movementStart = millis();
  }
}

void reportMovementCount(int aCount)
{
  GSM gsmAccess;     // include a 'true' parameter to enable debugging
  GPRS gprs;
  GSMClient client;
  XivelyClient xivelyclient(client);

  // connection state
  boolean notConnected = true;
  // After starting the modem with GSM.begin()
  // attach the shield to the GPRS network with the APN, login and password
  while(notConnected)
  {
    if((gsmAccess.begin(PINNUMBER)==GSM_READY) &
      (gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD)==GPRS_READY))
      notConnected = false;
    else
    {
      digitalWrite(4, HIGH);
      delay(1000);
      digitalWrite(4, LOW);
    }
  }
  
  // Record what our battery level is now too  
  // A4 is measuring the mid-point of a 100k/100k voltage divider
  datastreams[0].setFloat(analogRead(A4)*2.0*3.3/1023.0);
  // And whether or not we're charging
  datastreams[1].setInt(analogRead(A5));
  
  datastreams[2].setInt(aCount); 
  
  if (xivelyclient.put(feed, xivelyKey) < 0)
  {
    // Something went wrong
    for (int i=0; i < 4; i++)
    {
      digitalWrite(4, HIGH);
      delay(1000);
      digitalWrite(4, LOW);
      delay(200);
    }
  }

  while ( gsmAccess.shutdown() != 1 ) {
    delay (1000);
  }
}

