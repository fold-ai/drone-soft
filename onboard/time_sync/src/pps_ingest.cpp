#include "time_sync/pps_ingest.hpp"
#include <chrono>
#include <thread>

namespace actprove::time_sync {

PpsIngest::PpsIngest(Config cfg) : cfg_(std::move(cfg)) {}
PpsIngest::~PpsIngest() { stop(); }

bool PpsIngest::start() {
  // The hardware PPS backend is not implemented yet.  Never report a
  // disciplined clock from a synthetic timer unless simulation was explicit.
  if (!cfg_.simulate) return false;
  if (run_.exchange(true)) return true;
  thr_ = std::thread([this] { loop(); });
  return true;
}

void PpsIngest::stop() {
  if (!run_.exchange(false)) return;
  if (thr_.joinable()) thr_.join();
}

void PpsIngest::loop() {
  // Stub: on target, wait on PPS IRQ / HTE / ppstime — never soft-stamp sensors with now().
  using clock = std::chrono::steady_clock;
  auto t0 = clock::now();
  while (run_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    const auto dt = std::chrono::duration<double>(clock::now() - t0).count();
    last_edge_pps_.store(dt);
    const auto edge_ns = static_cast<PpsTimeNs>(dt * 1e9);
    clock_.note_pps_edge(edge_ns);
    edges_.fetch_add(1);
    disciplined_.store(true);
  }
}

double PpsIngest::now_pps() const {
  return last_edge_pps_.load();
}

double mid_exposure_pps(double sof_pps, double exposure_us) {
  return sof_pps + (exposure_us * 1e-6) * 0.5;
}

}  // namespace actprove::time_sync
