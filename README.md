This is an experimental implementation of Wi-Fi Display (aka Miracast).

The application will stream the selected monitor if the mutter screencast
portal is available. If it is unavailable, a fallback to X11 based frame
grabbing will happen. As such, it should work fine in almost all setups.

To get audio streaming, you need to change the audio output in pulseaudio
to use the created "Network-Displays" sink.

To use it, you will need:
 * openh264 or x264
 * For audio supporting using AAC one of fdkaacenc, faac or avenc_aac
 * NetworkManager version > 1.15.2

Issues
======

Mutter currently has an issue when streaming from a multi-monitor setup. See
  https://gitlab.gnome.org/GNOME/mutter/issues/424

Devices
=======

The following devices have been tested:
 * Measy "Miracast Receiver" Model A2W
   - Announces itself as EZMirror/EZCast
   - Only supports uncompressed audio (LPCM) which is not supported yet
 * LG WebOS TV
 * MontoView (Software Revision 2.18.02)
 * MiraScreen

Testing
=======

For testing purposes you can run with NETWORK_DISPLAYS_DUMMY=1 set. In that case, a dummy
sink will be provided that allows connecting on localhost using any RTSP capable
client to test WFD streaming.

You can connect to rtsp://localhost:7236/wfd1.0 then.

Debugging
=========

Appropriate video/audio encoders will be selected automatically. You can
modify the choice by setting the NETWORK_DISPLAYS_H264_ENC and NETWORK_DISPLAYS_AAC_ENC
environment variables and specifying the gstreamer element to use (if
supported and detected). run with G_MESSAGES_DEBUG=all to see the selection
at work during connection establishment.
