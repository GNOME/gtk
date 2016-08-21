subdir('parser')
subdir('nodes')
subdir('style')

test_api = executable('api', 'api.c', dependencies: libgtk_dep, link_with: libgtk)
test('css/api', test_api)
