#include "bt_spp_link.h"

namespace aprs2mesh {

bool BtSppLink::begin(const char* deviceName) {
  started_ = serial_.begin(deviceName);
  return started_;
}

size_t BtSppLink::write(const uint8_t* data, size_t length) {
  if (!serial_.hasClient()) {
    return 0;
  }
  return serial_.write(data, length);
}

void BtSppLink::poll(KissHub& hub, Ax25Handler handler, void* context) {
  if (!started_) {
    return;
  }

  if (serial_.hasClient()) {
    if (!registered_) {
      registered_ = hub.addClient(this);
    }
  } else if (registered_) {
    hub.removeClient(this);
    registered_ = false;
  }

  if (!registered_) {
    return;
  }

  while (serial_.available() > 0) {
    const int value = serial_.read();
    if (value < 0) {
      break;
    }
    if (hub.feed(this, static_cast<uint8_t>(value))) {
      handler(context, hub.frame(), hub.frameLen());
    }
  }
}

}  // namespace aprs2mesh
