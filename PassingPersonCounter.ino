// PassingPersonCounter
// (c) Copyright 2014 MCQN Ltd
// 
// Arduino sketch to periodically log PIR-sensed movements to a Xively feed.

#include <JeeLib.h>
#include <SPI.h>
#include <GSM.h>
#include <HttpClient.h>
#include <Time.h>
#include <Xively.h>
#include <FreeMemory.h>

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

#define DEBUG 0

// How long each reading set should span (IN MINUTES!)
const unsigned long kReadingInterval =15UL;
// How many readings we should take before we report them to the Internet
const int kReadingsPerReport = 8;

// FIXME
time_t readingsTime = 1401633365;

// Define the strings for our datastream IDs
char sensorId[] = "Movement";
char batteryId[] = "Battery";
char chargingId[] = "Charge";
char freeMemId[] = "FreeRAM";
XivelyDatapoint batteryReadings[kReadingsPerReport] = {
  XivelyDatapoint(DATASTREAM_FLOAT),
  XivelyDatapoint(DATASTREAM_FLOAT),
  XivelyDatapoint(DATASTREAM_FLOAT),
  XivelyDatapoint(DATASTREAM_FLOAT),
  XivelyDatapoint(DATASTREAM_FLOAT),
  XivelyDatapoint(DATASTREAM_FLOAT),
  XivelyDatapoint(DATASTREAM_FLOAT),
  XivelyDatapoint(DATASTREAM_FLOAT)
};
XivelyDatapoint chargingReadings[kReadingsPerReport] = {
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT)
};
XivelyDatapoint sensorReadings[kReadingsPerReport] = {
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT)
};
XivelyDatapoint freeMemReadings[kReadingsPerReport] = {
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT),
  XivelyDatapoint(DATASTREAM_INT)
};
XivelyHistoricDatastream datastreams[] = {
  XivelyHistoricDatastream(batteryId, strlen(batteryId), DATASTREAM_FLOAT, batteryReadings, kReadingsPerReport),
  XivelyHistoricDatastream(chargingId, strlen(chargingId), DATASTREAM_INT, chargingReadings, kReadingsPerReport),
  XivelyHistoricDatastream(sensorId, strlen(sensorId), DATASTREAM_INT, sensorReadings, kReadingsPerReport),
  XivelyHistoricDatastream(freeMemId, strlen(freeMemId), DATASTREAM_INT, freeMemReadings, kReadingsPerReport)
};
// Finally, wrap the datastreams into a feed
XivelyHistoricFeed feed(xivelyFeedId, datastreams, 4 /* number of datastreams */);

volatile int movementCount =0;
volatile unsigned long movementStart =0;
int lastMovementCount =0;
unsigned long lastMovementTime =0;
unsigned long lastReadingTime =0;

// How long it'd take one person to pass by the sensor
const unsigned long kPassingTime =30*1000;

// boilerplate for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup() {
#if DEBUG
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
  for (int readingIdx = 0; readingIdx < kReadingsPerReport; readingIdx++)
  {
    for (int i =0; i < kReadingInterval; i++) {
      lastReadingTime = millis();
#if DEBUG
      delay(60000);
#else
      while (Sleepy::loseSomeTime((60*1000)-(millis()-lastReadingTime)) == 0)
      {
        // It was interrupted by the movement trigger, go back to sleep
      }
#endif
    }
    // Store these readings
    // Record what our battery level is now too  
    // A4 is measuring the mid-point of a 100k/100k voltage divider
    batteryReadings[readingIdx].setFloat(analogRead(A4)*2.0*3.3/1023.0);
    // And whether or not we're charging
    chargingReadings[readingIdx].setInt(analogRead(A5));
    // Plus how much memory is free (so we'll maybe spot memory leaks)
    freeMemReadings[readingIdx].setInt(FreeRam());
    
    sensorReadings[readingIdx].setInt(movementCount);
    movementCount = 0;
    for (int i=0; i < sensorReadings[readingIdx].getInt(); i++)
    {
      digitalWrite(13, LOW);
      delay(300);
      digitalWrite(13, HIGH);
      delay(200);
    }
  }

  digitalWrite(13, LOW);

#if 1
  reportMovementCount();
  digitalWrite(13, HIGH);
#else
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
#endif
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

void reportMovementCount()
{
  GSM gsmAccess;     // include a 'true' parameter to enable debugging
  GPRS gprs;
  GSMClient client;
  XivelyClient xivelyclient(client);

#if DEBUG
  Serial.println("reporting movement count");
#endif
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

#if DEBUG
  Serial.println("connected");
#endif
  // Set current value to whatever the last reading was
  datastreams[0].setFloat(batteryReadings[kReadingsPerReport-1].getFloat());
  datastreams[1].setInt(chargingReadings[kReadingsPerReport-1].getInt());
  datastreams[2].setInt(sensorReadings[kReadingsPerReport-1].getInt());
  datastreams[3].setInt(freeMemReadings[kReadingsPerReport-1].getInt());
  
  // Sort out the times FIXME!!!
  for (int idx = 0; idx < kReadingsPerReport; idx++)
  {
    batteryReadings[idx].setTime(readingsTime);
    chargingReadings[idx].setTime(readingsTime);
    sensorReadings[idx].setTime(readingsTime);
    freeMemReadings[idx].setTime(readingsTime);
    readingsTime += kReadingInterval*60;
  }
  
#if DEBUG
  Serial.println("putting data");
  Serial.println(feed);
#endif
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
#if DEBUG
  Serial.println("finished putting data");
#endif

  while ( gsmAccess.shutdown() != 1 ) {
    delay (1000);
  }
}

