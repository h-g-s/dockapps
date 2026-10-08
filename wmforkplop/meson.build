project(
  'wmforkplop',
  'c',
  version: '0.9.3',
  meson_version: '>=0.60.0',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
  ],
)

cc = meson.get_compiler('c')
deps = [
  dependency('glib-2.0'),
  dependency('libgtop-2.0', version: '>=2.7.90'),
  dependency('imlib2'),
  dependency('x11'),
  dependency('xext'),
  cc.find_library('m', required: true),
]

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted(
  'PKGDATADIR',
  join_paths(get_option('prefix'), get_option('datadir'), 'wmforkplop'),
)
configure_file(output: 'config.h', configuration: conf)

executable(
  'wmforkplop',
  'wmforkplop.c',
  'util.c',
  'procstat.c',
  'dockapp_imlib2.c',
  c_args: ['-D_GNU_SOURCE'],
  dependencies: deps,
  install: true,
)

install_data(
  'Vera.ttf',
  'README',
  install_dir: join_paths(get_option('datadir'), 'wmforkplop'),
)
