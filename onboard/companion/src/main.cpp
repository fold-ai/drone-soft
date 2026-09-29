#include "companion/app.hpp"

#include "iface/detection_ipc.hpp"
#include "mavlink_bridge/bridge.hpp"
#include "mavlink_v1_cmds.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {
volatile std::sig_atomic_t g_stop = 0;
void on_stop(int) { g_stop = 1; }

double now_s() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void print_status(const actprove::companion::CompanionStatus& st, bool fc_open) {
  const char* phase = "MANUAL";
  if (st.phase == actprove::companion::CompanionPhase::Search) phase = "SEARCH";
  if (st.phase == actprove::companion::CompanionPhase::Close) phase = "CLOSE";
  std::printf(
      "METRIC phase=%s intent=%u lock=%d armed=%d track=%d guided=%d "
      "fc_open=%d vx=%.2f vy=%.2f vz=%.2f range=%.1f q=%.2f id=%u\n",
      phase, static_cast<unsigned>(st.intent), st.lock_latched ? 1 : 0,
      st.armed ? 1 : 0, st.track_ok ? 1 : 0, st.want_guided ? 1 : 0,
      fc_open ? 1 : 0, st.vx, st.vy, st.vz, st.range_est_m, st.lock_quality,
      st.track_id);
  std::fflush(stdout);
}
}  // namespace

