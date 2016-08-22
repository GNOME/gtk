
cssresources = gnome.compile_resources(
  'cssresources',
  'test-css-style.gresource.xml',
  source_dir: '.',
)

test_style = executable(
  'test-css-style',
  'test-css-style.c',
  cssresources,
  dependencies: libgtk_dep,
)
test('css/style/test-style', test_style)
