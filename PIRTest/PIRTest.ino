#include <GSM.h>
#include <Time.h>

const int kReadingsBufferSize = 30;
volatile int gReadingIdx = 0;
unsigned long gReadingsBuffer[30];

#define MEGA 0

#if MEGA
// The Mega-Hera200 doesn't expose Serial to the expansion connector
// but Serial3 is on PJ0 and PJ1
#define LOG_SERIAL_PORT Serial3
// PIR sensor constants
const int kPIRSensorPin = 20;
const int kPIRSensorInterrupt = 3;

#else

// PIR sensor constants
const int kPIRSensorPin = 1;
const int kPIRSensorInterrupt = 3;
#define LOG_SERIAL_PORT Serial

#endif


volatile int movementCount =0;
volatile unsigned long movementStart =0;
int lastMovementCount =0;
unsigned long lastMovementTime =0;


void setup() {
  // put your setup code here, to run once:
  LOG_SERIAL_PORT.begin(9600);
  while (!LOG_SERIAL_PORT) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  pinMode(kPIRSensorPin, INPUT);
  attachInterrupt(kPIRSensorInterrupt, movementDetected, CHANGE);
  LOG_SERIAL_PORT.println("Let's go!");
  
  findTime();
}

volatile int gLastIdx = 0;

class PrintTime : public Printable
{
public:
  PrintTime(time_t aTime) : iTime(aTime) {};
  virtual size_t printTo(Print& p) const;
protected:
  time_t iTime;
};

size_t PrintTime::printTo(Print& p) const
{
  size_t ret = 0;
  tmElements_t time_elements;
  
  breakTime(iTime, time_elements);
  ret += p.print(tmYearToCalendar(time_elements.Year));
  ret += p.print("-");
  if (time_elements.Month < 10)
  {
    ret += p.print("0");
  }
  ret += p.print(time_elements.Month);
  ret += p.print("-");
  if (time_elements.Day < 10)
  {
    ret += p.print("0");
  }
  ret += p.print(time_elements.Day);
  ret += p.print("T");
  if (time_elements.Hour < 10)
  {
    ret += p.print("0");
  }
  ret += p.print(time_elements.Hour);
  ret += p.print(":");
  if (time_elements.Minute < 10)
  {
    ret += p.print("0");
  }
  ret += p.print(time_elements.Minute);
  ret += p.print(":");
  if (time_elements.Second < 10)
  {
    ret += p.print("0");
  }
  ret += p.print(time_elements.Second);
  ret += p.print("Z");
  
  return ret;
}

void loop() {
  // put your main code here, to run repeatedly: 
  int newIdx = gReadingIdx;
  if (newIdx != gLastIdx)
  {
    // We've got a new reading
    unsigned long the_time = now();
    PrintTime pt(the_time);
    LOG_SERIAL_PORT.print(pt);
    LOG_SERIAL_PORT.print(",");
    LOG_SERIAL_PORT.print(the_time);
    LOG_SERIAL_PORT.print(",");
    LOG_SERIAL_PORT.print(gLastIdx);
    LOG_SERIAL_PORT.print(",");
    LOG_SERIAL_PORT.println(gReadingsBuffer[gLastIdx]);
    gLastIdx = newIdx;
  }
  delay(100);
}

void movementDetected()
{
  if (digitalRead(kPIRSensorPin))
  {
    movementStart = millis();
  }
  else
  {
    // Find out how long we've been active, and use that to guide
    // how many people we think have passed
    movementCount++;
    // AMc - the 4430 is the right value, using the lower one
    // AMc - during testing to check we get decent readings
//    if (millis()-movementStart > 4430) {
    if (millis()-movementStart > 4030) {
      gReadingsBuffer[gReadingIdx++] = millis()-movementStart;
      if (gReadingIdx >= kReadingsBufferSize)
      {
        gReadingIdx = 0;
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
      unsigned long the_time = LOG_SERIAL_PORT.parseInt();
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


