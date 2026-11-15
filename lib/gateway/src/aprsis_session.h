#pragma once

#include <cstddef>
#include <cstdint>

#include "gateway_config.h"

namespace aprs2mesh {

inline constexpr uint32_t kKeepaliveMs = 20000;
inline constexpr uint32_t kLoginTimeoutMs = 15000;
inline constexpr uint32_t kRetryBaseMs = 5000;
inline constexpr uint32_t kRetryMaxMs = 300000;

enum class SessionState : uint8_t {
  Disabled = 0,
  Disconnected,
  AwaitingLogin,
  Verified,
  Unverified,
};

// Portal-ready description: "desconectada", "verificada", etc.
const char* sessionStateText(SessionState state);

struct AprsIsSettings {
  char callsign[kGwCallsign] = {};
  uint16_t passcode = 0;
  char filter[kGwFilter] = {};
  char softwareId[8] = "APZ2M";
};

// Protocol state of the APRS-IS connection. The socket itself lives in the
// Arduino transport; this class only decides when to connect, what to send and
// whether injection is allowed, which keeps it testable on the host.
class AprsIsSession {
 public:
  void configure(const AprsIsSettings& settings, uint32_t nowMs);
  void start(uint32_t nowMs);
  void stop();

  // Login timeout and other portable deadlines.
  void tick(uint32_t nowMs);

  bool connectDue(uint32_t nowMs) const;
  void onConnected(uint32_t nowMs);
  bool loginPending() const { return loginPending_; }
  size_t buildLogin(char* out, size_t capacity) const;
  void onLoginSent(uint32_t nowMs);
  void onTransportDown(uint32_t nowMs, const char* reason);
  void onLine(const char* line, size_t length, uint32_t nowMs);

  bool canInject() const { return state_ == SessionState::Verified; }
  bool keepaliveDue(uint32_t nowMs) const;
  size_t buildKeepalive(uint32_t nowMs, char* out, size_t capacity);

  // Set when the transport must close a socket that never logged in.
  bool closeRequested() const { return closeRequested_; }
  void clearCloseRequest() { closeRequested_ = false; }

  SessionState state() const { return state_; }
  const char* lastError() const { return lastError_; }
  uint8_t failedAttempts() const { return attempts_; }
  uint32_t retryDelayMs() const { return retryDelay_; }
  uint32_t nextAttemptAt() const { return retryAt_; }
  uint32_t lastLineAt() const { return lastLineAt_; }
  static uint32_t retryDelayFor(uint8_t attempt);

 private:
  AprsIsSettings settings_;
  SessionState state_ = SessionState::Disabled;
  bool enabled_ = false;
  bool loginPending_ = false;
  bool closeRequested_ = false;
  uint8_t attempts_ = 0;
  uint32_t retryDelay_ = 0;
  uint32_t retryAt_ = 0;
  uint32_t loginSentAt_ = 0;
  uint32_t lastKeepaliveAt_ = 0;
  uint32_t lastLineAt_ = 0;
  const char* lastError_ = "";
};

}  // namespace aprs2mesh
