#ifndef MACRNDISDRIVER_HPP
#define MACRNDISDRIVER_HPP

#include <DriverKit/DriverKit.h>
#include <DriverKit/IOService.h>
#include <DriverKit/IOUSBHostDevice.h>
#include <DriverKit/IOUSBHostInterface.h>

class MacRNDISDriver : public IOService {
  OSDeclareDefaultStructors(MacRNDISDriver);

public:
  bool Start(IOService *provider) override;
  void Stop(IOService *provider) override;

private:
  IOUSBHostInterface *usbInterface {nullptr};

  bool MatchRNDISInterface(IOUSBHostInterface *interface);
  void ReleaseInterface();
};

#endif // MACRNDISDRIVER_HPP
