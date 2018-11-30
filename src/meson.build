
subdir('wfd')

gnome_screencast_sources = [
  'main.c',
  'gnome-screencast-window.c',
  'screencast-sink-list.c',
  'screencast-sink-row.c',
  'screencast-sink.c',
  'screencast-provider.c',
  'screencast-meta-sink.c',
  'screencast-meta-provider.c',
  'screencast-portal.c',
  'screencast-wfd-p2p-sink.c',
  'screencast-wfd-p2p-provider.c',
  'screencast-wfd-p2p-registry.c',
  'screencast-dummy-provider.c',
  'screencast-dummy-wfd-sink.c',
]

enum_headers = files('screencast-sink.h')

gnome_screencast_sources += gnome.mkenums_simple(
  'screencast-enum-types',
  sources: enum_headers,
)

gnome_screencast_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  dependency('libnm', version: '>= 1.15'),
  dependency('gstreamer-1.0', version: '>= 1.14'),
]

gnome_screencast_deps += wfd_server_deps

gnome_screencast_sources += gnome.compile_resources('gnome-screencast-resources',
  'gnome-screencast.gresource.xml',
  c_name: 'gnome_screencast'
)

executable('gnome-screencast',
  gnome_screencast_sources,
  dependencies: gnome_screencast_deps,
  install: true,
  link_with: wfd_server,
)
