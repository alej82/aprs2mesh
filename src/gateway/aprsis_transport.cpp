#include "aprsis_transport.h"

#include <cstring>

namespace aprs2mesh {

void AprsIsTransport::configure(const char* host, uint16_t port) {
  std::memset(host_, 0, sizeof(host_));
  if (host != nullptr) {
    std::strncpy(host_, host, sizeof(host_) - 1);
  }
  port_ = port;
}

void AprsIsTransport::close() {
  client_.stop();
  client_ = WiFiClient();
  socketOpen_ = false;
  lineLength_ = 0;
  discarding_ = false;
}

bool AprsIsTransport::send(const char* data, size_t length) {
  if (!socketOpen_ || !client_.connected() || data == nullptr) {
    return false;
  }
  const size_t written =
      client_.write(reinterpret_cast<const uint8_t*>(data), length);
  return written == length;
}

void AprsIsTransport::flushLine(AprsIsSession& session, uint32_t nowMs,
                                AprsIsLineHandler handler, void* context) {
  if (lineLength_ == 0) {
    return;
  }
  line_[lineLength_] = '\0';
  session.onLine(line_, lineLength_, nowMs);
  if (line_[0] != '#' && handler != nullptr) {
    handler(context, line_, lineLength_);
  }
  lineLength_ = 0;
}

void AprsIsTransport::handleByte(char value, AprsIsSession& session,
                                 uint32_t nowMs, AprsIsLineHandler handler,
                                 void* context) {
  if (value == '\n') {
    if (discarding_) {
      discarding_ = false;
      lineLength_ = 0;
      return;
    }
    flushLine(session, nowMs, handler, context);
    return;
  }
  if (value == '\r') {
    return;
  }
  if (lineLength_ + 1 >= sizeof(line_)) {
    // Overlong line: consume it silently until the next newline.
    discarding_ = true;
    lineLength_ = 0;
    return;
  }
  line_[lineLength_++] = value;
}

void AprsIsTransport::poll(AprsIsSession& session, uint32_t nowMs,
                           AprsIsLineHandler handler, void* context) {
  session.tick(nowMs);

  if (session.closeRequested()) {
    close();
    session.clearCloseRequest();
  }

  if (socketOpen_ && !client_.connected()) {
    const SessionState state = session.state();
    close();
    if (state == SessionState::AwaitingLogin || state == SessionState::Verified ||
        state == SessionState::Unverified) {
      session.onTransportDown(nowMs, "conexión cerrada");
    }
  }

  if (!socketOpen_ && session.connectDue(nowMs)) {
    if (!client_.connect(host_, port_, 2000)) {
      client_.stop();
      session.onTransportDown(nowMs, "sin conexión APRS-IS");
      return;
    }
    socketOpen_ = true;
    lineLength_ = 0;
    discarding_ = false;
    session.onConnected(nowMs);
  }

  if (!socketOpen_) {
    return;
  }

  if (session.loginPending()) {
    char login[256] = {};
    const size_t length = session.buildLogin(login, sizeof(login));
    if (length == 0 || !send(login, length)) {
      close();
      session.onTransportDown(nowMs, "login no enviado");
      return;
    }
    session.onLoginSent(nowMs);
  }

  if (session.keepaliveDue(nowMs)) {
    char keepalive[32] = {};
    const size_t length = session.buildKeepalive(nowMs, keepalive, sizeof(keepalive));
    if (length > 0 && !send(keepalive, length)) {
      close();
      session.onTransportDown(nowMs, "keepalive fallido");
      return;
    }
  }

  while (client_.available() > 0) {
    const int value = client_.read();
    if (value < 0) {
      break;
    }
    handleByte(static_cast<char>(value), session, nowMs, handler, context);
  }
}

}  // namespace aprs2mesh
