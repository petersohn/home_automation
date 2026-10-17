# Vendored libb64 decoder for native tests

`cdecode.h` and `cdecode.cpp` are verbatim copies of the libb64 decoder
bundled with the ESP8266 Arduino core, taken from the local core
installation:

- Source path: `cores/esp8266/libb64/cdecode.h` and
  `cores/esp8266/libb64/cdecode.cpp`
- ESP8266 core version: 3.1.2
  (`~/.arduino15/packages/esp8266/hardware/esp8266/3.1.2/`)
- Upstream project: libb64, http://sourceforge.net/projects/libb64
- License: public domain (see the notice in each file header)

The device firmware uses the same decoder through the header bundled with
the core (`libb64/cdecode.h`, `base64_decode_chars`); no second decoder
implementation is used or written. These vendored copies exist so the
native test target compiles the matching decoder source with the same
header path (`libb64/cdecode.h`), keeping device and test decoding
behavior identical.

`pgmspace.h` is a test-only shim providing the ESP8266 `PROGMEM` /
`pgm_read_byte` API on the host so the verbatim decoder source compiles
unchanged; it is not part of the vendored decoder and is never used on
the device.

The vendored files are third-party public-domain code and are not
reformatted; they are compiled as-is (compiler warnings are disabled for
this target in `CMakeLists.txt`).
