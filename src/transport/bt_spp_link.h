#pragma once

#include <Arduino.h>
#include <BluetoothSerial.h>

#include "kiss_hub.h"

namespace aprs2mesh {

// KISS over Bluetooth Classic SPP. One peer at a time, as SPP provides.
class BtSppLink : public KissSink {
 public:
  bool begin(const char* deviceName);
  void poll(KissHub& hub, Ax25Handler handler, void* context);
  bool connected() const { return registered_; }

  size_t write(const uint8_t* data, size_t length) override;

 private:
  BluetoothSerial serial_;
  bool registered_ = false;
  bool started_ = false;
};

}  // namespace aprs2mesh
