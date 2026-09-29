#pragma once
#include <actprove/detection_msg.hpp>

namespace actprove::iface {

// Compatibility names for the perception publisher.  The actual wire types
// live in interfaces/include/actprove and are shared with Tracking.
using CamId = ::actprove::CamId;
using ClassId = ::actprove::ClassId;
using Box = ::actprove::Box;
using DetectionMsg = ::actprove::DetectionMsg;
static constexpr uint32_t MAX_DET = ::actprove::kMaxDet;

}  // namespace actprove::iface
