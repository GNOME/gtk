gtk_libexecdir = join_paths(gtk_prefix, get_option('libexecdir'))
installed_test_bindir = join_paths(gtk_libexecdir, 'installed-tests', 'gtk-4.0')
installed_test_datadir = join_paths(gtk_datadir, 'installed-tests', 'gtk-4.0')

# We call diff in various tests, so we need to check it's available,
# otherwise we're going to have failures down the line
diff = find_program('diff', required: true)

subdir('gdk')
subdir('gsk')
subdir('gtk')
subdir('css')
subdir('tools')
subdir('reftests')
