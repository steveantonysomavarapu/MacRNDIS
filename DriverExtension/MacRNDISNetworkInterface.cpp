#include "MacRNDISNetworkInterface.hpp"

#include <DriverKit/OSLog.h>

OSDefineMetaClassAndStructors(MacRNDISNetworkInterface, IOUserNetworkEthernet);

bool MacRNDISNetworkInterface::Init(RNDISProtocol *protocolRef) {
  if (!IOUserNetworkEthernet::Init()) {
    return false;
  }
  protocol = protocolRef;
  return true;
}

bool MacRNDISNetworkInterface::Start(IOService *provider) {
  if (!IOUserNetworkEthernet::Start(provider)) {
    return false;
  }

  SetLinkStatus(kIOUserNetworkLinkStateInactive, 0, nullptr);
  return true;
}

void MacRNDISNetworkInterface::Stop(IOService *provider) {
  SetLinkStatus(kIOUserNetworkLinkStateInactive, 0, nullptr);
  IOUserNetworkEthernet::Stop(provider);
}

IOReturn MacRNDISNetworkInterface::GetHardwareAddress(IOEthernetAddress *addr) {
  if (addr == nullptr) {
    return kIOReturnBadArgument;
  }

  *addr = currentAddress;
  return kIOReturnSuccess;
}

IOReturn MacRNDISNetworkInterface::SetMTU(uint32_t mtu) {
  currentMTU = mtu;
  return kIOReturnSuccess;
}

uint32_t MacRNDISNetworkInterface::GetMaxPacketSize() const {
  return currentMTU + sizeof(IOEthernetAddress) * 2;
}

IOReturn MacRNDISNetworkInterface::OutputPacket(const void *data, uint32_t length) {
  if (protocol == nullptr) {
    return kIOReturnNotReady;
  }
  if (!protocol->SendEthernetFrame(reinterpret_cast<const uint8_t *>(data), length)) {
    return kIOReturnIOError;
  }
  return kIOReturnSuccess;
}

void MacRNDISNetworkInterface::UpdateLinkState(bool isUp, uint32_t mtu, const RNDISEthernetAddress &mac) {
  linkUp = isUp;
  currentMTU = mtu;
  memcpy(currentAddress.bytes, mac.bytes, sizeof(mac.bytes));

  if (linkUp) {
    SetLinkStatus(kIOUserNetworkLinkStateActive, currentMTU, nullptr);
  } else {
    SetLinkStatus(kIOUserNetworkLinkStateInactive, 0, nullptr);
  }
}

void MacRNDISNetworkInterface::SubmitInboundPacket(const uint8_t *data, uint32_t length) {
  if (!linkUp || data == nullptr || length == 0) {
    return;
  }

  InputPacket(data, length, 0);
}
