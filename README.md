This is an experimental GNOME Screencaster implementing Wi-Fi Display (aka Miracast).

Currently a test image will be streamed.

To use it, you will need:
 * openh264 or x264
 * NetworkManager P2P patches
   https://gitlab.freedesktop.org/NetworkManager/NetworkManager/merge_requests/24

The changes are work in progress and need cleaning up. However, this should
already work with at least some Miracast devices.

Issues
======

If there is any connection failure or similar, you will need to restart the
program to try again.

This program requires support for remote desktop in mutter. If this support is
not available for you, then you can turn the "screencast_portal" compile time
option off to fall back to X11 based screen grabbing.


Testing
=======

For testing purposes you can run with SCREENCAST_DUMMY=1 set. In that case, a dummy
sink will be provided that allows connecting on localhost using any RTSP capable
client to test WFD streaming.

You can connect to rtsp://localhost:7236/wfd1.0 then.
