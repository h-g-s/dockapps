project(
  'wmsystemtray',
  'c',
  version: '1.4',
  license: 'GPL-2.0-or-later',
  meson_version: '>=0.60.0',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
  ],
)

deps = [
  dependency('x11'),
  dependency('xext'),
  dependency('xfixes'),
  dependency('xpm'),
]

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: conf)

executable(
  'wmsystemtray',
  'fdtray.c',
  'wmsystemtray.c',
  dependencies: deps,
  install: true,
)

install_man('wmsystemtray.1')
