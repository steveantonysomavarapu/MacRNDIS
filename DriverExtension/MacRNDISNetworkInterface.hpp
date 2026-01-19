#ifndef MACRNDISNETWORKINTERFACE_HPP
#define MACRNDISNETWORKINTERFACE_HPP

#include <DriverKit/DriverKit.h>
#include <DriverKit/IOUserNetworkEthernet.h>

#include "RNDISProtocol.hpp"

class MacRNDISNetworkInterface : public IOUserNetworkEthernet {
  OSDeclareDefaultStructors(MacRNDISNetworkInterface);

public:
  bool Init(RNDISProtocol *protocolRef);

  bool Start(IOService *provider) override;
  void Stop(IOService *provider) override;

  IOReturn GetHardwareAddress(IOEthernetAddress *addr) override;
  IOReturn SetMTU(uint32_t mtu) override;
  uint32_t GetMaxPacketSize() const override;

  IOReturn OutputPacket(const void *data, uint32_t length) override;

  void UpdateLinkState(bool linkUp, uint32_t mtu, const RNDISEthernetAddress &mac);
  void SubmitInboundPacket(const uint8_t *data, uint32_t length);

private:
  RNDISProtocol *protocol {nullptr};
  bool linkUp {false};
  uint32_t currentMTU {1500};
  IOEthernetAddress currentAddress {};
};

#endif // MACRNDISNETWORKINTERFACE_HPP