int main(int argc, char** argv) {
  actprove::mavlink_bridge::BridgeConfig bridge_cfg;
  actprove::companion::CompanionConfig app_cfg;
  std::string ipc_path = "/run/actprove/detections.sock";
  bool smoke = false;
  bool require_ipc = true;
  int inject_n = 0;
  float inject_box[6] = {900.f, 500.f, 80.f, 50.f, 0.85f, 0.f};

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto need = [&](const char* name) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "missing value for %s\n", name);
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--port") bridge_cfg.port = need("--port");
    else if (a == "--baud") bridge_cfg.baud = std::atoi(need("--baud"));
    else if (a == "--ipc") ipc_path = need("--ipc");
    else if (a == "--lock-ch") app_cfg.rc.lock_ch = std::atoi(need("--lock-ch"));
    else if (a == "--takeover-ch") app_cfg.rc.takeover_ch = std::atoi(need("--takeover-ch"));
    else if (a == "--abort-ch") app_cfg.rc.abort_ch = std::atoi(need("--abort-ch"));
    else if (a == "--speed") app_cfg.homing.close_speed_mps = std::strtof(need("--speed"), nullptr);
    else if (a == "--hfov") app_cfg.homing.hfov_deg = std::strtof(need("--hfov"), nullptr);
    else if (a == "--known-width") app_cfg.homing.known_width_m = std::strtof(need("--known-width"), nullptr);
    else if (a == "--smoke") {
      smoke = true;
      require_ipc = false;
      bridge_cfg.smoke = true;
      app_cfg.require_armed = false;
    }
    else if (a == "--no-ipc") require_ipc = false;
    else if (a == "--allow-any-class") app_cfg.allow_any_class = true;
    else if (a == "--class-gate") app_cfg.allow_any_class = false;
    else if (a == "--inject-box") {
      // x,y,w,h,conf,class
      std::sscanf(need("--inject-box"), "%f,%f,%f,%f,%f,%f",
                  &inject_box[0], &inject_box[1], &inject_box[2],
                  &inject_box[3], &inject_box[4], &inject_box[5]);
      inject_n = 3;
    }
    else if (a == "--help") {
      std::puts(
          "actprove_companion — Orin Lock/Takeover intercept loop\n"
          "  --port /dev/ttyTHS1  --baud 57600  --ipc /run/actprove/detections.sock\n"
          "  --lock-ch 7  --takeover-ch 8  --abort-ch 6\n"
          "  --speed 18  --hfov 10  --known-width 0.35\n"
          "  balloon first test: --known-width 1.2 --hfov 10\n"
          "  --smoke   host dry-run (no UART)\n"
          "Takeover always returns stick control. Lock latches until Takeover.");
      return 0;
    }
  }

  std::signal(SIGINT, on_stop);
  std::signal(SIGTERM, on_stop);

  actprove::companion::CompanionApp app(app_cfg);
  actprove::mavlink_bridge::Bridge bridge(bridge_cfg);
  const bool fc_open = bridge.open();
  if (!fc_open && !smoke) {
    std::fprintf(stderr, "companion: failed to open %s (continuing; no GUIDED)\n",
                 bridge_cfg.port.c_str());
  }

  actprove::iface::UnixDetectionSubscriber ipc(ipc_path);
  bool ipc_open = false;
  if (!smoke && !ipc_path.empty()) {
    ipc_open = ipc.open();
    if (!ipc_open && require_ipc) {
      std::fprintf(stderr, "companion: failed to bind %s\n", ipc_path.c_str());
      return 1;
    }
  }

  bridge.set_rx_handler([&](const actprove::mavlink_bridge::RxEvent& ev) {
    if (ev.type == actprove::mavlink_bridge::RxEventType::Heartbeat) {
      app.on_fc_heartbeat((ev.base_mode & AP_MAV_MODE_FLAG_SAFETY_ARMED) != 0,
                          ev.custom_mode, now_s());
    } else if (ev.type == actprove::mavlink_bridge::RxEventType::RcChannels) {
      app.on_rc(ev.rc_raw, ev.rc_count ? ev.rc_count : 18);
    }
  });

  if (fc_open && !smoke) {
    bridge.request_data_stream(AP_MAV_DATA_STREAM_RC_CHANNELS, 10, true);
    bridge.request_data_stream(AP_MAV_DATA_STREAM_EXTRA1, 10, true);
  }

  if (smoke) {
    uint16_t ch[18]{};
    for (auto& c : ch) c = 1500;
    ch[6] = 1900;  // CH7 Lock
    app.on_rc(ch, 18);
    app.on_fc_heartbeat(true, AP_COPTER_MODE_STABILIZE, now_s());
    actprove::DetectionMsg det{};
    det.t_pps = 1.0;
    det.src_w = 1920;
    det.src_h = 1080;
    det.n = 1;
    det.boxes[0] = {inject_box[0], inject_box[1], inject_box[2], inject_box[3],
                    inject_box[4], static_cast<uint32_t>(inject_box[5])};
    for (int i = 0; i < std::max(inject_n, 3); ++i) {
      det.t_pps = 1.0 + 0.03 * i;
      det.seq = static_cast<uint64_t>(i + 1);
      app.on_detection(det);
    }
    app.tick(now_s());
    print_status(app.status(), fc_open);
    const auto st = app.status();
    if (!st.want_guided || !st.setpoint_valid) {
      std::fprintf(stderr, "companion smoke: expected GUIDED CLOSE setpoint\n");
      return 1;
    }
    std::puts("companion smoke PASS");
    return 0;
  }

  std::printf("companion ipc=%s fc=%s baud=%d lock_ch=%d takeover_ch=%d\n",
              ipc_path.c_str(), bridge_cfg.port.c_str(), bridge_cfg.baud,
              app_cfg.rc.lock_ch, app_cfg.rc.takeover_ch);
  std::fflush(stdout);

  double last_hb = 0.0;
  double last_sp = 0.0;
  double last_metric = 0.0;
  double last_stream = 0.0;
  double last_mission_tx = 0.0;
  uint8_t last_mission = 255;
  int last_mode_cmd = -1;  // -1 none, 0 manual, 1 guided, 2 rtl

  while (!g_stop) {
    const double t = now_s();
    if (fc_open) {
      bridge.spin_once();
      if (t - last_hb >= 1.0) {
        bridge.send_heartbeat();
        last_hb = t;
      }
      if (t - last_stream >= 2.0) {
        bridge.request_data_stream(AP_MAV_DATA_STREAM_RC_CHANNELS, 10, true);
        last_stream = t;
      }
    }

    if (ipc_open) {
      actprove::DetectionMsg det{};
      while (ipc.receive(det, 0)) {
        app.on_detection(det);
      }
    }

    app.tick(t);
    const auto st = app.status();
    if (st.mission_state != last_mission || t - last_mission_tx >= 0.5) {
      if (fc_open) bridge.send_mission_state(st.mission_state);
      last_mission = st.mission_state;
      last_mission_tx = t;
    }

    int mode_cmd = 0;
    if (st.want_rtl) mode_cmd = 2;
    else if (st.want_guided) mode_cmd = 1;
    else mode_cmd = 0;

    if (fc_open && mode_cmd != last_mode_cmd) {
      if (mode_cmd == 2) {
        bridge.send_rtb();
        bridge.send_set_mode(AP_COPTER_MODE_RTL,
                             AP_MAV_MODE_FLAG_CUSTOM_MODE_ENABLED |
                                 (st.armed ? AP_MAV_MODE_FLAG_SAFETY_ARMED : 0));
      } else if (mode_cmd == 1) {
        bridge.send_set_mode(app_cfg.guided_mode,
                             AP_MAV_MODE_FLAG_CUSTOM_MODE_ENABLED |
                                 (st.armed ? AP_MAV_MODE_FLAG_SAFETY_ARMED : 0));
      } else {
        bridge.send_set_mode(app_cfg.manual_mode,
                             AP_MAV_MODE_FLAG_CUSTOM_MODE_ENABLED |
                                 (st.armed ? AP_MAV_MODE_FLAG_SAFETY_ARMED : 0));
      }
      last_mode_cmd = mode_cmd;
    }

    if (fc_open && st.want_guided && st.setpoint_valid && t - last_sp >= 1.0 / 15.0) {
      bridge.send_setpoint(app.setpoint());
      last_sp = t;
    }

    if (t - last_metric >= 1.0) {
      print_status(st, fc_open);
      last_metric = t;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return 0;
}
