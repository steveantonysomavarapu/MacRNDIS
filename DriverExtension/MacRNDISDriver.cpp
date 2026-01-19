#include "MacRNDISDriver.hpp"

#include <DriverKit/OSLog.h>

#define kRNDISInterfaceClass 0xE0
#define kRNDISInterfaceSubClass 0x01
#define kRNDISInterfaceProtocol 0x03

OSDefineMetaClassAndStructors(MacRNDISDriver, IOService);

bool MacRNDISDriver::Start(IOService *provider) {
  if (!IOService::Start(provider)) {
    return false;
  }

  usbInterface = OSDynamicCast(IOUSBHostInterface, provider);
  if (usbInterface == nullptr) {
    OSLog("MacRNDISDriver: provider is not IOUSBHostInterface\n");
    return false;
  }

  if (!MatchRNDISInterface(usbInterface)) {
    OSLog("MacRNDISDriver: interface does not match RNDIS class/subclass/protocol\n");
    ReleaseInterface();
    return false;
  }

  OSLog("MacRNDISDriver: matched RNDIS interface, TODO: enumerate endpoints and configure pipes\n");
  // TODO: Enumerate endpoints and configure control/bulk pipes.
  // TODO: Implement RNDIS control channel (OID set/query/keep-alive).
  // TODO: Create and register IOUserNetworkEthernet interface.

  return true;
}

void MacRNDISDriver::Stop(IOService *provider) {
  OSLog("MacRNDISDriver: Stop\n");
  ReleaseInterface();
  IOService::Stop(provider);
}

bool MacRNDISDriver::MatchRNDISInterface(IOUSBHostInterface *interface) {
  if (interface == nullptr) {
    return false;
  }

  const IOUSBHostInterfaceDescriptor *descriptor = interface->GetInterfaceDescriptor();
  if (descriptor == nullptr) {
    OSLog("MacRNDISDriver: missing interface descriptor\n");
    return false;
  }

  return descriptor->bInterfaceClass == kRNDISInterfaceClass &&
    descriptor->bInterfaceSubClass == kRNDISInterfaceSubClass &&
    descriptor->bInterfaceProtocol == kRNDISInterfaceProtocol;
}

void MacRNDISDriver::ReleaseInterface() {
  usbInterface = nullptr;
}
