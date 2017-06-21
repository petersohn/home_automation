// #ifndef INTERFACE_CPP
// #define INTERFACE_CPP
//
// #include "Interface.hpp"
//
// namespace {
//
// String getBinaryState(bool value) {
//     return value ? "on" : "off";
// }
//
// } // unnamed namespace
//
// HttpResult GpioInput::answer(DynamicJsonBuffer& #<{(|buffer|)}>#, String url) {
//     if (url.length() != 0) {
//         return {405, "Input pin cannot be set"};
//     }
//
//     return {200, port.getState()};
// }
//
// String GpioInput::get() {
//     return getBinaryState(port.getState());
// }
//
// HttpResult GpioOutput::answer(
//         DynamicJsonBuffer& buffer, String url) {
//     if (url.length() == 0) {
//         return {200, port.getState()};
//     }
//
//     bool value;
//     if (url == "1" || url.equalsIgnoreCase("on")) {
//         value = true;
//     } else if (url == "0" || url.equalsIgnoreCase("off")) {
//         value = false;
//     } else {
//         return {405, buffer.strdup("Invalid value: " + url)};
//     }
//
//     port.setState(value);
//     return {200, value};
// }
//
// String GpioOutput::get() {
//     return getBinaryState(port.getState());
// }
//
// #endif // INTERFACE_CPP
