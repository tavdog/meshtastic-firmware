/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.9.1 */

#ifndef PB_MESHTASTIC_MESHTASTIC_DEVICEONLY_PB_H_INCLUDED
#define PB_MESHTASTIC_MESHTASTIC_DEVICEONLY_PB_H_INCLUDED
#include <pb.h>
#include <vector>
#include "meshtastic/channel.pb.h"
#include "meshtastic/mesh.pb.h"
#include "meshtastic/telemetry.pb.h"
#include "meshtastic/config.pb.h"

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Struct definitions */
/* Position with static location information only for NodeDBLite */
typedef struct _meshtastic_PositionLite {
    /* The new preferred location encoding, multiply by 1e-7 to get degrees
 in floating point */
    int32_t latitude_i;
    /* TODO: REPLACE */
    int32_t longitude_i;
    /* In meters above MSL (but see issue #359) */
    int32_t altitude;
    /* This is usually not sent over the mesh (to save space), but it is sent
 from the phone so that the local device can set its RTC If it is sent over
 the mesh (because there are devices on the mesh without GPS), it will only
 be sent by devices which has a hardware GPS clock.
 seconds since 1970 */
    uint32_t time;
    /* TODO: REPLACE */
    meshtastic_Position_LocSource location_source;
} meshtastic_PositionLite;

typedef PB_BYTES_ARRAY_T(32) meshtastic_UserLite_public_key_t;
typedef struct _meshtastic_UserLite {
    /* This is the addr of the radio. */
    pb_byte_t macaddr[6];
    /* A full name for this user, i.e. "Kevin Hester" */
    char long_name[40];
    /* A VERY short name, ideally two characters.
 Suitable for a tiny OLED screen */
    char short_name[5];
    /* TBEAM, HELTEC, etc...
 Starting in 1.2.11 moved to hw_model enum in the NodeInfo object.
 Apps will still need the string here for older builds
 (so OTA update can find the right image), but if the enum is available it will be used instead. */
    meshtastic_HardwareModel hw_model;
    /* In some regions Ham radio operators have different bandwidth limitations than others.
 If this user is a licensed operator, set this flag.
 Also, "long_name" should be their licence number. */
    bool is_licensed;
    /* Indicates that the user's role in the mesh */
    meshtastic_Config_DeviceConfig_Role role;
    /* The public key of the user's device.
 This is sent out to other nodes on the mesh to allow them to compute a shared secret key. */
    meshtastic_UserLite_public_key_t public_key;
} meshtastic_UserLite;

typedef struct _meshtastic_NodeInfoLite {
    /* The node number */
    uint32_t num;
    /* The user info for this node */
    bool has_user;
    meshtastic_UserLite user;
    /* This position data. Note: before 1.2.14 we would also store the last time we've heard from this node in position.time, that is no longer true.
 Position.time now indicates the last time we received a POSITION from that node. */
    bool has_position;
    meshtastic_PositionLite position;
    /* Returns the Signal-to-noise ratio (SNR) of the last received message,
 as measured by the receiver. Return SNR of the last received message in dB */
    float snr;
    /* Set to indicate the last time we received a packet from this node */
    uint32_t last_heard;
    /* The latest device metrics for the node. */
    bool has_device_metrics;
    meshtastic_DeviceMetrics device_metrics;
    /* local channel index we heard that node on. Only populated if its not the default channel. */
    uint8_t channel;
    /* True if we witnessed the node over MQTT instead of LoRA transport */
    bool via_mqtt;
    /* Number of hops away from us this node is (0 if direct neighbor) */
    bool has_hops_away;
    uint8_t hops_away;
    /* True if node is in our favorites list
 Persists between NodeDB internal clean ups */
    bool is_favorite;
    /* True if node is in our ignored list
 Persists between NodeDB internal clean ups */
    bool is_ignored;
    /* Last byte of the node number of the node that should be used as the next hop to reach this node. */
    uint8_t next_hop;
} meshtastic_NodeInfoLite;

/* This message is never sent over the wire, but it is used for serializing DB
 state to flash in the device code
 FIXME, since we write this each time we enter deep sleep (and have infinite
 flash) it would be better to use some sort of append only data structure for
 the receive queue and use the preferences store for the other stuff */
