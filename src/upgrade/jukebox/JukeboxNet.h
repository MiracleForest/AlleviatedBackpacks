#pragma once

// Jukebox sound packets (companion protocol, server -> client), the role of
// `PlayDiscPayload` / `StopDiscPlaybackPayload`:
//
//   JukeboxPlayPacket  {"uuid": "<backpack>", "sound": "<sound definition>",
//                       "entity": <ActorUniqueID raw> | "pos": [x, y, z]}
//   JukeboxStopPacket  {"uuid": "<backpack>"}
//
// The client plays the record attached to the entity (it follows the carrier every tick, like
// The `EntityBoundSoundInstance`, for every player including the local one) or at the block
// position of a placed backpack. One sound per backpack: a new Play replaces the backpack's
// previous sound. Sent to companion players within `kSoundRange` blocks only (
// `ServerStorageSoundHandler`: `sendToPlayersNear(..., 128, ...)`).

#include "net/Packets.h"

namespace abp::upgrade::jukebox {

/// play / stop payloads go to players within 128 blocks.
inline constexpr float kSoundRange = 128.0f;

class JukeboxPlayPacket : public net::JsonPacket<JukeboxPlayPacket> {
    using JsonPacket::JsonPacket;
};
class JukeboxStopPacket : public net::JsonPacket<JukeboxStopPacket> {
    using JsonPacket::JsonPacket;
};

} // namespace abp::upgrade::jukebox
