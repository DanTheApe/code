#include "New/MavlinkFrames.hpp"

#if USE_LORA

// =============================================================================
//  MavlinkFrames.cpp
//  Each builder packs a single MAVLink message and serializes it into the
//  caller-provided byte buffer. Returned length is what should be transmitted.
// =============================================================================

namespace {

// Input  : msg - a packed MAVLink message; out - destination byte buffer.
// What   : serializes the MAVLink frame to `out` using the v2.0 wire format.
// Output : number of bytes written into `out`.
size_t serialize(const mavlink_message_t& msg, uint8_t* out) {
  return mavlink_msg_to_send_buffer(out, &msg);
}

}  // namespace

namespace MavlinkFrames {

size_t buildHeartbeat(uint8_t* out) {
  mavlink_message_t msg;
  mavlink_msg_heartbeat_pack(MAV_SYSID, MAV_COMPID, &msg,
                             MAV_TYPE_GENERIC,
                             MAV_AUTOPILOT_INVALID,
                             0, 0, MAV_STATE_ACTIVE);
  return serialize(msg, out);
}

size_t buildAccel(uint8_t* out, const AccelReading& r) {
  mavlink_message_t msg;
  // SCALED_IMU carries acc + gyro + mag + temp in one frame.  We zero the
  // gyro/mag/temp fields so this packet only conveys acceleration.
  mavlink_msg_scaled_imu_pack(MAV_SYSID, MAV_COMPID, &msg,
                              r.t_ms,
                              r.x_mg, r.y_mg, r.z_mg,
                              0, 0, 0,    // gyro unused here
                              0, 0, 0,    // mag unused here
                              0);         // temp unused here
  return serialize(msg, out);
}

size_t buildGyro(uint8_t* out, const GyroReading& r) {
  mavlink_message_t msg;
  // SCALED_IMU2 — acc/mag/temp zeroed, only gyro filled.
  mavlink_msg_scaled_imu2_pack(MAV_SYSID, MAV_COMPID, &msg,
                               r.t_ms,
                               0, 0, 0,
                               r.x_mrad_s, r.y_mrad_s, r.z_mrad_s,
                               0, 0, 0,
                               0);
  return serialize(msg, out);
}

size_t buildMag(uint8_t* out, const MagReading& r) {
  mavlink_message_t msg;
  // SCALED_IMU3 — acc/gyro/temp zeroed, only mag filled.
  mavlink_msg_scaled_imu3_pack(MAV_SYSID, MAV_COMPID, &msg,
                               r.t_ms,
                               0, 0, 0,
                               0, 0, 0,
                               r.x_mgauss, r.y_mgauss, r.z_mgauss,
                               0);
  return serialize(msg, out);
}

size_t buildBaroPressure(uint8_t* out, const BaroReading& r) {
  mavlink_message_t msg;
  // SCALED_PRESSURE — only press_abs is meaningful here. Temperature has
  // its own dedicated NAMED_VALUE_INT below so we leave both temp fields 0.
  mavlink_msg_scaled_pressure_pack(MAV_SYSID, MAV_COMPID, &msg,
                                   r.t_ms,
                                   r.pressure_hPa, 0.0f,
                                   0, 0);
  return serialize(msg, out);
}

size_t buildBaroTemp(uint8_t* out, const BaroReading& r) {
  mavlink_message_t msg;
  // NAMED_VALUE_INT carries one int32_t under a 10-char name.
  mavlink_msg_named_value_int_pack(MAV_SYSID, MAV_COMPID, &msg,
                                   r.t_ms, "BARO_T",
                                   static_cast<int32_t>(r.temperature_cC));
  return serialize(msg, out);
}

size_t buildGeiger(uint8_t* out, const GeigerReading& r, uint8_t channel) {
  mavlink_message_t msg;
  const char* name = (channel == 2) ? "GEIG_2" : "GEIG_1";
  uint32_t val = (channel == 2) ? r.counts2 : r.counts1;
  if (val > static_cast<uint32_t>(INT32_MAX)) val = INT32_MAX;
  mavlink_msg_named_value_int_pack(MAV_SYSID, MAV_COMPID, &msg,
                                   r.t_ms, name,
                                   static_cast<int32_t>(val));
  return serialize(msg, out);
}

size_t buildLoraTemp(uint8_t* out, const LoraTempReading& r) {
  mavlink_message_t msg;
  mavlink_msg_named_value_int_pack(MAV_SYSID, MAV_COMPID, &msg,
                                   r.t_ms, "LORA_T",
                                   static_cast<int32_t>(r.t_cC));
  return serialize(msg, out);
}

size_t buildLoraVolt(uint8_t* out, const LoraVoltReading& r) {
  mavlink_message_t msg;
  mavlink_msg_named_value_int_pack(MAV_SYSID, MAV_COMPID, &msg,
                                   r.t_ms, "LORA_V",
                                   static_cast<int32_t>(r.mV));
  return serialize(msg, out);
}

}  // namespace MavlinkFrames

#endif  // USE_LORA
