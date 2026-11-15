#include "aprsis_session.h"

#include <cstdio>
#include <cstring>

namespace aprs2mesh {

namespace {

bool containsToken(const char* line, size_t length, const char* token) {
  const size_t tokenLength = std::strlen(token);
  if (tokenLength == 0) {
    return false;
  }
  for (size_t i = 0; i + tokenLength <= length; ++i) {
    if (std::memcmp(line + i, token, tokenLength) != 0) {
      continue;
    }
    const bool startOk =
        i == 0 || line[i - 1] == ' ' || line[i - 1] == ',' || line[i - 1] == '(';
    const size_t end = i + tokenLength;
    const bool endOk = end == length || line[end] == ' ' || line[end] == ',' ||
                       line[end] == ')' || line[end] == '\r';
    if (startOk && endOk) {
      return true;
    }
  }
  return false;
}

}  // namespace

const char* sessionStateText(SessionState state) {
  switch (state) {
    case SessionState::Disabled:
      return "deshabilitada";
    case SessionState::Disconnected:
      return "desconectada";
    case SessionState::AwaitingLogin:
      return "esperando login";
    case SessionState::Verified:
      return "verificada";
    case SessionState::Unverified:
      return "no verificada";
  }
  return "?";
}

void AprsIsSession::configure(const AprsIsSettings& settings, uint32_t) {
  settings_ = settings;
  state_ = SessionState::Disabled;
  enabled_ = false;
  loginPending_ = false;
  closeRequested_ = false;
  attempts_ = 0;
  retryDelay_ = 0;
  retryAt_ = 0;
  lastError_ = "";
}

void AprsIsSession::start(uint32_t nowMs) {
  enabled_ = true;
  state_ = SessionState::Disconnected;
  loginPending_ = false;
  closeRequested_ = false;
  attempts_ = 0;
  retryDelay_ = 0;
  retryAt_ = nowMs;
  lastError_ = "";
}

void AprsIsSession::stop() {
  enabled_ = false;
  state_ = SessionState::Disabled;
  loginPending_ = false;
  closeRequested_ = false;
}

uint32_t AprsIsSession::retryDelayFor(uint8_t attempt) {
  if (attempt == 0) {
    return 0;
  }
  uint32_t delay = kRetryBaseMs;
  for (uint8_t i = 1; i < attempt; ++i) {
    if (delay >= kRetryMaxMs) {
      return kRetryMaxMs;
    }
    delay *= 2;
  }
  return delay > kRetryMaxMs ? kRetryMaxMs : delay;
}

bool AprsIsSession::connectDue(uint32_t nowMs) const {
  return enabled_ && state_ == SessionState::Disconnected &&
         static_cast<int32_t>(nowMs - retryAt_) >= 0;
}

void AprsIsSession::onConnected(uint32_t) {
  state_ = SessionState::AwaitingLogin;
  loginPending_ = true;
  loginSentAt_ = 0;
  closeRequested_ = false;
}

size_t AprsIsSession::buildLogin(char* out, size_t capacity) const {
  if (out == nullptr || capacity == 0 || settings_.callsign[0] == '\0') {
    return 0;
  }

  char line[256] = {};
  int written = std::snprintf(line, sizeof(line), "user %s pass %u vers %s",
                              settings_.callsign,
                              static_cast<unsigned>(settings_.passcode),
                              settings_.softwareId);
  if (written < 0 || static_cast<size_t>(written) >= sizeof(line)) {
    return 0;
  }

  if (settings_.filter[0] != '\0') {
    const int appended = std::snprintf(line + written, sizeof(line) - static_cast<size_t>(written),
                                       " filter %s", settings_.filter);
    if (appended < 0 ||
        static_cast<size_t>(appended) >= sizeof(line) - static_cast<size_t>(written)) {
      return 0;
    }
    written += appended;
  }

  if (static_cast<size_t>(written) + 3 > sizeof(line)) {
    return 0;
  }
  line[written++] = '\r';
  line[written++] = '\n';
  line[written] = '\0';

  const size_t length = static_cast<size_t>(written);
  if (length + 1 > capacity) {
    return 0;
  }
  std::memcpy(out, line, length + 1);
  return length;
}

void AprsIsSession::onLoginSent(uint32_t nowMs) {
  loginPending_ = false;
  loginSentAt_ = nowMs;
  lastKeepaliveAt_ = nowMs;
}

void AprsIsSession::onTransportDown(uint32_t nowMs, const char* reason) {
  state_ = SessionState::Disconnected;
  loginPending_ = false;
  if (attempts_ < 255) {
    ++attempts_;
  }
  retryDelay_ = retryDelayFor(attempts_);
  retryAt_ = nowMs + retryDelay_;
  if (reason != nullptr) {
    lastError_ = reason;
  }
}

void AprsIsSession::tick(uint32_t nowMs) {
  if (state_ == SessionState::AwaitingLogin && !loginPending_ &&
      static_cast<uint32_t>(nowMs - loginSentAt_) > kLoginTimeoutMs) {
    onTransportDown(nowMs, "timeout de login");
    closeRequested_ = true;
  }
}

void AprsIsSession::onLine(const char* line, size_t length, uint32_t nowMs) {
  lastLineAt_ = nowMs;
  if (line == nullptr || length == 0 || line[0] != '#') {
    return;
  }
  if (!containsToken(line, length, "logresp")) {
    return;
  }
  if (containsToken(line, length, "unverified")) {
    state_ = SessionState::Unverified;
    lastError_ = "login no verificado";
    return;
  }
  if (containsToken(line, length, "verified")) {
    state_ = SessionState::Verified;
    attempts_ = 0;
    retryDelay_ = 0;
    lastError_ = "";
  }
}

bool AprsIsSession::keepaliveDue(uint32_t nowMs) const {
  if (state_ != SessionState::Verified && state_ != SessionState::Unverified) {
    return false;
  }
  return static_cast<uint32_t>(nowMs - lastKeepaliveAt_) >= kKeepaliveMs;
}

size_t AprsIsSession::buildKeepalive(uint32_t nowMs, char* out, size_t capacity) {
  constexpr char kLine[] = "# aprs2mesh\r\n";
  constexpr size_t kLength = sizeof(kLine) - 1;
  if (out == nullptr || capacity < kLength + 1) {
    return 0;
  }
  std::memcpy(out, kLine, kLength + 1);
  lastKeepaliveAt_ = nowMs;
  return kLength;
}

}  // namespace aprs2mesh
