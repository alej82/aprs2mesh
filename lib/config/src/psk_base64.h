#pragma once

#include <cstddef>
#include <cstdint>

namespace aprs2mesh {

// Standard base64 with padding, as the Meshtastic app shows channel keys.
// Returns the number of characters written (without the terminator), or 0 when
// the buffer is too small.
size_t encodeBase64(const uint8_t* data, size_t length, char* out, size_t capacity);

// Decodes `text` into `out`. Rejects a length that is not a multiple of four,
// characters outside the alphabet and padding in the wrong place. An empty
// input decodes to zero bytes and succeeds (open channel).
bool decodeBase64(const char* text, uint8_t* out, size_t capacity, size_t& length);

}  // namespace aprs2mesh
