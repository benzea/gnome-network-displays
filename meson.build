project('gnome-network-displays', 'c',        version: '0.1.0',
  meson_version: '>= 0.42.0',
)

gnome = import('gnome')
i18n = import('i18n')

datadir = get_option('datadir')
icondir = join_paths(datadir, 'icons')

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('GETTEXT_PACKAGE', 'gnome-network-displays')
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
configure_file(
  output: 'gnome-network-displays-config.h',
  configuration: config_h,
)
add_project_arguments([
  '-I' + meson.build_root(),
], language: 'c')

subdir('data')
subdir('src')
subdir('po')

meson.add_install_script('build-aux/meson/postinstall.py')
