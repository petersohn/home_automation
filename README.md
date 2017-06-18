[![Build Status](https://travis-ci.org/petersohn/home_automation.svg?branch=master)](https://travis-ci.org/petersohn/home_automation)

# Table of Contents

- [Introduction](#introduction)
- [Requirements](#requirements)
    - [Hardware requirements](#hardware-requirements)
        - [ESP8266 Devices](#esp8266-devices)
        - [Raspberry Pi Devices](#raspberry-pi-devices)
        - [Server](#server)
        - [Development](#development)
    - [Software requirements](#software-requirements)
        - [Server](#server-1)
        - [Development](#development-1)
- [Configuration](#configuration)
    - [Setting up the web server](#setting-up-the-web-server)
    - [Setting up the database server](#setting-up-the-database-server)
    - [Setting up a Raspberry Pi device](#setting-up-a-raspberry-pi-device)
    - [Configuration variables](#configuration-variables)
        - [Environment variables](#environment-variables)
        - [Variables in configuration files](#variables-in-configuration-files)
- [Building](#building)
- [Build the device code, but do not upload it](#build-the-device-code-but-do-not-upload-it)
- [Build everything, but ignore the errors](#build-everything-but-ignore-the-errors)
    - [Permissions of serial devices](#permissions-of-serial-devices)
- [Automatic deployment](#automatic-deployment)
- [Tests](#tests)

# Introduction

This is a home automation framework based on the ESP8266 WiFi-enabled
microcontroller. The microcontrollers (from now on, called *devices*) can
control one or more appliances (such as lamps) and/or sensors (switches). The
whole system is coordinated by a central server over HTTP.

The system integrates with [Home Assistant](https://home-assistant.io/).

**Note:** The project is still under development. It is not yet tested in any
real environment.

# Requirements

## Hardware requirements

The system requires a central server and one or more ESP8266 devices.

### ESP8266 Devices

The ESP8266 requires a stable 3.3V input voltage (according to the
specifications, it works from 1.7V to 3.6V, but stable power is needed
otherwise unwanted resets may occur). It can be run from 2 cell batteries (not
recommended for home automation), or from an external power supply, for example
an 5V phone charger and a voltage regulator (for example, an AMS1117-3.3V).

Programming the device is done through serial port. It can be programmed
through USB with an USB to TTL module. These modules usually provide 3.3V
output, but with low power that is only enough to drive the communication
itself, but not enough to power the ESP8266 device. The 5V output of the USB
with a voltage regulator can be used though.

Communication with the server is done through WiFi (b/g/n) network.

Here is the pin layout of the most common ESP8266 configuration. It supports
GPIO ports 0 and 2. Note that in order for the device to boot up properly,
these ports should be pulled up to logical 1 value at boot time.

![ESP8266 pin layout](data/ESP8266.jpg)

It is possible to use additional two GPIO ports. If the serial port is
disabled, its ports can be used the same way as the other ports. The UTXD port
is GPIO 1 and URXD is GPIO 3.

### Server

The server can be any machine that can run Home Assistant. It must be connected
to the same network as the ESP devices. Since encrypted connections are not yet
supported, the HTTP ports of either Home Assistant or the ESP devices should
not be reachable from the internet. For secure access to Home Assistant from
outside, a proxy server is recommended (e.g. Apache, set to listen on HTTPS and
forward requests to Home Assistant through HTTP on the local network).

### Development

Development can be done on any PC that meets the requirement of the server (it
is recommended to use the same machine for server and development for testing).
It needs to have a serial port (or USB if an USB to TTL adapter is used) for
flashing the device.

## Software requirements

### Server

Home Assistant should be running on the server. Password authentication should
be turned on (the device always sends the `x-ha-access` header. Status updates
are sent automatically to the server.

### Development

* [ESP8266 toolchain for Eclipse](https://github.com/esp8266/Arduino/blob/master/doc/eclipse/eclipse.rst).
* [ESP8266 toolchain for Arduino IDE](https://github.com/esp8266/Arduino/)
  with [SPIFFS support](https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#uploading-files-to-file-system).

# Configuration

The following steps assume that the software described in
[Software requirements](#software-requirements) are installed properly.

## Configuration files

The configuration of the device is done by uploading the following files to the
ESP:

* `global_config.json`
* `device_config.json`

Examples for these config files are found in the
[example_config](example_config/) directory.

To upload these files to the device, create an empty sketch with the Arduino
IDE and set it up with the parameters of your ESP device. Follow [these
instructions](https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#uploading-files-to-file-system)
to upload the file to the device.

# Building

The program can be compiled and uploaded to the device from Eclipse.
