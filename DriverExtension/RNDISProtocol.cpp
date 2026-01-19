#include "RNDISProtocol.hpp"
#include "USBTransport.hpp"

#include <DriverKit/OSLog.h>

#define RNDIS_MSG_INIT 0x00000002
#define RNDIS_MSG_INIT_CMPLT 0x80000002
#define RNDIS_MSG_QUERY 0x00000004
#define RNDIS_MSG_QUERY_CMPLT 0x80000004
#define RNDIS_MSG_SET 0x00000005
#define RNDIS_MSG_SET_CMPLT 0x80000005
#define RNDIS_MSG_KEEPALIVE 0x00000008
#define RNDIS_MSG_KEEPALIVE_CMPLT 0x80000008
#define RNDIS_MSG_PACKET 0x00000001

#define RNDIS_STATUS_SUCCESS 0x00000000

#define OID_802_3_CURRENT_ADDRESS 0x01010102
#define OID_GEN_MAXIMUM_FRAME_SIZE 0x00010106
#define OID_GEN_MAXIMUM_TOTAL_SIZE 0x00010111
#define OID_GEN_CURRENT_PACKET_FILTER 0x0001010E

#define RNDIS_PACKET_FILTER_DIRECTED 0x00000001
#define RNDIS_PACKET_FILTER_BROADCAST 0x00000002
#define RNDIS_PACKET_FILTER_ALL_MULTICAST 0x00000004

struct __attribute__((packed)) RNDISMessageHeader {
  uint32_t messageType;
  uint32_t messageLength;
};

struct __attribute__((packed)) RNDISInitializeMessage {
  RNDISMessageHeader header;
  uint32_t requestId;
  uint32_t majorVersion;
  uint32_t minorVersion;
  uint32_t maxTransferSize;
};

struct __attribute__((packed)) RNDISInitializeComplete {
  RNDISMessageHeader header;
  uint32_t requestId;
  uint32_t status;
  uint32_t majorVersion;
  uint32_t minorVersion;
  uint32_t deviceFlags;
  uint32_t medium;
  uint32_t maxPacketsPerTransfer;
  uint32_t maxTransferSize;
  uint32_t packetAlignmentFactor;
  uint32_t reserved[2];
};

struct __attribute__((packed)) RNDISQueryMessage {
  RNDISMessageHeader header;
  uint32_t requestId;
  uint32_t oid;
  uint32_t infoBufferLength;
  uint32_t infoBufferOffset;
  uint32_t deviceVcHandle;
};

struct __attribute__((packed)) RNDISQueryComplete {
  RNDISMessageHeader header;
  uint32_t requestId;
  uint32_t status;
  uint32_t infoBufferLength;
  uint32_t infoBufferOffset;
};

struct __attribute__((packed)) RNDISSetMessage {
  RNDISMessageHeader header;
  uint32_t requestId;
  uint32_t oid;
  uint32_t infoBufferLength;
  uint32_t infoBufferOffset;
  uint32_t deviceVcHandle;
};

struct __attribute__((packed)) RNDISSetComplete {
  RNDISMessageHeader header;
  uint32_t requestId;
  uint32_t status;
};

struct __attribute__((packed)) RNDISKeepAliveMessage {
  RNDISMessageHeader header;
  uint32_t requestId;
};

struct __attribute__((packed)) RNDISKeepAliveComplete {
  RNDISMessageHeader header;
  uint32_t requestId;
  uint32_t status;
};

struct __attribute__((packed)) RNDISPacketMessage {
  RNDISMessageHeader header;
  uint32_t dataOffset;
  uint32_t dataLength;
  uint32_t oobDataOffset;
  uint32_t oobDataLength;
  uint32_t numOobDataElements;
  uint32_t perPacketInfoOffset;
  uint32_t perPacketInfoLength;
  uint32_t vcHandle;
  uint32_t reserved;
};

OSDefineMetaClassAndStructors(RNDISProtocol, OSObject);

bool RNDISProtocol::Init(USBTransport *transportRef, const Callbacks &callbacksRef) {
  if (transportRef == nullptr) {
    return false;
  }
  transport = transportRef;
  callbacks = callbacksRef;
  state.ready = false;
  state.maxTransferSize = 0;
  state.macAddress = {};
  return true;
}

void RNDISProtocol::TearDown() {
  phase = Phase::kIdle;
  state.ready = false;
}

