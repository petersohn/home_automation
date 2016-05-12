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

Raspberry Pi devices are also supported. This means that GPIO ports on a
Raspberry Pi can be controlled the same way as ESP8266 devices. It can be
useful if a Raspberry Pi is used as the central server. In this case its GPIO
ports can also be utilized.

The system can be monitored and configured through a web interface (not yet
implemented).

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

### Raspberry Pi Devices

GPIO ports of the Raspberry Pi can be controlled the same way as the ESP8266.
It is useful for example when the server runs on a Raspberry Pi.

Here is the pin layout for the Raspberry Pi Model B+. Most other models use the
same layout.

![Raspberry Pi pin layout](data/Raspberry-Pi-Model-B+.png)

Other than the usage of GPIO ports and a working network connection, no special
hardware configuration is required. See
[here](#setting-up-a-raspberry-pi-device) for the software configuration.

### Server

The server should be a computer capable of running a HTTP server with Django.
It can be a low-power machine such as a Raspberry Pi or any PC. It should be
connected to the same network as the devices. A wired connection is
recommended.

### Development

Development can be done on any PC that meets the requirement of the server (it
is recommended to use the same machine for server and development for testing).
It needs to have a serial port (or USB if an USB to TTL adapter is used) for
flashing the device.

## Software requirements

### Server

* OS: Tested under Linux, but it should be work on any OS that can meet the
  other requirements.
* Database server (tested with [PostgreSQL](http://www.postgresql.org))
* Web server that works with Django (tested with
  [lighttpd](https://www.lighttpd.net/)).
* Python 3 (tested with 3.5) with the following modules installed:
  * [Django](https://www.djangoproject.com/) and its dependencies
    * For PostgreSQL, Django uses [psycopg2](http://initd.org/psycopg/).

### Development

* OS: Tested under Linux, but it should work on other Unix-like systems.
  Windows is not supported.
* PostgreSQL, HTTP server and Pyhton requirements are the same as for the
  server.
* [tup](https://github.com/gittup/tup) (it also requires FUSE).
* [ESP8266 toolchain for the Arduino IDE](https://github.com/esp8266/Arduino/).

# Configuration

The following steps assume that the software described in
[Software requirements](#software-requirements) are installed properly.

For the purpose of examples, the repository is cloned into
`/home/username/workspace/home_automation`.

## Setting up the web server

The following configurations are required for the web server:
* Use `server/home_automation/home` as the document root.
* Use `server/home_automation/home_automation/wsgi.py` as FastCGI server.
* Use FastCGI for all files not in `/static/.
  * Disable checking of the availability of files on the filesystem.
* Make `index.html` files automatically completed (e.g. the URL `/` should point
  to `/index.html`).
* It is recommended to allow connection keep-alive with a high (> 1 min)
  timeout, because opening new connections every time wastes the resources of
  the devices. Because TCP connections remain in TIME_WAIT after closing, it
  can make them run out of memory fast.

An example `lighttpd.conf` file can be found
[here](example_config/lighttpd.conf).

The web server requires [JQuery UI](http://jqueryui.com/) to be present in
`server/home_automation/home/static/home` under `jquery-ui` and
`jquery-ui-themes` directories. These can be automatically downloaded using the
[automatic deployment script](#automatic-deployment).

## Setting up the database server

Django requires a connection to a database. The default settings can be changed
in `server/home/home_automation/home_automation/settings.py`, or by setting the
environment variable `HOME_DJANGO_CONFIG_FILE` to a JSON file with the
following contents:

```
{
    "name": "name-of-the-database",
    "user": "database-role-to-use"
}
```

See the Django documentation for more information about database connection.

## Setting up a Raspberry Pi device

The program for the Raspberry Pi device is located under `device/pi`. It
requires a FastCGI compatible web server. It can be the same web server that
the server runs on, but they should listen on different ports.
* Use `device/pi/fcgi.py` as a FastCGI server.
* Use FastCGI for all files.
* Only allow 1 instance of the FastCGI server.
* Disable checking of the availability of files on the filesystem.

An example `lighttpd.conf` file can be found
[here](example_config/lighttpd.conf).

The configuration for the device is done a `config.json` file. It must be
located next to the `fcgi.py`. [Here](example_config/pi_config.json) is an
example for such a config file.
* The `port` parameter must be the same as the port configured in the web
  server.
* The `server` parameter controls the connection to the server. If the server
  runs on the same Raspberry Pi, it can be `127.0.0.1`. It can also be any IP
  address or hostname accepted by Python's HTTP client implementation.
* The `number` parameter under `pins` is the board pin number, which is labeled
  on the pins on the above imgage (as opposed to the BCM pin number, labeled as
  GPIOxx; although it can be changed in `gpio.py`).

## Configuration variables

The configuration of the environment is done through two ways:
* Environemet variables for options that affect how the build is done but not
  the build code directly.
* Configuration files written in JSON for options that affect the code itself.
  These options are are used to generate C++ source files by tup.

### Environment variables

The following environment variables are used by tup:

* `ESP_DIR`: The directory containing the header and source files for the
  ESP8266 firmware.
* `ESP_COMPILER_DIR`: The directory containing the ESP8266 cross-compiler.
* `ESPTOOL`: The flash tool used to create ESP8266 images and to upload these
  images to the device.
* `ESP_PORT`: The device name for serial communication with the device.
* `ESP_CONFIG_FILES`: The config files used for code generation (see [Variables
  in configuration files](#variables-in-configuration-files)). A full path is
  recommended to be able to run tup from anywhere within the project.

An example configuration (the directories are for a typical installation
through the Arduino IDE's module manager):

```bash
export ESPTOOL="/home/username/.arduino15/packages/esp8266/tools/esptool/0.4.5/esptool"
export ESP_PORT="/dev/ttyUSB0"
export ESP_CONFIG_FILES="/home/username/workspace/home_automation/device/custom/config_common.json /home/username/workspace/home_automation/device/custom/testDevice.json /home/username/workspace/home_automation/device/custom/config_db.json"
export ESP_DIR="/home/username/.arduino15/packages/esp8266/hardware/esp8266/1.6.5-947-g39819f0"
export ESP_COMPILER_DIR="/home/username/.arduino15/packages/esp8266/tools/xtensa-lx106-elf-gcc/1.20.0-26-gb404fb9/"
```

The script `check_environment.sh` parses the tupfiles and checks for any unset
variables. Similarly, `save_environment.sh` saves these variables to a file (by
default `load_environment.sh`), which can be sourced to recover the environment,
so that there is no need to set each of them manually every time a new shell is
started.

Additionally, the following is used by the server at runtime:
* `HOME_DJANGO_CONFIG_FILE`: The config file for Django that contains database
  connection information. It is optional if the default server connection
  settings work. See [Setting up the database
  server](#setting-up-the-database-server) for more details.

### Variables in configuration files

Some configuration variables are built into the code. The directory
`device/src/config` contains template files that are are converted to real C++
source files by tup using these variables.

These variables are stored in one or more JSON files. These files should
contain an object whose keys are the configuration variable names and the
values are usually strings or numbers. It is also possible to use an array as a
value, which can contain strings, numbers and other arrays. These are converted
into (possibly nested) initializer lists for C++ code.

The JSON config files used by tup can be configured with the `ESP_CONFIG_FILES`
environment variable. Example files can be found in the `example_config`
directory, and contain the following variables:
* [`config_common.json`](example_config/config_common.json): This file contains
  configuration that is common in the system.
  * `ssid` (string): The SSID of the WiFi network to connect.
  * `password` (string): The password of the WiFi connection.
  * `serverAddress` (string): IP address of the server.
  * `serverPort` (number): Port used by the web server.
* [`config_device_specific.json`](example_config/config_device_specific.json):
  This file contains configuration that is specific to each device.
  * `debug` (bool): Whether debug messages are sent through the serial port. If
    the serial ports are used as GPIO, this should be set to false.
  * `deviceName` (string): The name of the device. It should be unique for all
    devices running at the same time.
  * `pins` (composite): The pin configuration of the device. It is an array of
    arrays with the following format: `[name, number, isOutput]`.
    * `name`: The name of the pin. It should be unique within one device.
    * `number`: The pin number on the device.
    * `isOutput`: If true, it is an output pin, otherwise an input.
* [`config_db.json`](example_config/config_db.json): While other config files
  are used by the device, this is used by the server.
  * `psql_connect_string`: The connection string used to connect to the
    database. See the [PostgreSQL
    documentation](http://www.postgresql.org/docs/9.4/interactive/libpq-connect.html#LIBPQ-PARAMKEYWORDS)
    for more information on the format.

Note that it is not necessary to arrange the config files this way: they will
be concatenated. However, the device specific configuration should be in a
separate file (or more separate files in case of multiple devices) to make
deploying more than one device easier.

# Building

When all configuration is set, simply run tup to build everything and upload to
the device:

```bash
tup
```

If you do not want to upload the code to device, you may use individual tup
targets, or run tup with `--keep-going` to avoid stoppin when it fails to
upload to the device:

```bash
# Build the device code, but do not upload it
tup device/esp/src/main/main.bin

# Build everything, but ignore the errors
tup --keep-going
```

## Permissions of serial devices

You may not have the permission to write to the necessary port. In
this case you can eiter build as root (not recommended) or add yourself to the
necessary group. For example:

```bash
% ls -l /dev/ttyUSB0
crw-rw---- 1 root uucp 188, 0 Jan 10 09:37 /dev/ttyUSB0
```

In this case, the serial device has the group `uucp`. Adding your user to the
`uucp` group solves the problem.

If the device has the group `root`, it is not recommended to add yourself to
this group. Instead, change the permissions of the file:

```bash
sudo chmod 666 /dev/ttyUSB0
```

Unfortunately, you have to do it every time you connect the device to the
computer.

# Automatic deployment

There is an automatic deployment found in `deploy`. The `create_deployment.py`
script creates a package that automatically installs/upgrades the server and
optionally the Raspberry Pi device. The installed server has the following
properties:
* It listens on port 80 (for the Raspberry Pi device, on port 81).
* It runs as user and group `home_automation:home_automation`.
* Deployed files are only writable by root and readable by the server but not
  other users.
* The database `home_automation` is used.
* The service `home_automation` is used for non-webserver related tasks by the
  server.

Requirements on the target machine:
* Linux that uses `systemd` for service management.
* Debian's `adduser` command.
* All [prerequisites](#server-1) installed.
  * The PostgreSQL server must be up and running and the `postgres` Linux user
    must be able to have superuser access.
  * The lighttpd should have a service installed as `lighttpd.service`.

The following is done at installation:
* The Linux user is created.
* The database is created.
* Files are copied to the user's home directory and the appropriate permissions
  are set.
* `lighttpd.conf` file is overwritten with the one provided by the package.
* Database migration is done by Django.
* The `home_automation` service is created.
* The lighttpd service is restarted.

The upgrade does the same except for the user and database creation.

# Tests

The server has some tests implemented in Django's test framework, which is
based on Python's `unittest`. This can be found in
`server/home_automation/home/tests.py`. The tests can be run by the Django
manage script (`server/home_automation/manage.py`) with the `test` command.
Note that a database connectivity is required for the tests (no set up database
is required though).

```bash
python3 ./manage.py test
```