typedef struct _meshtastic_DeviceState {
    /* Read only settings/info about this node */
    bool has_my_node;
    meshtastic_MyNodeInfo my_node;
    /* My owner info */
    bool has_owner;
    meshtastic_User owner;
    /* Received packets saved for delivery to the phone */
    pb_size_t receive_queue_count;
    meshtastic_MeshPacket receive_queue[1];
    /* We keep the last received text message (only) stored in the device flash,
 so we can show it on the screen.
 Might be null */
    bool has_rx_text_message;
    meshtastic_MeshPacket rx_text_message;
    /* A version integer used to invalidate old save files when we make
 incompatible changes This integer is set at build time and is private to
 NodeDB.cpp in the device code. */
    uint32_t version;
    /* Used only during development.
 Indicates developer is testing and changes should never be saved to flash.
 Deprecated in 2.3.1 */
    bool no_save;
    /* Previously used to manage GPS factory resets.
 Deprecated in 2.5.23 */
    bool did_gps_reset;
    /* We keep the last received waypoint stored in the device flash,
 so we can show it on the screen.
 Might be null */
    bool has_rx_waypoint;
    meshtastic_MeshPacket rx_waypoint;
    /* The mesh's nodes with their available gpio pins for RemoteHardware module */
    pb_size_t node_remote_hardware_pins_count;
    meshtastic_NodeRemoteHardwarePin node_remote_hardware_pins[12];
    /* New lite version of NodeDB to decrease memory footprint */
    std::vector<meshtastic_NodeInfoLite> node_db_lite;
} meshtastic_DeviceState;

/* The on-disk saved channels */
typedef struct _meshtastic_ChannelFile {
    /* The channels our node knows about */
    pb_size_t channels_count;
    meshtastic_Channel channels[8];
    /* A version integer used to invalidate old save files when we make
 incompatible changes This integer is set at build time and is private to
 NodeDB.cpp in the device code. */
    uint32_t version;
} meshtastic_ChannelFile;


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define meshtastic_PositionLite_init_default     {0, 0, 0, 0, _meshtastic_Position_LocSource_MIN}
#define meshtastic_UserLite_init_default         {{0}, "", "", _meshtastic_HardwareModel_MIN, 0, _meshtastic_Config_DeviceConfig_Role_MIN, {0, {0}}}
#define meshtastic_NodeInfoLite_init_default     {0, false, meshtastic_UserLite_init_default, false, meshtastic_PositionLite_init_default, 0, 0, false, meshtastic_DeviceMetrics_init_default, 0, 0, false, 0, 0, 0, 0}
#define meshtastic_DeviceState_init_default      {false, meshtastic_MyNodeInfo_init_default, false, meshtastic_User_init_default, 0, {meshtastic_MeshPacket_init_default}, false, meshtastic_MeshPacket_init_default, 0, 0, 0, false, meshtastic_MeshPacket_init_default, 0, {meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default, meshtastic_NodeRemoteHardwarePin_init_default}, {0}}
#define meshtastic_ChannelFile_init_default      {0, {meshtastic_Channel_init_default, meshtastic_Channel_init_default, meshtastic_Channel_init_default, meshtastic_Channel_init_default, meshtastic_Channel_init_default, meshtastic_Channel_init_default, meshtastic_Channel_init_default, meshtastic_Channel_init_default}, 0}
#define meshtastic_PositionLite_init_zero        {0, 0, 0, 0, _meshtastic_Position_LocSource_MIN}
#define meshtastic_UserLite_init_zero            {{0}, "", "", _meshtastic_HardwareModel_MIN, 0, _meshtastic_Config_DeviceConfig_Role_MIN, {0, {0}}}
#define meshtastic_NodeInfoLite_init_zero        {0, false, meshtastic_UserLite_init_zero, false, meshtastic_PositionLite_init_zero, 0, 0, false, meshtastic_DeviceMetrics_init_zero, 0, 0, false, 0, 0, 0, 0}
#define meshtastic_DeviceState_init_zero         {false, meshtastic_MyNodeInfo_init_zero, false, meshtastic_User_init_zero, 0, {meshtastic_MeshPacket_init_zero}, false, meshtastic_MeshPacket_init_zero, 0, 0, 0, false, meshtastic_MeshPacket_init_zero, 0, {meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero, meshtastic_NodeRemoteHardwarePin_init_zero}, {0}}
#define meshtastic_ChannelFile_init_zero         {0, {meshtastic_Channel_init_zero, meshtastic_Channel_init_zero, meshtastic_Channel_init_zero, meshtastic_Channel_init_zero, meshtastic_Channel_init_zero, meshtastic_Channel_init_zero, meshtastic_Channel_init_zero, meshtastic_Channel_init_zero}, 0}

