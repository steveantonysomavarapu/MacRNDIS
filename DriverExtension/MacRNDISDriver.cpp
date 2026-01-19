#include "MacRNDISDriver.hpp"
#include <DriverKit/OSLog.h>

#define kRNDISInterfaceClass     0xE0
#define kRNDISInterfaceSubClass  0x01
#define kRNDISInterfaceProtocol  0x03

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
    return false;
  }

  transport = OSObject::Create<USBTransport>();
  if (transport == nullptr) {
    return false;
  }

  USBTransport::Callbacks transportCallbacks {};
  transportCallbacks.target = this;
  transportCallbacks.onControlResponse = OnControlResponse;
  transportCallbacks.onDataReceived = OnDataReceived;
  transportCallbacks.onDeviceError = OnDeviceError;

  if (!transport->Init(usbInterface, transportCallbacks) ||
      !transport->ConfigurePipes()) {
    OSLog("MacRNDISDriver: failed to configure USB transport\n");
    ReleaseResources();
    return false;
  }

  protocol = OSObject::Create<RNDISProtocol>();
  if (protocol == nullptr) {
    ReleaseResources();
    return false;
  }

  RNDISProtocol::Callbacks protocolCallbacks {};
  protocolCallbacks.target = this;
  protocolCallbacks.onLinkUp = OnLinkUp;
  protocolCallbacks.onLinkDown = OnLinkDown;
  protocolCallbacks.onPacketReceived = OnPacketReceived;

  if (!protocol->Init(transport, protocolCallbacks)) {
    ReleaseResources();
    return false;
  }

  networkInterface = OSObject::Create<MacRNDISNetworkInterface>();
  if (networkInterface == nullptr || !networkInterface->Init(protocol)) {
    ReleaseResources();
    return false;
  }

  if (!networkInterface->Attach(this) ||
      !networkInterface->Start(this)) {
    ReleaseResources();
    return false;
  }

  transport->StartAsyncReads();
  protocol->BeginInitialize();

  return true;
}

void MacRNDISDriver::Stop(IOService *provider) {
  OSLog("MacRNDISDriver: Stop\n");
  ReleaseResources();
  IOService::Stop(provider);
}

bool MacRNDISDriver::MatchRNDISInterface(IOUSBHostInterface *interface) {
  if (interface == nullptr) {
    return false;
  }

  const IOUSBHostInterfaceDescriptor *descriptor =
    interface->GetInterfaceDescriptor();

  if (descriptor == nullptr) {
    OSLog("MacRNDISDriver: missing interface descriptor\n");
    return false;
  }

  return descriptor->bInterfaceClass    == kRNDISInterfaceClass &&
         descriptor->bInterfaceSubClass == kRNDISInterfaceSubClass &&
         descriptor->bInterfaceProtocol == kRNDISInterfaceProtocol;
}

void MacRNDISDriver::ReleaseResources() {
  if (networkInterface != nullptr) {
    networkInterface->Stop(this);
    networkInterface->Detach(this);
    networkInterface->release();
    networkInterface = nullptr;
  }

  if (protocol != nullptr) {
    protocol->TearDown();
    protocol->release();
    protocol = nullptr;
  }

  if (transport != nullptr) {
    transport->TearDown();
    transport->release();
    transport = nullptr;
  }

  usbInterface = nullptr;
}

// ================= Callbacks =================

void MacRNDISDriver::OnControlResponse(OSObject *target,
                                      const void *buffer,
                                      uint32_t length) {
  auto *driver = OSDynamicCast(MacRNDISDriver, target);
  if (driver && driver->protocol) {
    driver->protocol->HandleControlResponse(buffer, length);
  }
}

void MacRNDISDriver::OnDataReceived(OSObject *target,
                                    const void *buffer,
                                    uint32_t length) {
  auto *driver = OSDynamicCast(MacRNDISDriver, target);
  if (driver && driver->protocol) {
    driver->protocol->HandleDataMessage(buffer, length);
  }
}

void MacRNDISDriver::OnDeviceError(OSObject *target) {
  auto *driver = OSDynamicCast(MacRNDISDriver, target);
  if (driver && driver->networkInterface) {
    driver->networkInterface->UpdateLinkState(
      false, 0, RNDISEthernetAddress {}
    );
  }
}

void MacRNDISDriver::OnLinkUp(OSObject *target,
                              const RNDISProtocol::State &state) {
  auto *driver = OSDynamicCast(MacRNDISDriver, target);
  if (driver && driver->networkInterface) {
    driver->networkInterface->UpdateLinkState(
      true, state.maxTransferSize, state.macAddress
    );
  }
}

void MacRNDISDriver::OnLinkDown(OSObject *target) {
  auto *driver = OSDynamicCast(MacRNDISDriver, target);
  if (driver && driver->networkInterface) {
    driver->networkInterface->UpdateLinkState(
      false, 0, RNDISEthernetAddress {}
    );
  }
}

void MacRNDISDriver::OnPacketReceived(OSObject *target,
                                      const uint8_t *data,
                                      uint32_t length) {
  auto *driver = OSDynamicCast(MacRNDISDriver, target);
  if (driver && driver->networkInterface) {
    driver->networkInterface->SubmitInboundPacket(data, length);
  }
}
