fs = import('fs')

stylesheet_deps = []

# For git checkouts, but not for tarballs...
if not fs.exists('base.css')
  sassc = find_program('sassc', required: false)
  if not sassc.found()
    subproject('sassc', default_options: ['warning_level=0', 'werror=false'])
    sassc = find_program('sassc')
  endif

  if sassc.found()
    sassc_opts = [ '-a', '-M', '-t', 'compact' ]

    scss_deps = files([
      '_colors.scss',
      '_common.scss',
      '_compat-colors.scss',
      '_defaults.scss',
      '_drawing.scss',
      '_functions.scss',
      '_widgets.scss',

      'widgets/_avatar.scss',
      'widgets/_bottom-sheet.scss',
      'widgets/_buttons.scss',
      'widgets/_calendar.scss',
      'widgets/_checks.scss',
      'widgets/_color-chooser.scss',
      'widgets/_column-view.scss',
      'widgets/_deprecated.scss',
      'widgets/_dialogs.scss',
      'widgets/_dropdowns.scss',
      'widgets/_emoji-chooser.scss',
      'widgets/_entries.scss',
      'widgets/_expanders.scss',
      'widgets/_file-chooser.scss',
      'widgets/_header-bar.scss',
      'widgets/_labels.scss',
      'widgets/_level-bar.scss',
      'widgets/_linked.scss',
      'widgets/_links.scss',
      'widgets/_lists.scss',
      'widgets/_menus.scss',
      'widgets/_message-dialog.scss',
      'widgets/_misc.scss',
      'widgets/_notebook.scss',
      'widgets/_paned.scss',
      'widgets/_popovers.scss',
      'widgets/_preferences.scss',
      'widgets/_progress-bar.scss',
      'widgets/_scale.scss',
      'widgets/_scrolling.scss',
      'widgets/_shortcuts-window.scss',
      'widgets/_sidebars.scss',
      'widgets/_spinner.scss',
      'widgets/_spin-button.scss',
      'widgets/_switch.scss',
      'widgets/_tab-view.scss',
      'widgets/_text-selection.scss',
      'widgets/_toolbars.scss',
      'widgets/_tooltip.scss',
      'widgets/_views.scss',
      'widgets/_view-switcher.scss',
      'widgets/_window.scss',
    ])

    scss_files = [
      'base',
      'base-hc',
      'defaults-light',
      'defaults-dark',
    ]

    foreach scss: scss_files
      stylesheet_deps += custom_target('@0@.scss'.format(scss),
        input: '@0@.scss'.format(scss),
        output: '@0@.css'.format(scss),
        command: [
          sassc, sassc_opts, '@INPUT@', '@OUTPUT@',
        ],
        depend_files: scss_deps,
      )
    endforeach
  endif
endif

libadwaita_stylesheet_resources = gnome.compile_resources(
  'adwaita-stylesheet-resources',
  'adwaita-stylesheet.gresources.xml',

  source_dir: [
    # List in order of preference
    meson.current_build_dir(),
    meson.current_source_dir(),
  ],
  dependencies: stylesheet_deps,
  c_name: 'adw_stylesheet',
)