/* Field tags (for use in manual encoding/decoding) */
#define meshtastic_PositionLite_latitude_i_tag   1
#define meshtastic_PositionLite_longitude_i_tag  2
#define meshtastic_PositionLite_altitude_tag     3
#define meshtastic_PositionLite_time_tag         4
#define meshtastic_PositionLite_location_source_tag 5
#define meshtastic_UserLite_macaddr_tag          1
#define meshtastic_UserLite_long_name_tag        2
#define meshtastic_UserLite_short_name_tag       3
#define meshtastic_UserLite_hw_model_tag         4
#define meshtastic_UserLite_is_licensed_tag      5
#define meshtastic_UserLite_role_tag             6
#define meshtastic_UserLite_public_key_tag       7
#define meshtastic_NodeInfoLite_num_tag          1
#define meshtastic_NodeInfoLite_user_tag         2
#define meshtastic_NodeInfoLite_position_tag     3
#define meshtastic_NodeInfoLite_snr_tag          4
#define meshtastic_NodeInfoLite_last_heard_tag   5
#define meshtastic_NodeInfoLite_device_metrics_tag 6
#define meshtastic_NodeInfoLite_channel_tag      7
#define meshtastic_NodeInfoLite_via_mqtt_tag     8
#define meshtastic_NodeInfoLite_hops_away_tag    9
#define meshtastic_NodeInfoLite_is_favorite_tag  10
#define meshtastic_NodeInfoLite_is_ignored_tag   11
#define meshtastic_NodeInfoLite_next_hop_tag     12
#define meshtastic_DeviceState_my_node_tag       2
#define meshtastic_DeviceState_owner_tag         3
#define meshtastic_DeviceState_receive_queue_tag 5
#define meshtastic_DeviceState_rx_text_message_tag 7
#define meshtastic_DeviceState_version_tag       8
#define meshtastic_DeviceState_no_save_tag       9
#define meshtastic_DeviceState_did_gps_reset_tag 11
#define meshtastic_DeviceState_rx_waypoint_tag   12
#define meshtastic_DeviceState_node_remote_hardware_pins_tag 13
#define meshtastic_DeviceState_node_db_lite_tag  14
#define meshtastic_ChannelFile_channels_tag      1
#define meshtastic_ChannelFile_version_tag       2

/* Struct field encoding specification for nanopb */
#define meshtastic_PositionLite_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, SFIXED32, latitude_i,        1) \
X(a, STATIC,   SINGULAR, SFIXED32, longitude_i,       2) \
X(a, STATIC,   SINGULAR, INT32,    altitude,          3) \
X(a, STATIC,   SINGULAR, FIXED32,  time,              4) \
X(a, STATIC,   SINGULAR, UENUM,    location_source,   5)
#define meshtastic_PositionLite_CALLBACK NULL
#define meshtastic_PositionLite_DEFAULT NULL

#define meshtastic_UserLite_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, FIXED_LENGTH_BYTES, macaddr,           1) \
X(a, STATIC,   SINGULAR, STRING,   long_name,         2) \
X(a, STATIC,   SINGULAR, STRING,   short_name,        3) \
X(a, STATIC,   SINGULAR, UENUM,    hw_model,          4) \
X(a, STATIC,   SINGULAR, BOOL,     is_licensed,       5) \
X(a, STATIC,   SINGULAR, UENUM,    role,              6) \
X(a, STATIC,   SINGULAR, BYTES,    public_key,        7)
#define meshtastic_UserLite_CALLBACK NULL
#define meshtastic_UserLite_DEFAULT NULL

