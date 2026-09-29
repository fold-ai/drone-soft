#include "capture/frame.hpp"

namespace actprove::capture {

double latch_mid_exposure_pps(double sof_pps, float exposure_us) {
  return sof_pps + static_cast<double>(exposure_us) * 1e-6 * 0.5;
}

}  // namespace actprove::capture
