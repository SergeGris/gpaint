gpaint_sources = [
  'main.c',
  'color-swap-button.c',
  'drag-square.c',
  'backup.c',
  'tools/select-rectangle.c',
  'tools/freehand.c',
  'tools/bucket.c',
  'tools/eraser.c',
  'tools/picker.c',
  'tools/rectangle.c',
  'tools/ellipse.c',
  'tools/brush.c',
  'tools/line.c',
  'tools/tools-internal.c',
  'tools/tools-icons.c',
]

adwaita = dependency('libadwaita-1')

gpaint_deps = [
  dependency('gtk4'),
  adwaita,
  cc.find_library('m')
]

if adwaita.found()
   add_project_arguments(['-D' + 'HAS_LIBADWAITA=1'], language: 'c')
endif

png = dependency('libpng', required: false)
jpeg = dependency('libjpeg', required: false)

optional_deps = [ png, jpeg ]

if png.found()
   add_project_arguments(['-D' + 'HAS_PNG=1'], language: 'c')
endif

if jpeg.found()
   add_project_arguments(['-D' + 'HAS_JPEG=1'], language: 'c')
endif

# gpaint_sources += gnome.compile_resources('gpaint-resources',
#   'gpaint.gresource.xml',
#   c_name: 'gpaint'
# )

executable('gpaint', gpaint_sources,
  dependencies: gpaint_deps + optional_deps,
       install: true,
)
