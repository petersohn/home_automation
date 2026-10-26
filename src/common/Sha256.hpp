#ifndef COMMON_SHA256_HPP
#define COMMON_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>

class Sha256 {
public:
    void update(uint8_t value) {
        this->buffer[this->bufferSize++] = value;
        ++this->totalBytes;
        if (this->bufferSize == this->buffer.size()) {
            this->transform();
            this->bufferSize = 0;
        }
    }

    std::array<uint8_t, 32> finish() {
        const uint64_t bitLength = this->totalBytes * 8;
        this->buffer[this->bufferSize++] = 0x80;
        if (this->bufferSize > 56) {
            while (this->bufferSize < this->buffer.size()) {
                this->buffer[this->bufferSize++] = 0;
            }
            this->transform();
            this->bufferSize = 0;
        }
        while (this->bufferSize < 56) {
            this->buffer[this->bufferSize++] = 0;
        }
        for (std::size_t index = 0; index < 8; ++index) {
            this->buffer[63 - index] =
                static_cast<uint8_t>(bitLength >> (index * 8));
        }
        this->transform();

        std::array<uint8_t, 32> digest{};
        for (std::size_t index = 0; index < this->state.size(); ++index) {
            for (std::size_t byte = 0; byte < 4; ++byte) {
                digest[index * 4 + byte] =
                    static_cast<uint8_t>(this->state[index] >> (24 - byte * 8));
            }
        }
        return digest;
    }

private:
    static uint32_t rotateRight(uint32_t value, uint32_t bits) {
        return (value >> bits) | (value << (32 - bits));
    }

    void transform() {
        static constexpr std::array<uint32_t, 64> constants = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
            0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
            0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
            0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
            0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
            0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
            0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
            0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
            0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
            0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::array<uint32_t, 16> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const std::size_t offset = index * 4;
            words[index] =
                (static_cast<uint32_t>(this->buffer[offset]) << 24) |
                (static_cast<uint32_t>(this->buffer[offset + 1]) << 16) |
                (static_cast<uint32_t>(this->buffer[offset + 2]) << 8) |
                static_cast<uint32_t>(this->buffer[offset + 3]);
        }
        uint32_t a = this->state[0];
        uint32_t b = this->state[1];
        uint32_t c = this->state[2];
        uint32_t d = this->state[3];
        uint32_t e = this->state[4];
        uint32_t f = this->state[5];
        uint32_t g = this->state[6];
        uint32_t h = this->state[7];
        for (std::size_t index = 0; index < constants.size(); ++index) {
            const std::size_t slot = index % words.size();
            uint32_t word = words[slot];
            if (index >= words.size()) {
                const uint32_t left = words[(slot + 1) % words.size()];
                const uint32_t right = words[(slot + 14) % words.size()];
                const uint32_t sigma0 =
                    rotateRight(left, 7) ^ rotateRight(left, 18) ^ (left >> 3);
                const uint32_t sigma1 = rotateRight(right, 17) ^
                                        rotateRight(right, 19) ^ (right >> 10);
                word =
                    word + sigma0 + words[(slot + 9) % words.size()] + sigma1;
                words[slot] = word;
            }
            const uint32_t sum1 =
                rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
            const uint32_t choice = (e & f) ^ (~e & g);
            const uint32_t first = h + sum1 + choice + constants[index] + word;
            const uint32_t sum0 =
                rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
            const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t second = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + first;
            d = c;
            c = b;
            b = a;
            a = first + second;
        }
        this->state[0] += a;
        this->state[1] += b;
        this->state[2] += c;
        this->state[3] += d;
        this->state[4] += e;
        this->state[5] += f;
        this->state[6] += g;
        this->state[7] += h;
    }

    std::array<uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                     0xa54ff53a, 0x510e527f, 0x9b05688c,
                                     0x1f83d9ab, 0x5be0cd19};
    std::array<uint8_t, 64> buffer{};
    uint64_t totalBytes = 0;
    std::size_t bufferSize = 0;
};

#endif  // COMMON_SHA256_HPP