void RNDISProtocol::BeginInitialize() {
  if (transport == nullptr) {
    return;
  }
  phase = Phase::kInitialize;
  SendInitialize();
}

void RNDISProtocol::SendInitialize() {
  RNDISInitializeMessage message {};
  message.header.messageType = RNDIS_MSG_INIT;
  message.header.messageLength = sizeof(RNDISInitializeMessage);
  message.requestId = requestId++;
  message.majorVersion = 1;
  message.minorVersion = 0;
  message.maxTransferSize = 0x4000;

  transport->SendControlMessage(&message, sizeof(message));
}

void RNDISProtocol::SendQuery(uint32_t oid) {
  RNDISQueryMessage message {};
  message.header.messageType = RNDIS_MSG_QUERY;
  message.header.messageLength = sizeof(RNDISQueryMessage);
  message.requestId = requestId++;
  message.oid = oid;
  message.infoBufferLength = 0;
  message.infoBufferOffset = 0;
  message.deviceVcHandle = 0;

  transport->SendControlMessage(&message, sizeof(message));
}

void RNDISProtocol::SendSet(uint32_t oid, const void *payload, uint32_t length) {
  uint8_t buffer[sizeof(RNDISSetMessage) + 32] {};
  if (length > 32) {
    OSLog("RNDISProtocol: set payload too large\n");
    phase = Phase::kError;
    return;
  }

  auto *message = reinterpret_cast<RNDISSetMessage *>(buffer);
  message->header.messageType = RNDIS_MSG_SET;
  message->header.messageLength = sizeof(RNDISSetMessage) + length;
  message->requestId = requestId++;
  message->oid = oid;
  message->infoBufferLength = length;
  message->infoBufferOffset = sizeof(RNDISSetMessage) - sizeof(RNDISMessageHeader);
  message->deviceVcHandle = 0;

  if (payload != nullptr && length > 0) {
    memcpy(buffer + sizeof(RNDISSetMessage), payload, length);
  }

  transport->SendControlMessage(buffer, message->header.messageLength);
}

void RNDISProtocol::SendKeepAlive() {
  RNDISKeepAliveMessage message {};
  message.header.messageType = RNDIS_MSG_KEEPALIVE;
  message.header.messageLength = sizeof(RNDISKeepAliveMessage);
  message.requestId = requestId++;

  transport->SendControlMessage(&message, sizeof(message));
}

void RNDISProtocol::HandleControlResponse(const void *buffer, uint32_t length) {
  if (buffer == nullptr || length < sizeof(RNDISMessageHeader)) {
    return;
  }

  const auto *header = reinterpret_cast<const RNDISMessageHeader *>(buffer);
  switch (header->messageType) {
    case RNDIS_MSG_INIT_CMPLT:
      HandleInitializeComplete(buffer, length);
      break;
    case RNDIS_MSG_QUERY_CMPLT:
      if (phase == Phase::kQueryMac) {
        HandleQueryComplete(OID_802_3_CURRENT_ADDRESS, buffer, length);
      } else if (phase == Phase::kQueryMTU) {
        HandleQueryComplete(OID_GEN_MAXIMUM_TOTAL_SIZE, buffer, length);
      }
      break;
    case RNDIS_MSG_SET_CMPLT:
      HandleSetComplete(buffer, length);
      break;
    case RNDIS_MSG_KEEPALIVE_CMPLT:
      HandleKeepAliveComplete(buffer, length);
      break;
    default:
      OSLog("RNDISProtocol: unexpected control response 0x%x\n", header->messageType);
      break;
  }
}

void RNDISProtocol::HandleDataMessage(const void *buffer, uint32_t length) {
  if (buffer == nullptr || length < sizeof(RNDISPacketMessage)) {
    return;
  }

  const auto *packet = reinterpret_cast<const RNDISPacketMessage *>(buffer);
  if (packet->header.messageType != RNDIS_MSG_PACKET) {
    return;
  }

  const uint32_t payloadOffset = packet->dataOffset + sizeof(RNDISMessageHeader);
  if (payloadOffset + packet->dataLength > length) {
    OSLog("RNDISProtocol: invalid packet length\n");
    return;
  }

  const uint8_t *payload = reinterpret_cast<const uint8_t *>(buffer) + payloadOffset;
  if (callbacks.onPacketReceived != nullptr) {
    callbacks.onPacketReceived(callbacks.target, payload, packet->dataLength);
  }
}

