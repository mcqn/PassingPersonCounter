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

// The Mega-Hera200 doesn't expose Serial to the expansion connector
// but Serial3 is on PJ0 and PJ1
#define LOG_SERIAL_PORT Serial3

// How long each reading set should span (IN MINUTES!)
const unsigned long kReadingInterval =15UL;
// How many readings we should take before we report them to the Internet
const int kReadingsPerReport = 8;

// PIR sensor constants
const int kPIRSensorPin = 20;
const int kPIRSensorInterrupt = 3;

#ifdef SERIAL_NUMBER
const int kActivityDetectedLED = 22;
const int kReportingLED = 26;
#else
const int kActivityDetectedLED = 13;
const int kReportingLED = 12;
#endif

// Define the strings for our datastream IDs
char sensorId[] = "Movement";
char batteryId[] = "Battery";
char chargingId[] = "Charge";
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
XivelyHistoricDatastream datastreams[] = {
  XivelyHistoricDatastream(batteryId, strlen(batteryId), DATASTREAM_FLOAT, batteryReadings, kReadingsPerReport),
  XivelyHistoricDatastream(chargingId, strlen(chargingId), DATASTREAM_INT, chargingReadings, kReadingsPerReport),
  XivelyHistoricDatastream(sensorId, strlen(sensorId), DATASTREAM_INT, sensorReadings, kReadingsPerReport)
};
// Finally, wrap the datastreams into a feed
XivelyHistoricFeed feed(xivelyFeedId, datastreams, 3 /* number of datastreams */);

// Initialize the variables in setup() too, so if the quick-and-dirty
// soft reset skips the initialisation they'll still get set up correctly
volatile int movementCount =0;
volatile unsigned long movementStart =0;
int lastMovementCount =0;
unsigned long lastMovementTime =0;
unsigned long lastReadingTime =0;
unsigned long lastReportTime;

// How long it'd take one person to pass by the sensor
const unsigned long kPassingTime =30*1000;

// boilerplate for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup() {
  // Do all initialisation of variables in setup() so that if we have to soft-reset
  // they'll get initialised (not 100% sure we need it, but better safe than sorry)
  movementCount =0;
  movementStart =0;
  lastMovementCount =0;
  lastMovementTime =0;
  lastReadingTime =0;
  lastReportTime = millis();

#if DEBUG
  LOG_SERIAL_PORT.begin(9600);
  while (!LOG_SERIAL_PORT) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  LOG_SERIAL_PORT.println("Let's go!");
#endif
  pinMode(kActivityDetectedLED, OUTPUT);
  pinMode(kReportingLED, OUTPUT);
  pinMode(4, OUTPUT);
  digitalWrite(kReportingLED, LOW);
  digitalWrite(kActivityDetectedLED, LOW);
  delay(1000);
  digitalWrite(kReportingLED, HIGH);
  digitalWrite(kActivityDetectedLED, HIGH);

  pinMode(kPIRSensorPin, INPUT);
  attachInterrupt(kPIRSensorInterrupt, movementDetected, CHANGE);
  
  // Work out what time it is
  findTime();
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
    
    sensorReadings[readingIdx].setInt(movementCount);
    movementCount = 0;
    for (int i=0; i < sensorReadings[readingIdx].getInt(); i++)
    {
      digitalWrite(kReportingLED, LOW);
      delay(300);
      digitalWrite(kReportingLED, HIGH);
      delay(200);
    }
    // Record when we took these readings
    time_t readingsTime = now();
    batteryReadings[readingIdx].setTime(readingsTime);
    chargingReadings[readingIdx].setTime(readingsTime);
    sensorReadings[readingIdx].setTime(readingsTime);
#if DEBUG
    LOG_SERIAL_PORT.println("Readings...");
    LOG_SERIAL_PORT.print("Time: ");
    LOG_SERIAL_PORT.println(readingsTime);
    LOG_SERIAL_PORT.print("Battery: ");
    LOG_SERIAL_PORT.println(batteryReadings[readingIdx].getFloat());
    LOG_SERIAL_PORT.print("Charging: ");
    LOG_SERIAL_PORT.println(chargingReadings[readingIdx].getInt());
    LOG_SERIAL_PORT.print("Movement: ");
    LOG_SERIAL_PORT.println(sensorReadings[readingIdx].getInt());
    LOG_SERIAL_PORT.println(feed);
