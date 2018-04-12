
# benchmark name, optional extra sources
gtk_benchmarks = [
  ['containers'],
  ['css'],
  ['widget'],
  ['renderers'],
]

foreach b : gtk_benchmarks
  b_name = b.get(0)
  b_sources = ['@0@.c'.format(b_name), b.get(1, [])]
  b_exec = executable (
    b_name,
    b_sources,
    include_directories: [confinc, gdkinc],
    dependencies: [libgtk_dep, libm]
  )
  benchmark(b_name, b_exec)

endforeach
