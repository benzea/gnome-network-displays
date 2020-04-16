
wfd_server_sources = [
  'wfd-client.c',
  'wfd-media.c',
  'wfd-media-factory.c',
  'wfd-params.c',
  'wfd-resolution.c',
  'wfd-server.c',
  'wfd-session-pool.c',
  'wfd-audio-codec.c',
  'wfd-video-codec.c',
]

#enum_headers = files()
#
#gnome_nd_sources += gnome.mkenums_simple(
#  'nd-enum-types',
#  sources: enum_headers,
#)

wfd_server_deps = [
  dependency('gstreamer-video-1.0', version: '>= 1.14'),
  dependency('gstreamer-rtsp-1.0', version: '>= 1.14'),
  dependency('gstreamer-rtsp-server-1.0', version: '>= 1.14'),
]

wfd_server = static_library(
  'wfd-server',
  wfd_server_sources,
  dependencies: wfd_server_deps,
)