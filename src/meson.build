gpaint_sources = [
  'main.c',
  'color-swap-button.c',
  'border-widget.c',
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
  'tools/drag.c',
  'tools/symmetric.c',
  'tools/tools-internal.c',
  'tools/tools-icons.c',
  'formats.c',
]

gpaint_deps = [
  dependency('gtk4'),
  cc.find_library('m')
]

gpaint_deps += dependency('libavutil')
gpaint_deps += dependency('libavcodec')
gpaint_deps += dependency('libavformat')
gpaint_deps += dependency('libswscale')

if adwaita_dep.found()
  gpaint_deps += adwaita_dep
endif

# # Add format-specific sources if the dependencies are found.
# if png_dep.found()
#   gpaint_sources += 'formats/png.c'
# endif
# if jpeg_dep.found()
#   gpaint_sources += 'formats/jpeg.c'
# endif
# if tiff_dep.found()
#   gpaint_sources += 'formats/tiff.c'
# endif
# if avif_dep.found()
#   gpaint_sources += 'formats/avif.c'
# endif
# if giflib_dep.found()
#   gpaint_sources += 'formats/gif.c'
# endif

executable('gpaint', gpaint_sources,
  dependencies: gpaint_deps + optional_deps,
       install: true,
)
