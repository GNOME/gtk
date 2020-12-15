if profiler_enabled
  if libsysprof_dep.found()
    test_performance = executable('test-performance',
      sources: 'test-performance.c',
      c_args: common_cflags,
      dependencies: [libsysprof_dep, platform_gio_dep, libm],
    )
  endif
endif
