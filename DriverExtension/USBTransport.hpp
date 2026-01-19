#ifndef USBTRANSPORT_HPP
#define USBTRANSPORT_HPP

#include <DriverKit/DriverKit.h>
#include <DriverKit/OSObject.h>
#include <DriverKit/IOUSBHostInterface.h>
#include <DriverKit/IOUSBHostPipe.h>

class RNDISProtocol;

class USBTransport : public OSObject {
  OSDeclareDefaultStructors(USBTransport);

public:
  struct Callbacks {
    OSObject *target;
    void (*onControlResponse)(OSObject *target, const void *buffer, uint32_t length);
    void (*onDataReceived)(OSObject *target, const void *buffer, uint32_t length);
    void (*onDeviceError)(OSObject *target);
  };

  bool Init(IOUSBHostInterface *interfaceRef, const Callbacks &callbacksRef);
  void TearDown();

  bool ConfigurePipes();
  void StartAsyncReads();

  bool SendControlMessage(const void *buffer, uint32_t length);
  bool SendBulkOut(const void *buffer, uint32_t length);

private:
  IOUSBHostInterface *usbInterface {nullptr};
  IOUSBHostPipe *controlPipe {nullptr};
  IOUSBHostPipe *bulkInPipe {nullptr};
  IOUSBHostPipe *bulkOutPipe {nullptr};
  Callbacks callbacks {};

  void HandleControlComplete(IOReturn status, uint32_t bytesTransferred);
  void HandleBulkInComplete(IOReturn status, uint32_t bytesTransferred);
  void HandleBulkOutComplete(IOReturn status, uint32_t bytesTransferred);

  static void ControlCompletion(void *context, IOReturn status, uint32_t bytesTransferred);
  static void BulkInCompletion(void *context, IOReturn status, uint32_t bytesTransferred);
  static void BulkOutCompletion(void *context, IOReturn status, uint32_t bytesTransferred);

  uint8_t controlBuffer[4096] {};
  uint8_t bulkInBuffer[4096] {};
};

#endif // USBTRANSPORT_HPP
