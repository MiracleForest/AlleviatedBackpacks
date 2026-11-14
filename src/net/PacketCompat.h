#pragma once

// LeviLamina runtime packet differences between the supported lines.

#include "ll/api/network/packet/Packet.h"

/// The extra parameter of a runtime packet handler's handlePacket: LeviLamina 26.60 passes the
/// side the packet arrived on between the callback and the packet, 26.51 does not. Write
/// `handlePacket(NetworkIdentifier const&, NetEventCallback&, ABP_PACKET_SIDE_PARAM T const&)`.
#if ABP_LL_VERSION >= 2660
#define ABP_PACKET_SIDE_PARAM ll::network::PacketSide,
#else
#define ABP_PACKET_SIDE_PARAM
#endif