#endif
  }

  digitalWrite(kReportingLED, LOW);

#if 1
  reportMovementCount();
  digitalWrite(kReportingLED, HIGH);
#else
  if (movementCount > lastMovementCount)
  {
    int diff = movementCount - lastMovementCount;
    lastMovementCount = movementCount;
#if 1
    for (int i=0; i < diff; i++)
    {
      digitalWrite(kReportingLED, LOW);
      delay(300);
      digitalWrite(kReportingLED, HIGH);
      delay(200);
    }
    
    reportMovementCount(diff);
#else
    unsigned long movementTime = millis();
    
    LOG_SERIAL_PORT.print(movementTime);
    LOG_SERIAL_PORT.print(" (");
    LOG_SERIAL_PORT.print(movementTime-lastMovementTime);
    LOG_SERIAL_PORT.print("): movementCount is ");
    LOG_SERIAL_PORT.println(lastMovementCount);
    lastMovementTime = movementTime;
#endif
  }
#endif
}

void movementDetected()
{
  // While we're in the movement interrupt, check that the
  // main loop hasn't stalled (it seems to every now and then, sadly)
  if (millis()-lastReportTime > (1+kReadingsPerReport)*kReadingInterval*60*1000UL)
  {
    // Looks like things have stalled.  Let's restart...
    // (ideally we'd use the watchdog timer to kick off a proper reset, but
    // I couldn't get that working in testing - maybe something to do with
    // using it for the sleeping)
    asm volatile ("  jmp 0");  
  }
  
  if (digitalRead(kPIRSensorPin))
  {
    digitalWrite(kActivityDetectedLED, LOW);
    movementStart = millis();
  }
  else
  {
    digitalWrite(kActivityDetectedLED, HIGH);
    // Find out how long we've been active, and use that to guide
    // how many people we think have passed
    // But really short readings will be the sensor lying to us
    if (millis()-movementStart > 3300)
    {
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
  }
}

void findTime()
{
  GSMClock modem;
  
  if(modem.begin()) 
  {
#if DEBUG
    LOG_SERIAL_PORT.println("modem.begin() succeeded");
#endif
  }
  else
  {
#if DEBUG
    LOG_SERIAL_PORT.println("ERROR, no modem answer.");
#endif
  }

  if (modem.getTime()[0] == '0')
  {
    // The modem is reporting a time from the 2000s, which means
    // it's not the right time
#if !DEBUG
    LOG_SERIAL_PORT.begin(9600);
#endif
    
    // Try to set it from the network
    if (!modem.setTimeFromNetwork(1*60*1000UL))
    {
      LOG_SERIAL_PORT.println("Didn't get the time signal from the network");
      // Give the user a window to set it via the serial monitor
      LOG_SERIAL_PORT.println("Enter the time (in seconds since Jan 1 1970):");
      LOG_SERIAL_PORT.setTimeout(5*60*1000UL);
      unsigned long the_time = Serial3.parseInt();
      LOG_SERIAL_PORT.print("Got time of ");
      LOG_SERIAL_PORT.println(the_time);
      if (the_time > 0)
      {
        // We've been given a time, so let's use it
        modem.setTime(timeToString(now()));
      }
    }
  }
  // Set the Time library time from the modem
  String modemTime = modem.getTime();
  LOG_SERIAL_PORT.print("The time is ");
  LOG_SERIAL_PORT.println(modemTime);
  setTimeFromModem(modemTime);
  LOG_SERIAL_PORT.print("The internal time is ");
  LOG_SERIAL_PORT.println(now());
#if !DEBUG
  LOG_SERIAL_PORT.end();
#endif
}

String timeToString(time_t aTime)
{
  TimeElements time_elements;
  breakTime(aTime, time_elements);
  String ret(tmYearToY2k(time_elements.Year));
  ret += "/";
  if (time_elements.Month < 10)
  {
    ret += "0";
  }
  ret += time_elements.Month;
  ret += "/";
  if (time_elements.Day < 10)
  {
    ret += "0";
  }
  ret += time_elements.Day;
  ret += ",";
  if (time_elements.Hour < 10)
  {
    ret += "0";
  }
  ret += time_elements.Hour;
  ret += ":";
  if (time_elements.Minute < 10)
  {
    ret += "0";
  }
  ret += time_elements.Minute;
  ret += ":";
  if (time_elements.Second < 10)
  {
    ret += "0";
  }
  ret += time_elements.Second;
  ret += "+00";
  return ret;
}

void setTimeFromModem(String& aTime)
{
  const int kDateTimeStringLength = 20;
  const int kYearOffset = 0;
  const int kYearLength = 2;
  const int kMonthOffset = 3;
  const int kMonthLength = 2;
  const int kDayOffset = 6;
  const int kDayLength = 2;
  const int kHourOffset = 9;
  const int kHourLength = 2;
  const int kMinuteOffset = 12;
  const int kMinuteLength = 2;
  const int kSecondOffset = 15;
  const int kSecondLength = 2;
  
  if (aTime.length() >= kDateTimeStringLength)
  {
    String component = aTime.substring(kYearOffset, kYearOffset+kYearLength);
    tmElements_t time_elements;
#if DEBUG
    LOG_SERIAL_PORT.println(component);
#endif
    time_elements.Year = y2kYearToTm(component.toInt());
    component = aTime.substring(kMonthOffset, kMonthOffset+kMonthLength);
    time_elements.Month = component.toInt();
    component = aTime.substring(kDayOffset, kDayOffset+kDayLength);
    time_elements.Day = component.toInt();
    component = aTime.substring(kHourOffset, kHourOffset+kHourLength);
    time_elements.Hour = component.toInt();
    component = aTime.substring(kMinuteOffset, kMinuteOffset+kMinuteLength);
    time_elements.Minute = component.toInt();
    component = aTime.substring(kSecondOffset, kSecondOffset+kSecondLength);
    time_elements.Second = component.toInt();
    
    // Now use that to set the time
    setTime(makeTime(time_elements));
  }
}

void reportMovementCount()
{
  GSM gsmAccess;     // include a 'true' parameter to enable debugging
  GPRS gprs;
  GSMClient client;
  XivelyClient xivelyclient(client);

#if DEBUG
  LOG_SERIAL_PORT.println("reporting movement count");
#endif
  // connection state
  boolean notConnected = true;
  // After starting the modem with GSM.begin()
  // attach the shield to the GPRS network with the APN, login and password
  while(notConnected)
  {
    if((gsmAccess.begin(PINNUMBER)==GSM_READY) &
      (gprs.attachGPRS(GPRS_APN, GPRS_LOGIN, GPRS_PASSWORD)==GPRS_READY))
    {
      notConnected = false;
    }
#if 0
    else
    {
      digitalWrite(4, HIGH);
      delay(1000);
      digitalWrite(4, LOW);
    }
#endif
  }

#if DEBUG
  LOG_SERIAL_PORT.println("connected");
#endif
  // Set current value to whatever the last reading was
  datastreams[0].setFloat(batteryReadings[kReadingsPerReport-1].getFloat());
  datastreams[1].setInt(chargingReadings[kReadingsPerReport-1].getInt());
  datastreams[2].setInt(sensorReadings[kReadingsPerReport-1].getInt());

#if 0 // properly implementing time now 
  // Sort out the times FIXME!!!
  for (int idx = 0; idx < kReadingsPerReport; idx++)
  {
    time_t readingsTime = now();
    batteryReadings[idx].setTime(readingsTime);
    chargingReadings[idx].setTime();
    sensorReadings[idx].setTime(readingsTime);
    freeMemReadings[idx].setTime(readingsTime);
    readingsTime += kReadingInterval*60;
  }
#endif
  
#if DEBUG
  LOG_SERIAL_PORT.println("putting data");
  LOG_SERIAL_PORT.println(feed);
#endif
  if (xivelyclient.put(feed, xivelyKey) < 0)
  {
    // Something went wrong
#if 0
    for (int i=0; i < 4; i++)
    {
      digitalWrite(4, HIGH);
      delay(1000);
      digitalWrite(4, LOW);
      delay(200);
    }
#endif
  }
#if DEBUG
  LOG_SERIAL_PORT.println("finished putting data");
#endif

  while ( gsmAccess.shutdown() != 1 ) {
    delay (1000);
  }
  
  // Mark that we've successfully reported
  // so the manual watchdog in the movement ISR doesn't trip
  lastReportTime = millis();
}

