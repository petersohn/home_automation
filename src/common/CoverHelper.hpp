#ifndef COVER_HELPER_HPP
#define COVER_HELPER_HPP

inline bool getActualValue(bool value, bool invert) {
    return invert ? !value : value;
}

#endif  // COVER_HELPER_HPP
