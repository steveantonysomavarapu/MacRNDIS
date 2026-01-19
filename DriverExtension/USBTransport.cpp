#include "USBTransport.hpp"

#include <DriverKit/OSLog.h>
#include <DriverKit/OSArray.h>
#include <DriverKit/OSData.h>

OSDefineMetaClassAndStructors(USBTransport, OSObject);

bool USBTransport::Init(IOUSBHostInterface *interfaceRef, const Callbacks &callbacksRef) {
  if (interfaceRef == nullptr) {
    return false;
  }

  usbInterface = interfaceRef;
  callbacks = callbacksRef;
  return true;
}

void USBTransport::TearDown() {
  controlPipe = nullptr;
  bulkInPipe = nullptr;
  bulkOutPipe = nullptr;
  usbInterface = nullptr;
}

bool USBTransport::ConfigurePipes() {
  if (usbInterface == nullptr) {
    return false;
  }

  OSArray *endpoints = usbInterface->CopyEndpointDescriptors();
  if (endpoints == nullptr) {
    OSLog("USBTransport: no endpoint descriptors\n");
    return false;
  }

  const uint32_t count = endpoints->getCount();
  for (uint32_t index = 0; index < count; ++index) {
    auto *descriptorData = OSDynamicCast(OSData, endpoints->getObject(index));
    if (descriptorData == nullptr) {
      continue;
    }

    const auto *descriptor = reinterpret_cast<const IOUSBHostEndpointDescriptor *>(descriptorData->getBytesNoCopy());
    if (descriptor == nullptr) {
      continue;
    }

    if (descriptor->bmAttributes == kIOUSBHostPipeTypeBulk) {
      if ((descriptor->bEndpointAddress & kIOUSBEndpointDirectionMask) == kIOUSBEndpointDirectionIn) {
        bulkInPipe = usbInterface->CopyPipe(descriptor);
      } else {
        bulkOutPipe = usbInterface->CopyPipe(descriptor);
      }
    }
  }

  endpoints->release();

  controlPipe = usbInterface->CopyPipe(0);

  if (controlPipe == nullptr || bulkInPipe == nullptr || bulkOutPipe == nullptr) {
    OSLog("USBTransport: missing pipes\n");
    return false;
  }

  return true;
}

void USBTransport::StartAsyncReads() {
  if (bulkInPipe == nullptr) {
    return;
  }

  bulkInPipe->Read(bulkInBuffer, sizeof(bulkInBuffer), this, BulkInCompletion, 0);
}

bool USBTransport::SendControlMessage(const void *buffer, uint32_t length) {
  if (controlPipe == nullptr || buffer == nullptr || length == 0) {
    return false;
  }

  memcpy(controlBuffer, buffer, length);
  return controlPipe->Write(controlBuffer, length, this, ControlCompletion, 0) == kIOReturnSuccess;
}

bool USBTransport::SendBulkOut(const void *buffer, uint32_t length) {
  if (bulkOutPipe == nullptr || buffer == nullptr || length == 0) {
    return false;
  }

  return bulkOutPipe->Write(buffer, length, this, BulkOutCompletion, 0) == kIOReturnSuccess;
}

void USBTransport::HandleControlComplete(IOReturn status, uint32_t bytesTransferred) {
  if (status != kIOReturnSuccess) {
    if (callbacks.onDeviceError != nullptr) {
      callbacks.onDeviceError(callbacks.target);
    }
    return;
  }

  if (callbacks.onControlResponse != nullptr) {
    callbacks.onControlResponse(callbacks.target, controlBuffer, bytesTransferred);
  }
}

void USBTransport::HandleBulkInComplete(IOReturn status, uint32_t bytesTransferred) {
  if (status != kIOReturnSuccess) {
    if (callbacks.onDeviceError != nullptr) {
      callbacks.onDeviceError(callbacks.target);
    }
    return;
  }

  if (callbacks.onDataReceived != nullptr) {
    callbacks.onDataReceived(callbacks.target, bulkInBuffer, bytesTransferred);
  }

  StartAsyncReads();
}

void USBTransport::HandleBulkOutComplete(IOReturn status, uint32_t bytesTransferred) {
  if (status != kIOReturnSuccess) {
    if (callbacks.onDeviceError != nullptr) {
      callbacks.onDeviceError(callbacks.target);
    }
    return;
  }
}

void USBTransport::ControlCompletion(void *context, IOReturn status, uint32_t bytesTransferred) {
  auto *transport = static_cast<USBTransport *>(context);
  if (transport != nullptr) {
    transport->HandleControlComplete(status, bytesTransferred);
  }
}

void USBTransport::BulkInCompletion(void *context, IOReturn status, uint32_t bytesTransferred) {
  auto *transport = static_cast<USBTransport *>(context);
  if (transport != nullptr) {
    transport->HandleBulkInComplete(status, bytesTransferred);
  }
}

void USBTransport::BulkOutCompletion(void *context, IOReturn status, uint32_t bytesTransferred) {
  auto *transport = static_cast<USBTransport *>(context);
  if (transport != nullptr) {
    transport->HandleBulkOutComplete(status, bytesTransferred);
  }
}
