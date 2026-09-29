# drone-soft — код для NVIDIA Orin

Це **вихідники** для переносу на Orin. Бінарників з Mac тут немає — їх треба зібрати вже на Jetson (ARM64).

**Готово переносити:** так, як перший стенд (HDMI Lock + потім companion).  
**Не готово як «вставив флешку і збиває дрон»:** немає TensorRT engine під ваш JetPack, JAI це USB3 Vision (не `/dev/video0`), потрібні HDMI, UART TELEM2 і збірка на платі.

## Що всередині

| Папка | Навіщо |
|-------|--------|
| `onboard/companion/` | Lock CH7 / Takeover CH8 → ArduPilot GUIDED |
| `vision/` | захват + детектор (V4L2/TensorRT); ONNX `vision/training/exports/smoke_e2.onnx` |
| `tools/first_test_hdmi_preview.py` | **перший тест:** картинка на монітор, бокс, LOCK (JAI через Aravis) |
| `deploy/orin/` | systemd |
| `flight/params/ardupilot_companion.params` | Pixhawk 6C TELEM2 |

Не кладіть сюди `.venv` з Mac.

## День 1 на Orin (камера + монітор)

Скопіюйте цю папку на плату як `/opt/drone-soft`. HDMI встроміть **до** буту.

```bash
sudo apt-get update
sudo apt-get install -y python3-opencv python3-gi gir1.2-aravis-0.8 cmake g++
export DISPLAY=:0
python3 /opt/drone-soft/tools/first_test_hdmi_preview.py --target balloon --hfov 10 --known-width 1.2
```

Малий дрон: `--target drone --known-width 0.35`.  
Якщо JAI не видно — USB3 SuperSpeed кабель у порт USB3, не USB2.

## День 2 (companion → Pixhawk)

```bash
cmake -S /opt/drone-soft/onboard -B /opt/drone-soft/onboard/build -DCMAKE_BUILD_TYPE=Release
cmake --build /opt/drone-soft/onboard/build -j$(nproc) --target actprove_companion
/opt/drone-soft/onboard/build/actprove_companion --smoke

cmake -S /opt/drone-soft/vision -B /opt/drone-soft/vision/build -DSOFT_NO_JETPACK=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build /opt/drone-soft/vision/build -j$(nproc)

python3 /opt/drone-soft/tools/build_tensorrt_engine.py \
  --onnx /opt/drone-soft/vision/training/exports/smoke_e2.onnx \
  --engine /opt/drone-soft/vision/models/engines/yolov8n_fp16.engine

sudo /opt/drone-soft/deploy/orin/install_companion.sh
```

Pixhawk: `flight/params/ardupilot_companion.params`. UART: Orin `/dev/ttyTHS1` ↔ TELEM2 @ 57600. Пульт: CH7 Lock, CH8 Takeover.

`dual_detect_node` чекає UVC `/dev/video0`. JAI GOX-5103 на першому тесті крутіть через HDMI-скрипт (Aravis), не через цей node.

Докладно: `docs/hardware/FIRST_TEST_JAI_PIXHAWK_BUY.md`, `docs/bringup/orin_companion.md`.
