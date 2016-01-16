# Table of Contents

- [Introduction](#introduction)
- [Requirements](#requirements)
    - [Hardware requirements](#hardware-requirements)
        - [Devices](#devices)
        - [Server](#server)
        - [Development](#development)
    - [Software requirements](#software-requirements)
        - [Server](#server-1)
        - [Development](#development-1)
- [Configuration](#configuration)
    - [Setting up the web server](#setting-up-the-web-server)
    - [Setting up the PostgreSQL server](#setting-up-the-postgresql-server)
    - [Configuration variables](#configuration-variables)
        - [Environment variables](#environment-variables)
        - [Variables in configuration files](#variables-in-configuration-files)
- [Building](#building)
- [build only the server](#build-only-the-server)
- [Build the device code, but do not upload it](#build-the-device-code-but-do-not-upload-it)
- [Build everything, but ignore the errors](#build-everything-but-ignore-the-errors)
    - [Permissions of serial devices](#permissions-of-serial-devices)
- [Tests](#tests)

# Introduction

This is a home automation framework based on the ESP8266 WiFi-enabled
microcontroller.The microcontrollers (from now on, called *devices*) can
control one or more appliances (such as lamps) and/or sensors (switches). The
whole system is coordinated by a central server over HTTP.

The system can be monitored and configured through a web interface (not yet
implemented).

**Note:** The project is still under development. It is not yet tested in any
real environment.

# Requirements

## Hardware requirements

The system requires a central server and one or more ESP8266 devices.

### Devices

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

### Server

The server should be a computer capable of running a HTTP server and PostgreSQL
database server. It can be a low-power machine such as a Raspberry Pi (not yet
tested) or any PC. It should be connected to the same network as the devices. A
wired connection is recommended.

### Development

Development can be done on any PC that meets the requirement of the server (it
is recommended to use the same machine for server and development for testing).
It needs to have a serial port (or USB if an USB to TTL adapter is used) for
flashing the device.

## Software requirements

### Server

* OS: Tested under Linux, but it should be work on any OS that can meet the
  other requirements.
* [PostgreSQL](http://www.postgresql.org) server
* Web server with FastCGI capability (tested with
  [lighttpd](https://www.lighttpd.net/)).
* Python 3 (tested with 3.5) with the following modules installed:
  * [flipflop](https://pypi.python.org/pypi/flipflop) (for FastCGI server).
  * [psycopg2](http://initd.org/psycopg/) (for PostgreSQL access).

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
* Use `server/www` as the document root.
* Use `server/fcgi.py` as FastCGI server.
* Use this FastCGI server for all `.py` files.
* Make `index.py` files automatically completed (e.g. the URL `/` should point
  to `/index.py`).
* It is recommended to allow connection keep-alive with a high (> 1 min)
  timeout, because opening new connections every time wastes the resources of
  the devices. Because TCP connections remain in TIME_WAIT after closing, it
  can make them run out of memory fast.

An example `lighttpd.conf` file can be found
[here](example_config/lighttpd.conf).

## Setting up the PostgreSQL server

The server requires a connection to an arbitrary PostgreSQL database The
connection parameters for this database can be configured (see [Configuration
variables](#configuration-variables)). The contents of the database can be set
up with the `server/sql/tables.sql` file. For example:

```bash
postgres@machine:~$ psql postgres <<<'create database home_automation;'
postgres@machine:~$ psql home_automation -f /home/username/workspace/home_automation/server/sql/tables.sql
```

Additionally, in order to run the tests for the database binding, it is
recommended to have access to the `postgres` database in order to create
databases. In typical PostgreSQL installations, it can be achived by either
creating a role with the same name as your Linux user with SUPERUSER or
CREATEDB privileges, or running the tests as `postgres` Linux user. See [the
PostgreSQL
documentation](http://www.postgresql.org/docs/9.4/interactive/user-manag.html)
for more details.

## Configuration variables

The configuration of the environment is done through two ways:
* Environemet variables for options that affect how the build is done but not
  the build code directly.
* Configuration files written in JSON for options that affect the code itself.
  These options are are used to generate C++/Python source files by tup.

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

Additionally, the following is used by the server test:
* `TEST_SERVER_ADDRESS`: The address (host:port) used for connecting to the web
  server. It is optional and defaults to `127.0.0.1:8080`.

### Variables in configuration files

Some configuration variables are built into the code. The directories `device/src/config` and `server/lib/config` contains template files that are are converted to real C++/Python source files by tup using these variables.

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
# build only the server
tup server

# Build the device code, but do not upload it
tup device/sec/main/main.bin

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

# Tests

The server has some tests implemented in Python's `unittest`. This can be found
in `server/test`. The tests can be run by the `server/test/runTest.sh` script,
which in addition to running the tests, creates a unique database, runs the
tests, then drops the database. Arguments are passed to Python's unit test
runner, as described in [the
documentation](https://docs.python.org/2/library/unittest.html#command-line-options).

The tests can also be run manually. In this case an empty database (without any
tables in it) must be provided to it. In this case, the following parameters
are mandatory:
* `--connectString`: Connect string used to access the database.
* `--testServerAddress`: Address (host:port) of the running web server.
