# esphome-paulmann-lights
ESPHome custom component for controlling Paulmann Bluetooth LED lights with ESP32-S3

## Features

- Light control (on/off, brightness, color temperature)
- Device controls (timer, working mode, controller enable)
- BLE password authentication (`1234` default)
- Automatic reconnect attempts with configurable retry count
- System time synchronization write support
- Device info text sensors (system id, model, serial, firmware/hardware/software revision, manufacturer, IEEE cert, PnP ID)

## ESPHome component structure

- `custom_components/paulmann_lights/__init__.py`
- `custom_components/paulmann_lights/light.py`
- `custom_components/paulmann_lights/number.py`
- `custom_components/paulmann_lights/text_sensor.py`
- `custom_components/paulmann_lights/const.py`
- `custom_components/paulmann_lights/paulmann_lights.h`
- `custom_components/paulmann_lights/paulmann_lights.cpp`
- `example-esp32s3.yaml`

## Usage

Use `example-esp32s3.yaml` as a starting point. Replace the BLE MAC address and Wi-Fi secrets.
Make sure the repository is used as a local external component source:

```yaml
external_components:
  - source:
      type: local
      path: ./custom_components
```

The component targets ESP32-S3 (DevKitC-1 compatible) and requires `esp32_ble_tracker`. It
manages its own BLE client internally, so no separate `ble_client` configuration is required.
