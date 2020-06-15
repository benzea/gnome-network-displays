# WARNING
# This project has been moved to https://gitlab.gnome.org/GNOME/gnome-network-displays
## All issues have also been moved there.

--------

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

Build
=====

To build it locally:

- clone the repository
- install `meson`
- run `meson build` on the cloned repository
- run `meson install` on the `build` folder created by meson

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

Codecs
------

Appropriate video/audio encoders will be selected automatically. You should
also get a notification if codecs are missing and an offer to automatically
install these. To debug the codecs themselves, you can modify the default
choice by setting the `NETWORK_DISPLAYS_H264_ENC` and `NETWORK_DISPLAYS_AAC_ENC`
environment variables and specifying the gstreamer element to use (if
supported and detected). Run with `G_MESSAGES_DEBUG=all` to see the selection
at work during connection establishment.

Connection issues
-----------------

P2P WiFi/WiFi Direct and the mechanism to establish a connection is a relatively
complicated process that can fail in a number of different ways and stages.

As a first step, you should run with `G_MESSAGES_DEBUG=all` to get a better
idea when the failure happens. Your TV/Display may also give an indication
about the progress. Much of the information is also displayed in the UI, but
might only be visible for a very short period of time.

### Discovery

The first required step is successful discovery of the remote device.
This step can already be problematic sometimes. Confirm the following, each
ensuring that support exists:

 * Run `gnome-network-displays` with `G_MESSAGES_DEBUG=all`. This will print
   a lot more information. Theoretically it may be that we see the device, but
   think that it is not WiFi Display capable (e.g. because the `wpa_supplicant`
   support is missing, see further below). Look out for the following messages:
   * `WFDP2PRegistry: Got NMClient`:
      The connection to NetworkManager works.
   * `WFDP2PRegistry: Found a new device, creating provider`:
      This means that we have a seemingly usable P2P device installed.
   * `WFDP2PProvider: Ignoring peer "XX:XX:XX:XX:XX" (Y) as it has no WFDIEs set`:
      This means there is a P2P device, but it does not seem to support WiFi
      Display. It may also mean that `wpa_supplicant` is not complied with
      the required support, see below.
   * `WFDP2PProvider: Found a new sink with peer X on device Y`:
      The device has been found, everything should be good.

 * `nmcli device` shows your WiFi device and a corresponding `p2p-dev-X` device.
   If you do not see your WiFi device, then NetworkManager is probably not
   managing it for some reason. Both the main WiFi device and the P2P device
   need to be managed by NetworkManager.
 * `iw dev` should show an `Unnamed/non-netdev interface` device with type
   `P2P-device`.

   If this is not the case, then the device is likely unsupported.
   One example of this are legacy `wext` drivers, which may support P2P
   operation, but are not (and will not be) supported by this software.
 * NetworkManager only support P2P operation together with `wpa_supplicant`.
   The use of `iwd` is currently not supported and you may have enabled it
   in NetworkManager.
 * If NetworkManager does not even show a device, then, as root, run

   ```
   gdbus call --system --dest fi.w1.wpa_supplicant1 --object-path /fi/w1/wpa_supplicant1 --method org.freedesktop.DBus.Properties.Get fi.w1.wpa_supplicant1 Capabilities
   ```

   This should output something like `(<['ap', 'ibss-rsn', 'p2p', 'pmf', 'mesh', 'ft', 'sha384']>,)`.
   Make sure that `p2p` is listed here. If it is not, then `wpa_supplicant` was
   compiled without P2P WiFi support (`CONFIG_P2P`).

 * If a device is shown, but it does not work, then, as root, run

   ```
   gdbus call --system --dest fi.w1.wpa_supplicant1 --object-path /fi/w1/wpa_supplicant1 --method org.freedesktop.DBus.Properties.Get fi.w1.wpa_supplicant1 WFDIEs
   ```

   If this runs successfully, it'll return `(<@ay []>,)`. This answer will be
   longer when we try to connect to a device, but only at that point.
   If it shows an error, then your `wpa_supplicant` was compiled without
   WiFi Display support (`CONFIG_WIFI_DISPLAY`).


If everything looks good, but you still can't find the TV, then please try the
following:
 * Search for it using another device at the same time.
 * Try running
   ```
   gdbus call --system --dest fi.w1.wpa_supplicant1 --object-path /fi/w1/wpa_supplicant1 --method org.freedesktop.DBus.Properties.Set fi.w1.wpa_supplicant1 WFDIEs "<@ay [0x00, 0x00, 0x06, 0x00, 0x90, 0x1c, 0x44, 0x00, 0xc8]>"
   ```
   Doing this will set the WFD Information Elements earlier in the process. It
   should not make a difference. But if it does, then some bigger changes will
   be needed.
 * If you can, try to get a capture of another device discovering it.
   ```
   iw phy phyX interface add mon0 type monitor
   ip link set mon0 up
   ```
   Then connect to it using `wireshark`. You may need to disable your normal
   WiFi connection and change the channel using `iw dev`. Explaining the details
   is out of scope for this document.

### Establishing a P2P Group

When you click on the TV, the first step is to establish a WiFi P2P connection.
Check whether you see the message:

```
Got state change notification from streaming sink to state ND_SINK_STATE_WAIT_P2P
[...]
Got state change notification from streaming sink to state ND_SINK_STATE_WAIT_SOCKET
```

If you see both of these messages, then we have successfully created a P2P Group.

### Network configuration and socket connection

What happens next is that both side will configure their network. Then the TV
will establish a connection to `gnome-network-displays`.

Things could go wrong here:
 * Usually, NetworkManager will set up the network to be shared using `dnsmasq`
 * A local firewall might block DHCP requests/responses or prevent the later connection

At this point the IP link is there. You can do the following:

 * Look into the NetworkManager log whether you see something obviously wrong
   (unlikely, as otherwise it should disconnect immediately).
 * The low level link is established (i.e. a P2P network device has been created).
   This means, to debug further, we can montior this network device and see what
   is happening. We should see a DHCP handshake to configure the network, and
   then an attempt to establish an RTSP connection.

   The easiest way to do this is to have `wireshark` running. Then, right after
   clicking on the TV, the P2P device will show up in the list. Quickly start a
   capture on it.

   Alternatively, use a script like the following (as root):

   ```
   interface=""; while [ -z "$interface" ]; do interface=$( ls /sys/class/net/ | grep -- p2p- ); sleep 0.1; done; echo $interface; tcpdump -i "$interface" -w /tmp/p2p-connection-dump.pcap
   ```

   And then open the created dump file `/tmp/p2p-connection-dump.pcap` in Wireshark.

### RTSP stream issues

Not all devices are compliant, and the standard is a bit odd. If you have
problems where the stream seems to start, but then does not work or stop after
a while.

Try the following:
 * Run with `G_MESSAGES_DEBUG=all` and check if you see something that is wrong.
   Note that certain warnings are expected.
 * Try grabbing a `tcpdump`/`wireshark` capture (see above) and check whether
   you see something weird in the RTSP stream.
 * It could also be an issue with the GStreamer pipeline not starting properly.
   This can possibly be debugged using e.g. `GST_DEBUG=*:5`, but is generally
   harder to pin point. Try it a few times, and check if the problem persists.
