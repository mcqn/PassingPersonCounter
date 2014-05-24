PassingPersonCounter
====================

Arduino sketch to periodically log PIR-sensed movements to a Xively feed.

### Hardware

 * Hera200 board (effectively an Arduino Mega2560 plus GSM shield, LiPo battery and charging circuit)
 * PIR sensor
 * Solar panel [Optional, for charging.  Not needed if you've got a local power source]

### Software

This code requires a couple of libraries not part of the standard Arduino distribution:

 * [Xively Arduino](https://github.com/xively/xively_arduino)
 * [HttpClient](https://github.com/amcewen/HttpClient) (used by the Xively Arduino library)
 * [JeeLib](https://github.com/jcw/jeelib) (for the low-power sleep functionality)

### Install

 1. Install the additional Arduino libraries listed above
 1. Rename XivelyKeys.h.example to XivelyKeys.h
 1. Create a new feed in Xively.
 1. Copy the feed ID and the key to access it into the relevant parts of XivelyKeys.h


