# Changelog

## V1.1.0-alpha - 29.09.2026
- Add ZigBee Coordinator & router functionality
- Fix wifi scan
- Implement runtime loglevel change
- Rework frontend
- Add proper update status information 
- Create frontend section for network and mdns 
- Build automation

## V1.0.0-alpha - 18.06.2026
Alpha Firmware release. Manually created.

Suffixes:
`.debug` - Full debug output to console enabled during compilation.
`.ota` - Only contains application info. No partition table etc. --> Only used for Updating Firmware for example via the Web interface.
`.full` - Complete Firmware for the 4MB ESP32 Flash storage. DON'T USE IN Web interface.