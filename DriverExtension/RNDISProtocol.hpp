#ifndef RNDISPROTOCOL_HPP
#define RNDISPROTOCOL_HPP

#include <DriverKit/DriverKit.h>
#include <DriverKit/OSObject.h>
#include <DriverKit/OSData.h>

class USBTransport;

struct RNDISEthernetAddress {
  uint8_t bytes[6];
};

class RNDISProtocol : public OSObject {
  OSDeclareDefaultStructors(RNDISProtocol);

public:
  struct State {
    bool ready;
    uint32_t maxTransferSize;
    RNDISEthernetAddress macAddress;
  };

  struct Callbacks {
    OSObject *target;
    void (*onLinkUp)(OSObject *target, const State &state);
    void (*onLinkDown)(OSObject *target);
    void (*onPacketReceived)(OSObject *target, const uint8_t *data, uint32_t length);
  };

  bool Init(USBTransport *transport, const Callbacks &callbacks);
  void TearDown();

  void BeginInitialize();
  void HandleControlResponse(const void *buffer, uint32_t length);
  void HandleDataMessage(const void *buffer, uint32_t length);

  bool SendEthernetFrame(const uint8_t *data, uint32_t length);

private:
  enum class Phase {
    kIdle,
    kInitialize,
    kQueryMac,
    kQueryMTU,
    kSetPacketFilter,
    kReady,
    kError
  };

  USBTransport *transport {nullptr};
  Callbacks callbacks {};
  Phase phase {Phase::kIdle};
  uint32_t requestId {1};
  State state {};

  void SendInitialize();
  void SendQuery(uint32_t oid);
  void SendSet(uint32_t oid, const void *payload, uint32_t length);
  void SendKeepAlive();
  void AdvancePhase();

  void HandleInitializeComplete(const void *buffer, uint32_t length);
  void HandleQueryComplete(uint32_t oid, const void *buffer, uint32_t length);
  void HandleSetComplete(const void *buffer, uint32_t length);
  void HandleKeepAliveComplete(const void *buffer, uint32_t length);
};

#endif // RNDISPROTOCOL_HPP
