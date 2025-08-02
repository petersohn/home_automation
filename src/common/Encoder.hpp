#ifndef ENCODER_HPP
#define ENCODER_HPP

class Encoder {
public:
    virtual int read() = 0;
    virtual ~Encoder() {}
};

#endif  // ENCODER_HPP