#define meshtastic_NodeInfoLite_FIELDLIST(X, a) \
X(a, STATIC,   SINGULAR, UINT32,   num,               1) \
X(a, STATIC,   OPTIONAL, MESSAGE,  user,              2) \
X(a, STATIC,   OPTIONAL, MESSAGE,  position,          3) \
X(a, STATIC,   SINGULAR, FLOAT,    snr,               4) \
X(a, STATIC,   SINGULAR, FIXED32,  last_heard,        5) \
X(a, STATIC,   OPTIONAL, MESSAGE,  device_metrics,    6) \
X(a, STATIC,   SINGULAR, UINT32,   channel,           7) \
X(a, STATIC,   SINGULAR, BOOL,     via_mqtt,          8) \
X(a, STATIC,   OPTIONAL, UINT32,   hops_away,         9) \
X(a, STATIC,   SINGULAR, BOOL,     is_favorite,      10) \
X(a, STATIC,   SINGULAR, BOOL,     is_ignored,       11) \
X(a, STATIC,   SINGULAR, UINT32,   next_hop,         12)
#define meshtastic_NodeInfoLite_CALLBACK NULL
#define meshtastic_NodeInfoLite_DEFAULT NULL
#define meshtastic_NodeInfoLite_user_MSGTYPE meshtastic_UserLite
#define meshtastic_NodeInfoLite_position_MSGTYPE meshtastic_PositionLite
#define meshtastic_NodeInfoLite_device_metrics_MSGTYPE meshtastic_DeviceMetrics

#define meshtastic_DeviceState_FIELDLIST(X, a) \
X(a, STATIC,   OPTIONAL, MESSAGE,  my_node,           2) \
X(a, STATIC,   OPTIONAL, MESSAGE,  owner,             3) \
X(a, STATIC,   REPEATED, MESSAGE,  receive_queue,     5) \
X(a, STATIC,   OPTIONAL, MESSAGE,  rx_text_message,   7) \
X(a, STATIC,   SINGULAR, UINT32,   version,           8) \
X(a, STATIC,   SINGULAR, BOOL,     no_save,           9) \
X(a, STATIC,   SINGULAR, BOOL,     did_gps_reset,    11) \
X(a, STATIC,   OPTIONAL, MESSAGE,  rx_waypoint,      12) \
X(a, STATIC,   REPEATED, MESSAGE,  node_remote_hardware_pins,  13) \
X(a, CALLBACK, REPEATED, MESSAGE,  node_db_lite,     14)
extern bool meshtastic_DeviceState_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_t *field);
#define meshtastic_DeviceState_CALLBACK meshtastic_DeviceState_callback
#define meshtastic_DeviceState_DEFAULT NULL
#define meshtastic_DeviceState_my_node_MSGTYPE meshtastic_MyNodeInfo
#define meshtastic_DeviceState_owner_MSGTYPE meshtastic_User
#define meshtastic_DeviceState_receive_queue_MSGTYPE meshtastic_MeshPacket
#define meshtastic_DeviceState_rx_text_message_MSGTYPE meshtastic_MeshPacket
#define meshtastic_DeviceState_rx_waypoint_MSGTYPE meshtastic_MeshPacket
#define meshtastic_DeviceState_node_remote_hardware_pins_MSGTYPE meshtastic_NodeRemoteHardwarePin
#define meshtastic_DeviceState_node_db_lite_MSGTYPE meshtastic_NodeInfoLite

#define meshtastic_ChannelFile_FIELDLIST(X, a) \
X(a, STATIC,   REPEATED, MESSAGE,  channels,          1) \
X(a, STATIC,   SINGULAR, UINT32,   version,           2)
#define meshtastic_ChannelFile_CALLBACK NULL
#define meshtastic_ChannelFile_DEFAULT NULL
#define meshtastic_ChannelFile_channels_MSGTYPE meshtastic_Channel

extern const pb_msgdesc_t meshtastic_PositionLite_msg;
extern const pb_msgdesc_t meshtastic_UserLite_msg;
extern const pb_msgdesc_t meshtastic_NodeInfoLite_msg;
extern const pb_msgdesc_t meshtastic_DeviceState_msg;
extern const pb_msgdesc_t meshtastic_ChannelFile_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define meshtastic_PositionLite_fields &meshtastic_PositionLite_msg
#define meshtastic_UserLite_fields &meshtastic_UserLite_msg
#define meshtastic_NodeInfoLite_fields &meshtastic_NodeInfoLite_msg
#define meshtastic_DeviceState_fields &meshtastic_DeviceState_msg
#define meshtastic_ChannelFile_fields &meshtastic_ChannelFile_msg

/* Maximum encoded size of messages (where known) */
/* meshtastic_DeviceState_size depends on runtime parameters */
#define MESHTASTIC_MESHTASTIC_DEVICEONLY_PB_H_MAX_SIZE meshtastic_ChannelFile_size
#define meshtastic_ChannelFile_size              718
#define meshtastic_NodeInfoLite_size             188
#define meshtastic_PositionLite_size             28
#define meshtastic_UserLite_size                 96

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
