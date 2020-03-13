if get_option ('profiler')
  test_performance = executable('test-performance', 'test-performance.c',
                                c_args: common_cflags,
                                dependencies: [profiler_dep, platform_gio_dep, libm])
endif