bool RNDISProtocol::SendEthernetFrame(const uint8_t *data, uint32_t length) {
  if (data == nullptr || length == 0) {
    return false;
  }
  if (!state.ready) {
    return false;
  }

  uint8_t buffer[sizeof(RNDISPacketMessage) + 1600] {};
  if (length > 1600) {
    return false;
  }

  auto *packet = reinterpret_cast<RNDISPacketMessage *>(buffer);
  packet->header.messageType = RNDIS_MSG_PACKET;
  packet->header.messageLength = sizeof(RNDISPacketMessage) + length;
  packet->dataOffset = sizeof(RNDISPacketMessage) - sizeof(RNDISMessageHeader);
  packet->dataLength = length;
  packet->oobDataOffset = 0;
  packet->oobDataLength = 0;
  packet->numOobDataElements = 0;
  packet->perPacketInfoOffset = 0;
  packet->perPacketInfoLength = 0;
  packet->vcHandle = 0;
  packet->reserved = 0;

  memcpy(buffer + sizeof(RNDISPacketMessage), data, length);
  return transport->SendBulkOut(buffer, packet->header.messageLength);
}

void RNDISProtocol::HandleInitializeComplete(const void *buffer, uint32_t length) {
  if (length < sizeof(RNDISInitializeComplete)) {
    phase = Phase::kError;
    return;
  }

  const auto *response = reinterpret_cast<const RNDISInitializeComplete *>(buffer);
  if (response->status != RNDIS_STATUS_SUCCESS) {
    phase = Phase::kError;
    return;
  }

  state.maxTransferSize = response->maxTransferSize;
  phase = Phase::kQueryMac;
  SendQuery(OID_802_3_CURRENT_ADDRESS);
}

void RNDISProtocol::HandleQueryComplete(uint32_t oid, const void *buffer, uint32_t length) {
  if (length < sizeof(RNDISQueryComplete)) {
    phase = Phase::kError;
    return;
  }

  const auto *response = reinterpret_cast<const RNDISQueryComplete *>(buffer);
  if (response->status != RNDIS_STATUS_SUCCESS) {
    phase = Phase::kError;
    return;
  }

  const uint32_t dataOffset = response->infoBufferOffset + sizeof(RNDISMessageHeader);
  if (dataOffset + response->infoBufferLength > length) {
    phase = Phase::kError;
    return;
  }

  const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer) + dataOffset;
  if (oid == OID_802_3_CURRENT_ADDRESS && response->infoBufferLength >= 6) {
    memcpy(state.macAddress.bytes, data, 6);
    phase = Phase::kQueryMTU;
    SendQuery(OID_GEN_MAXIMUM_TOTAL_SIZE);
  } else if (oid == OID_GEN_MAXIMUM_TOTAL_SIZE && response->infoBufferLength >= sizeof(uint32_t)) {
    uint32_t mtu = 0;
    memcpy(&mtu, data, sizeof(uint32_t));
    if (mtu > 0) {
      state.maxTransferSize = mtu;
    }
    phase = Phase::kSetPacketFilter;
    uint32_t filter = RNDIS_PACKET_FILTER_DIRECTED | RNDIS_PACKET_FILTER_BROADCAST |
      RNDIS_PACKET_FILTER_ALL_MULTICAST;
    SendSet(OID_GEN_CURRENT_PACKET_FILTER, &filter, sizeof(filter));
  }
}

void RNDISProtocol::HandleSetComplete(const void *buffer, uint32_t length) {
  if (length < sizeof(RNDISSetComplete)) {
    phase = Phase::kError;
    return;
  }

  const auto *response = reinterpret_cast<const RNDISSetComplete *>(buffer);
  if (response->status != RNDIS_STATUS_SUCCESS) {
    phase = Phase::kError;
    return;
  }

  phase = Phase::kReady;
  state.ready = true;
  if (callbacks.onLinkUp != nullptr) {
    callbacks.onLinkUp(callbacks.target, state);
  }
}

void RNDISProtocol::HandleKeepAliveComplete(const void *buffer, uint32_t length) {
  if (length < sizeof(RNDISKeepAliveComplete)) {
    return;
  }

  const auto *response = reinterpret_cast<const RNDISKeepAliveComplete *>(buffer);
  if (response->status != RNDIS_STATUS_SUCCESS) {
    phase = Phase::kError;
    state.ready = false;
    if (callbacks.onLinkDown != nullptr) {
      callbacks.onLinkDown(callbacks.target);
    }
  }
}
