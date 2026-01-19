#ifndef MACRNDISDRIVER_HPP
#define MACRNDISDRIVER_HPP

#include <DriverKit/DriverKit.h>
#include <DriverKit/IOService.h>
#include <DriverKit/IOUSBHostInterface.h>

#include "USBTransport.hpp"
#include "RNDISProtocol.hpp"
#include "MacRNDISNetworkInterface.hpp"

class MacRNDISDriver : public IOService {
  OSDeclareDefaultStructors(MacRNDISDriver);

public:
  bool Start(IOService *provider) override;
  void Stop(IOService *provider) override;

private:
  IOUSBHostInterface *usbInterface { nullptr };
  USBTransport *transport { nullptr };
  RNDISProtocol *protocol { nullptr };
  MacRNDISNetworkInterface *networkInterface { nullptr };

  bool MatchRNDISInterface(IOUSBHostInterface *interface);
  void ReleaseResources();

  // USBTransport callbacks
  static void OnControlResponse(OSObject *target, const void *buffer, uint32_t length);
  static void OnDataReceived(OSObject *target, const void *buffer, uint32_t length);
  static void OnDeviceError(OSObject *target);

  // RNDISProtocol callbacks
  static void OnLinkUp(OSObject *target, const RNDISProtocol::State &state);
  static void OnLinkDown(OSObject *target);
  static void OnPacketReceived(OSObject *target, const uint8_t *data, uint32_t length);
};

#endif // MACRNDISDRIVER_HPP
