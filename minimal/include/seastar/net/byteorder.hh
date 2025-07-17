#pragma once

#include <arpa/inet.h>
#include <cstdint>

namespace seastar {

inline uint64_t ntohq(uint64_t v) {
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    // big endian, nothing to do
    return v;
#else
    // little endian, reverse bytes
    return __builtin_bswap64(v);
#endif
}

inline uint64_t htonq(uint64_t v) {
    // htonq and ntohq have identical implementations
    return ntohq(v);
}

namespace net {

inline void ntoh() {}
inline void hton() {}

inline uint8_t ntoh(uint8_t x) { return x; }
inline uint8_t hton(uint8_t x) { return x; }
inline uint16_t ntoh(uint16_t x) { return ntohs(x); }
inline uint16_t hton(uint16_t x) { return htons(x); }
inline uint32_t ntoh(uint32_t x) { return ntohl(x); }
inline uint32_t hton(uint32_t x) { return htonl(x); }
inline uint64_t ntoh(uint64_t x) { return ntohq(x); }
inline uint64_t hton(uint64_t x) { return htonq(x); }

inline int8_t ntoh(int8_t x) { return x; }
inline int8_t hton(int8_t x) { return x; }
inline int16_t ntoh(int16_t x) { return ntohs(x); }
inline int16_t hton(int16_t x) { return htons(x); }
inline int32_t ntoh(int32_t x) { return ntohl(x); }
inline int32_t hton(int32_t x) { return htonl(x); }
inline int64_t ntoh(int64_t x) { return ntohq(x); }
inline int64_t hton(int64_t x) { return htonq(x); }

}

} 