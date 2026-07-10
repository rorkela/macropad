# EE2504: Hybrid Macropad (Wired + Dongle)

## Team

| Name | Roll Number |
|---|---|
| Sarthak Kumar Mishra |EE24BTECH11221|
| Naman Singh |EE24BTECH11216|
| Dhawal Saini |EE24BTECH11015|
| Sidharth V |EE24BTECH11224|
| Vantaku Sai Yashwant |EE24BTECH11225|
| Sriram Nulu |EE24BTECH11217|

## Details
- 2 Layer PCB for Macropad+Dongle Pair
- STM32F103C8T6 Based
- Programmed with `libopencm3`
- ssd1306 display integrated
- CAD Model designed to be 3D Printed for keycaps, encoder caps and casing.
- `microui` based key-map programming interface

## Instruction
1. Make the libraries (`libopencm3`)
2. Make and flash bootloader using `make flash`
3. Make and flash macropad firmware using `make flash` or the dfu script
4. Generate keymaps and flash using `mapping_gen`
# Current Status
- [x] PCB Ordered
## Wired
- [x] Bootloader Firmware
- [x] USB HID Firmware
- [x] DFU Firmware
- [x] Encoder Firmware
- [x] Display Firmware
- [x] All module integration
- [x] Multi key mapping
- [ ] Multi key sequential mapping

## Wireless
- [x] Dongle testing
- [x] nRF24L01 Firmware
- [ ] Battery Charging check
- [ ] Soldering SMD Mosfet + nRF modules
- [ ] Replacing PROG resistor for TP4056 module
- [ ] Complete Integration
## Other
- [x] Mapping generation and flashing with DFU
- [x] GUI
- [ ] Size Optimization for keymaps

## Images
<img width="709" height="805" alt="image" src="https://github.com/user-attachments/assets/433137f9-d3c5-4b3a-b758-14a340197772" />
<img width="1102" height="645" alt="image" src="https://github.com/user-attachments/assets/11dc5a15-2ea6-4633-bed1-5cd90485f131" />

## Links
- [CAD Model (Onshape)](https://cad.onshape.com/documents/8695aacad1c4b6aaf32709eb/w/c4719be3eaa84105acadf71a/e/89d918a88e3d346d4846fa41)
