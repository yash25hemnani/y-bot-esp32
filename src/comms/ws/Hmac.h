#pragma once
#include <Arduino.h>
#include "mbedtls/md.h"

// hex(HMAC-SHA256(key, msg)). Must match the server's sign() exactly.
// The key is the hmac_secret string as stored (64 hex characters), NOT decoded to 32 bytes.
inline String hmacSha256Hex(const String &key, const String &msg)
{
    uint8_t out[32];
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    if (mbedtls_md_hmac(info,
                        (const uint8_t *)key.c_str(), key.length(),
                        (const uint8_t *)msg.c_str(), msg.length(),
                        out) != 0)
        return ""; // caller treats empty as "can't log in"

    static const char hex[] = "0123456789abcdef";
    String result;
    result.reserve(64);
    for (uint8_t b : out)
    {
        result += hex[b >> 4];   // high half of the byte
        result += hex[b & 0x0f]; // low half
    }
    return result;
}
