# LCFT Qt 6.10 Client

This directory now contains a Qt 6.10 QML client for the existing LAN/WAN file
transfer protocol.

## Open In Qt Creator

1. Open `CMakeLists.txt` from this directory.
2. Select a Qt 6.10 Desktop kit for Windows, or a Qt 6.10 Android kit.
3. Build and run `appLanChatShell`.

## Protocol Compatibility

The client keeps the same wire protocol as the original console client:

- ECS control: TCP `7000`
- ECS data relay: TCP `7001`
- LAN data: TCP `9000`
- LAN discovery: UDP `9001`
- LAN control: TCP `9002`

Windows and Android clients built from this project can discover each other on
LAN and exchange files using the same chunk protocol.

## Current Notes

- File names with spaces are rejected before sending because the original text
  protocol separates fields with spaces.
- The visible file library uses a default `reserve_file` folder beside the app
  executable when writable, with an app-data fallback for platforms that do not
  allow writing beside the executable.
- Android and narrow windows use a mobile UI with bottom navigation for chats,
  device discovery and the file library.
- Android builds include network, Wi-Fi, multicast and media read permissions in
  `android/AndroidManifest.xml`.
