# Context

This is a generic firmware for ESP8266 using the Arduino API. The behavior is decided by config files uploaded to the device.

# Project structure

- `src` contains the main source code.
    - Source files directly within `src` are platform-dependent and are only built for the device. They are not tested. 
    - Source files within `src`'s subdirectories are platform-independent and can be unit tested.
- `test` contains the tests and is only build locally.
- `build` contains the build artifacts for tests.
- `bin` contains CLI tools used for manual testing.

# Building and testing

## Architecture

- The C++ version used is C++17.
- Due to device limitations, only header-only standard libraries can be used for code that is built on the device. For tests, there is no such limitation.
- The device is 32 bits, but the tests are built for the native architecture, which is usually 64 bits, so common code's correctness should not depend on either.
- Exceptions are not supported on device code. They can be only used in tests.

## Building

To build the project, run this in the project root:
```
arduino-cli compile --fqbn esp8266:esp8266:generic --verify
```

To build the test, run this from `build/`:
```
cmake
make -j <cpu_count>
```

**Important:** after every change, build the project and run the tests.

## Testing

Testing uses the Boost test framework. To run the tests, run:

```
./build/home_automation_test --log_level=test_suite [ -t test_to_run ]
```

## Compilation database

`cmake` creates a `compile_command.json` which contains a complete compilation database for the testable code.

There are dependencies for device code that is not visible from the project. To get more information for this, run the following command:

```
arduino-cli compile --fqbn esp8266:esp8266:generic --only-compilation-database --json
```

Note that this is not formatted in the style of a `compile_commands.json`, but it should contain all relevant paths.

# Coding conventions

- When modifying a source file, format it using `clang-format`.
- Instance members should always be prefixed with `this->`.

## Coding considerations

- The code is built to an embedded device, efficiency and small code size are important. But also keep the code readable and secure.

## Device specific considerations

- In testable code, use `EspApi` to call ESP-specific functions.
- In device specific code, direct calls to the Arduino API can be used.
- The overload of `attachInterrupt` that takes a `std::function` should not be used because it is buggy. Use the overload that takes a function pointer.
- An interrupt handler and everything called within the handler must be tagged with `IRAM_ATTR`: `void IRAM_ATTR interruptHandler() { ... }`
