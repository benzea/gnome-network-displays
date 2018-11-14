gnome_screencast_sources = [
  'main.c',
  'gnome-screencast-window.c',
]

gnome_screencast_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
]

gnome = import('gnome')

gnome_screencast_sources += gnome.compile_resources('gnome-screencast-resources',
  'gnome-screencast.gresource.xml',
  c_name: 'gnome_screencast'
)

executable('gnome-screencast', gnome_screencast_sources,
  dependencies: gnome_screencast_deps,
  install: true,
)
