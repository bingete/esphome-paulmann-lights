# esphome-paulmann-lights
ESPHome external component for controlling Paulmann Bluetooth LED lights with an
ESP32-S3. The component manages its own BLE client; a separate `ble_client`
configuration is not needed.

## Features

- On/off, brightness, and color temperature (approximately 2700K-6500K).
- Password-authenticated BLE connections and configurable reconnect attempts.
- Polled lamp state, including changes made outside ESPHome.
- Optional raw device controls and diagnostic text sensors.

## Setup

The supplied configuration targets `esp32-s3-devkitc-1` with the ESP-IDF framework.
Configuration validation and firmware builds have been checked with ESPHome
2026.4.5. Compatibility with other lamp models and ESPHome versions is not
guaranteed.

Start with [example-esp32s3.yaml](example-esp32s3.yaml). Replace the placeholder
BLE MAC address, choose entity names, and provide these entries in your ESPHome
secrets file:

```yaml
wifi_ssid: "your-wifi-network"
wifi_password: "your-wifi-password"
paulmann_esp32s3__encryption_key: "your-32-byte-base64-api-encryption-key"
paulmann_password: "your-lamp-password"
```

Use a valid ESPHome API encryption key, not the placeholder above. Keep the lamp
password quoted so leading zeroes are preserved. The component's default lamp
password is `1234`, but your lamp may use a different one.

The example installs the component directly from GitHub:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/bingete/esphome-paulmann-lights
      ref: main
    refresh: 0s
    components: [paulmann_lights]
```

`refresh: 0s` checks for upstream changes on every validation or build. For a
stable installation, pin `ref` to a known-working commit rather than tracking
`main`. The example uses verbose logging for initial setup; reduce it to `DEBUG`
or `INFO` once the lamp is working.

Keep `esp32_ble_tracker:` enabled and use the current OTA syntax:

```yaml
ota:
  - platform: esphome
```

Validate, build, and upload with:

```sh
esphome config example-esp32s3.yaml
esphome compile example-esp32s3.yaml
esphome run example-esp32s3.yaml
```

The example enables API encryption but does not configure OTA authentication.
Add an OTA password for deployments where firmware uploads need protection.

### Local Development

When the YAML is in this repository's root, replace the Git source with:

```yaml
external_components:
  - source:
      type: local
      path: ./components
    components: [paulmann_lights]
```

For a YAML stored elsewhere, point `path` to this repository's `components`
directory. Local sources let you test uncommitted changes without fetching GitHub.

## Configuration

Each `paulmann_lights` entry represents one physical lamp:

| Option | Default | Description |
| --- | --- | --- |
| `id` | Generated | Set an explicit ID to link entities to the lamp. |
| `mac_address` | Required | Lamp's BLE MAC address. |
| `name` | `Paulmann Light` | Component name used in logs. |
| `password` | `1234` | Lamp's BLE password, as a string. |
| `connection_retries` | `5` | Reconnect-attempt limit, from 1 to 10. |
| `update_interval` | `30s` | Lamp-state polling interval. |

Link each light, number, or text sensor using `paulmann_lights_id`. Multiple lamps
can be configured with different IDs and MAC addresses, subject to the ESP32's
available BLE connection slots.

### Light Behavior

On/off and brightness commands leave the lamp's color temperature unchanged. The
initial ESPHome color value after a restart or firmware update is not sent to the
lamp. Polled lamp values update the displayed color; a changed ESPHome color target
is sent directly to the lamp, independently of brightness transitions.

The component does not automatically change the lamp's working mode. Changes made
with a physical control or another application become visible on the next poll.
Commands require a connected, authenticated BLE session.

### Optional Controls

The `number` platform exposes raw device values. All controls have a step of 1:

| Control | Range | Notes |
| --- | --- | --- |
| `timer` | 0-255 | Raw timer byte; units and special values are not documented. |
| `working_mode` | 0-10 | Raw mode byte; mode meanings are not confirmed. |
| `controller_enable` | 0-1 | Raw controller-enable value. |

These controls are optional. Leave them out if you only need light control.
Changing working mode can affect the lamp's color behavior; do not assume mode
`0` means color-temperature mode.

### Diagnostics

The optional `text_sensor` platform exposes `system_id`, `model`, `serial_number`,
`firmware_revision`, `hardware_revision`, `software_revision`, `manufacturer`,
`ieee_certification`, and `pnp_id`. Availability depends on the lamp's BLE
characteristics. See the full example for their configuration.

## Troubleshooting

- **Authentication fails:** verify the lamp password, including leading zeroes.
- **BLE connection fails:** check the MAC address and range, and disconnect other
  applications that may be holding the lamp's BLE connection.
- **State appears stale:** allow one polling interval for external changes to be
  reported. The default is 30 seconds.
- **Reconnect attempts are exhausted:** this version limits explicit reconnect
  attempts. Restart the ESP if it remains disconnected after the lamp is available.
- **Color changes unexpectedly:** verify you are using the fixed component version
  and have not changed the raw working-mode control. Capture verbose logs around
  the command before changing protocol behavior.

To view device logs:

```sh
esphome logs example-esp32s3.yaml
```

## Repository Layout

- [components/paulmann_lights](components/paulmann_lights): configuration schemas,
  entity registration, and the C++ BLE implementation.
- [example-esp32s3.yaml](example-esp32s3.yaml): complete Git-source example with
  light, optional controls, and diagnostic entities.
- [LICENSE](LICENSE): license terms.

Local secrets, ESPHome build files, and Python caches are excluded from Git.
