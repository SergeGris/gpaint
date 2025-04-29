project('gpaint', 'c',
          version: '0.1.0',
    meson_version: '>= 1.0.0',
  default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu99' ],
)

# TODO i18n = import('i18n')
#gnome = import('gnome')
cc = meson.get_compiler('c')

source_root = meson.current_source_dir()
project_build_root = meson.project_build_root()

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('GETTEXT_PACKAGE', 'gpaint')
config_h.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))

# libadwaita (optional)
adwaita_dep = dependency('libadwaita-1', required: false)
config_h.set('HAVE_ADWAITA', adwaita_dep.found() ? 1 : 0)

x11_dep = dependency('xcursor', required: false)
config_h.set('HAVE_XCURSOR', x11_dep.found() ? 1 : 0)
wayland_dep = dependency('wayland-cursor', required: false)
config_h.set('HAVE_WAYLAND_CURSOR', wayland_dep.found() ? 1 : 0)

ffmpeg_deps = [
  dependency('libavutil', required: false),
  dependency('libavcodec', required: false),
  dependency('libavformat', required: false),
  dependency('libswscale', required: false),
]

have_ffmpeg = 1
foreach dep: ffmpeg_deps
  if not dep.found()
    have_ffmpeg = 0
    break
  endif
endforeach

config_h.set('HAVE_FFMPEG', have_ffmpeg)

configure_file(output: 'config.h', configuration: config_h)
add_project_arguments(['-I' + project_build_root, '-DHAVE_CONFIG_H=1'], language: 'c') # TODO

add_project_arguments(['-pg'], language: 'c')
add_project_link_arguments(['-pg'], language: 'c')

add_project_arguments(['-fanalyzer'], language: 'c')
add_project_link_arguments(['-fanalyzer'], language: 'c')

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
# TODO  ['-Werror=format-security', '-Werror=format=2'],
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

gpaint_deps = [
  dependency('gtk4'),
  cc.find_library('m')
]

if have_ffmpeg != 0
  gpaint_deps += ffmpeg_deps
endif

if adwaita_dep.found()
  gpaint_deps += adwaita_dep
endif

if x11_dep.found()
  gpaint_deps += x11_dep
endif

if wayland_dep.found()
  gpaint_deps += wayland_dep
endif

# TODO subdir('data')
subdir('src')
# TODO subdir('po')

#gnome.post_install(
#     glib_compile_schemas: true,
#    gtk_update_icon_cache: true,
#  update_desktop_database: true,
#)

# TODO
summary({
  'Source code location': source_root,
  'Project build root': project_build_root,
  'Compiler': cc.get_id(),
  'Extra compiler warnings': project_c_args,
}, section: 'Build')
