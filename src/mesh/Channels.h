#pragma once

#include "CryptoEngine.h"
#include "NodeDB.h"
#include "mesh-pb-constants.h"
#include <Arduino.h>

/** A channel number (index into the channel table)
 */
typedef uint8_t ChannelIndex;

/** A low quality hash of the channel PSK and the channel name.  created by generateHash(chIndex)
 * Used as a hint to limit which PSKs are considered for packet decoding.
 */
typedef uint8_t ChannelHash;

/** The container/on device API for working with channels */
class Channels
{
    /// The index of the primary channel
    ChannelIndex primaryIndex = 0;

    /** The channel index that was requested for sending/receiving.  Note: if this channel is a secondary
    channel and does not have a PSK, we will use the PSK from the primary channel.  If this channel is disabled
    no sending or receiving will be allowed */
    ChannelIndex activeChannelIndex = 0;

    /// the precomputed hashes for each of our channels, or -1 for invalid
    int16_t hashes[MAX_NUM_CHANNELS] = {};

  public:
    Channels() {}

    /// Well known channel names
    static const char *adminChannel, *gpioChannel, *serialChannel, *mqttChannel;

    const meshtastic_ChannelSettings &getPrimary() { return getByIndex(getPrimaryIndex()).settings; }

    /** Return the Channel for a specified index */
    meshtastic_Channel &getByIndex(ChannelIndex chIndex);

    /** Return the Channel for a specified name, return primary if not found. */
    meshtastic_Channel &getByName(const char *chName);

    /** Using the index inside the channel, update the specified channel's settings and role.  If this channel is being promoted
     * to be primary, force all other channels to be secondary.
     */
    void setChannel(const meshtastic_Channel &c);

    /** Return a human friendly name for this channel (and expand any short strings as needed)
     */
    const char *getName(size_t chIndex);

    /**
     * Return a globally unique channel ID usable with MQTT.
     */
    const char *getGlobalId(size_t chIndex) { return getName(chIndex); } // FIXME, not correct

    /** The index of the primary channel */
    ChannelIndex getPrimaryIndex() const { return primaryIndex; }

    ChannelIndex getNumChannels() { return channelFile.channels_count; }

    /// Called by NodeDB on initial boot when the radio config settings are unset.  Set a default single channel config.
    void initDefaults();

    /// called when the user has just changed our radio config and we might need to change channel keys
    void onConfigChanged();

    /** Given a channel hash setup crypto for decoding that channel (or the primary channel if that channel is unsecured)
     *
     * This method is called before decoding inbound packets
     *
     * @return false if the channel hash or channel is invalid
     */
    bool decryptForHash(ChannelIndex chIndex, ChannelHash channelHash);

    /** Given a channel index setup crypto for encoding that channel (or the primary channel if that channel is unsecured)
     *
     * This method is called before encoding outbound packets
     *
     * @eturn the (0 to 255) hash for that channel - if no suitable channel could be found, return -1
     */
    int16_t setActiveByIndex(ChannelIndex channelIndex);

    // Returns true if the channel has the default name and PSK
    bool isDefaultChannel(const meshtastic_Channel &ch);

    // Returns true if we can be reached via a channel with the default settings given a region and modem preset
    bool hasDefaultChannel();

    // Returns true if any of our channels have enabled MQTT uplink or downlink
    bool anyMqttEnabled();

    // cycle the mqttdownlink channel
    void cycleMqttDownlink();

  private:
    /** Given a channel index, change to use the crypto key specified by that index
     *
     * @eturn the (0 to 255) hash for that channel - if no suitable channel could be found, return -1
     */
    int16_t setCrypto(ChannelIndex chIndex);

    /** Return the channel index for the specified channel hash, or -1 for not found */
    int8_t getIndexByHash(ChannelHash channelHash);

    /** Given a channel number, return the (0 to 255) hash for that channel
     * If no suitable channel could be found, return -1
     *
     * called by fixupChannel when a new channel is set
     */
    int16_t generateHash(ChannelIndex channelNum);

    int16_t getHash(ChannelIndex i) { return hashes[i]; }

    /**
     * Validate a channel, fixing any errors as needed
     */
    meshtastic_Channel &fixupChannel(ChannelIndex chIndex);

    /**
     * Write a default channel to the specified channel index
     */
    void initDefaultChannel(ChannelIndex chIndex);

    /**
     * Return the key used for encrypting this channel (if channel is secondary and no key provided, use the primary channel's
     * PSK)
     */
    CryptoKey getKey(ChannelIndex chIndex);
};

/// Singleton channel table
extern Channels channels;
