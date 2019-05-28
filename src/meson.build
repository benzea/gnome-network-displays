
subdir('wfd')

gnome_nd_sources = [
  'main.c',
  'nd-window.c',
  'nd-codec-install.c',
  'nd-sink-list.c',
  'nd-sink-row.c',
  'nd-sink.c',
  'nd-provider.c',
  'nd-meta-sink.c',
  'nd-meta-provider.c',
  'nd-wfd-p2p-sink.c',
  'nd-wfd-p2p-provider.c',
  'nd-wfd-p2p-registry.c',
  'nd-dummy-provider.c',
  'nd-dummy-wfd-sink.c',
  'nd-screencast-portal.c',
  'nd-pulseaudio.c',
]

enum_headers = files('nd-sink.h')

gnome_nd_sources += gnome.mkenums_simple(
  'nd-enum-types',
  sources: enum_headers,
)

gnome_nd_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  dependency('libnm', version: '>= 1.15'),
  dependency('gstreamer-1.0', version: '>= 1.14'),
  dependency('gstreamer-pbutils-1.0', version: '>= 1.14'),
  dependency('libpulse-mainloop-glib'),
]

gnome_nd_deps += wfd_server_deps

gnome_nd_sources += gnome.compile_resources('gnome-nd-resources',
  'gnome-network-displays.gresource.xml',
  c_name: 'gnome_screencast'
)

executable('gnome-network-displays',
  gnome_nd_sources,
  dependencies: gnome_nd_deps,
  install: true,
  link_with: wfd_server,
)
