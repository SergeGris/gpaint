project('gpaint', 'c',
          version: '0.1.0',
    meson_version: '>= 1.0.0',
  default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11' ],
)

# TODO i18n = import('i18n')
gnome = import('gnome')
cc = meson.get_compiler('c')

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('GETTEXT_PACKAGE', 'gpaint')
config_h.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))

# libadwaita (optional)
adwaita_dep = dependency('libadwaita-1', required: false)

# png_dep = dependency('libpng', required: false)
# jpeg_dep = dependency('libjpeg', required: false)
# tiff_dep = dependency('libtiff-4', required: false)
# avif_dep = dependency('libavif', required: false)

# # giflib: check header and library manually (without pkg-config)
# giflib_lib = cc.find_library('gif', required: false)
# if cc.has_header('gif_lib.h') and giflib_lib.found()
#   giflib_dep = declare_dependency(dependencies: [giflib_lib])
#   config_h.set('HAS_GIF', 1)
# else
#   giflib_dep = dependency('', required: false) # TODO
#   config_h.set('HAS_GIF', 0)
# endif

# Gather all optional dependencies together.
optional_deps = [] #[ png_dep, jpeg_dep, tiff_dep, avif_dep, giflib_dep ]

config_h.set('HAS_ADWAITA', adwaita_dep.found() ? 1 : 0)

# config_h.set('HAS_PNG',     png_dep.found()  ? 1 : 0)
# config_h.set('HAS_JPEG',    jpeg_dep.found() ? 1 : 0)
# config_h.set('HAS_TIFF',    tiff_dep.found() ? 1 : 0)
# config_h.set('HAS_AVIF',    avif_dep.found() ? 1 : 0)

configure_file(output: 'config.h', configuration: config_h)
add_project_arguments(['-I' + meson.project_build_root()], language: 'c')

project_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fno-strict-aliasing',
  ['-Werror=format-security', '-Werror=format=2'],
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    project_c_args += arg
  endif
endforeach
add_project_arguments(project_c_args, language: 'c')

# TODO subdir('data')
subdir('src')
# TODO subdir('po')

gnome.post_install(
     glib_compile_schemas: true,
    gtk_update_icon_cache: true,
  update_desktop_database: true,
)
