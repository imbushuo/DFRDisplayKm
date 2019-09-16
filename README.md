# Touch Bar (DFR) Display Driver for Windows

This driver implements custom display functionality for Apple Touch Bar (DFR)
on Windows 10. Touch Bar is a USB composite device with two configurations.
The first configuration implements basic function and media key input. 
Windows always takes the first configuration by default.

With [proper configuration](https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/selecting-the-configuration-for-a-multiple-interface--composite--usb-d) in the USB composite device driver stack, it is possible to select the second configuration for achieving advanced display and input functionalities.

This driver implements the display (iBridge Display). The composition device also exposes a HID-compliant Touch Pad (actually, a Touch Screen). It does report Y-axis data but such data are useless due to the form factor of Touch Bar. For more information about the HID digitizer, see [DFRContentHost](https://github.com/imbushuo/DFRContentHost/blob/master/Avalonia.DfrFrameBuffer/Digitizer.cs) for further details.

## Motivation

Keep myself busy during the last few weeks of my internship (thank you LinkedIn & Microsoft)

~~I should register this project to //oneweek/ next time~~

## Installation

No official signed binary will be built at this moment. To build the driver, a
few software needs to be installed including Visual Studio 2019 (with C/C++ 
workload) and Windows 10 Driver Kit, Version 1903. Install `DFRUsbCcgp.inf`
for the "Apple Touch Bar" device first, then install `DFRDisplayKm.inf` for the
"iBridge Display" device.

**You need to turn off Secure Boot as described in the [Apple Knowledge Base](https://support.apple.com/en-us/HT208330)**.

## IOCTLs

Two IOCTLs are provided:

* `IOCTL_DFR_UPDATE_FRAMEBUFFER`: Update the FrameBuffer.
* `IOCTL_DFR_CLEAR_FRAMEBUFFER`: Clear the FrameBuffer.

Check the badly-written user-mode application example `DFRDisplayUm.Utility.Console` for detailed usage.

## Known Caveats

* Only Apple T2-based MacBook Pro(s) are confirmed supported. T1 support is added but not yet tested.
* The driver might fails to load on T2 cold boot. Reboot the computer once and it should work.
* `UDCL` read acknowledgement is implemented, but I have not yet intensively tested.
* FrameBuffer update/clear are synchronous calls.

## License

Copyright (c) Bingxing Wang. All rights reserved.

Licensed under the [MIT License](./LICENSE).
