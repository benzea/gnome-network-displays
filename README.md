This is an experimental GNOME Screencaster implementing Wi-Fi Display (aka Miracast).

Currently a test image will be streamed.

To use it, you will need:
 * openh264 or x264
 * For audio supporting using AAC one of fdkaacenc, faac or avenc_aac
 * NetworkManager P2P patches
   https://gitlab.freedesktop.org/NetworkManager/NetworkManager/merge_requests/24

The changes are work in progress and need cleaning up. However, this should
already work with at least some Miracast devices.

Issues
======

Currently the screencast seems to get frames in the wrong order from GNOME
shell when the shell has multiple monitors. This is likely a bug in mutter
or pipewire.

Testing
=======

For testing purposes you can run with SCREENCAST_DUMMY=1 set. In that case, a dummy
sink will be provided that allows connecting on localhost using any RTSP capable
client to test WFD streaming.

You can connect to rtsp://localhost:7236/wfd1.0 then.
