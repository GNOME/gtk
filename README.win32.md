Notes on running GTK on Windows in general
===
The Win32 backend in GTK+ is not as stable or correct as the X11 one.

For prebuilt runtime and developer packages see
http://ftp.gnome.org/pub/gnome/binaries/win32/

Notes on using OpenGL (GtkGLArea/GdkGLArea) on Win32
===
Note that on Windows, if one is running Nahimic 3 on a system with
nVidia graphics, one needs to stop the "Nahimic service" or insert
the GTK application into the Nahimic blacklist, as noted in
https://www.nvidia.com/en-us/geforce/forums/game-ready-drivers/13/297952/nahimic-and-nvidia-drivers-conflict/2334568/
if using programs that utilise GtkGLArea and/or GdkGLArea, or use
GDK_GL=gles if you know that GLES support is enabled for the build.

This is a known issue, as the above link indicates, and affects quite
a number of applications--sadly, since this issue lies within the
nVidia graphics driver and/or the Nahimic 3 code, we are not able
to rememdy this on the GTK side; the best bet before trying the above
workarounds is to try to update your graphics drivers and Nahimic
installation.

Building GTK+ on Win32
===

First you obviously need developer packages for the compile-time
dependencies: `GDK-Pixbuf`, `Pango`*, `HarfBuzz`**, `atk`, `cairo`* and `glib`.
You will also need `libffi`, `gettext-runtime`, `libiconv` and PCRE (or PCRE2
for glib-2.74.x and later) and `zlib` for GLib; Cairo with DirectWrite support
and/or FontConfig support for best font shaping and display supportin Pango*;
and `librsvg`, `libpng`, `libjpeg-turbo` and `libtiff` for loading the
various icons via GDK-Pixbuf that are common to GTK.  You will need a Rust
installation with the appropriate toolchain installed as well, if building
librsvg-2.42.x or later.

(MinGW users should also look at the following section on the dependencies
that are required, either built from source or installed using `pacman`.

Notes on building with Visual Studio
===

You may wish to build the dependencies from the sources (all are required
for the best use experience unless noted).

For Visual Studio, it is possible to build the following with CMake:

*  zlib
*  libpng
*  FreeType (used in FontConfig, optionally used in Cairo and HarfBuzz)
*  libexpat (used in FontConfig)
*  libxml2 (needed for GResource support during build time and librsvg/libcroco)
*  libbrotlidec (optional, used in FreeType, requires Visual Studio 2013 or later)
*  libjpeg-turbo (you also need NASM, unless building for ARM64)
*  libtiff (requires libjpeg-turbo and zlib)
*  HarfBuzz** (for pre-2.6.0, using Meson is recommended for 2.6.0 or later)
*  PCRE (for glib-2.72.x and earlier), or PCRE2 (for glib-2.74.x or later)

It is possible to build the following items using Meson:
*  HarfBuzz** (2.6.0 and later)
*  Cairo (1.17.x or later; for 1.16.x, you need mozilla-build to build from the MSVC Makefiles,
   building cairo-gobject is required)
*  FontConfig (needed if PangoFT2 is used.  Note building Cairo with FontConfig is required,
   requires Visual Studio 2015 or later)
*  fribidi (required for Pango)
*  GLib, ATK, Pango, GDK-Pixbuf
*  gobject-introspection (recommended, if using language bindings or gedit is desired, requires GLib and libffi)
*  pixman (required for Cairo)
*  libepoxy***

For Visual Studio, Visual Studio projects or NMake Makefiles are provided with the following:
*  librsvg (runtime, 2.42.x or later require Visual Studio 2013 or later with a Rust
   MSVC toolchain installed; requires libxml2)
*  libcroco (required for librsvg-2.40.x or earlier, requires libxml2)
*  libbz2 (optional for FreeType)
*  nasm (needed for building libjpeg-turbo on x86/x64)
*  adwaita-icon-theme (run-time, after building GTK and librsvg)

NMake Makefiles are provided as an add-on with patches to build the sources,
at https://github.com/fanc999/gtk-deps-msvc/, under $(dependency) / $(dep_version)
*  libiconv (used by gettext-runtime)
*  gettext-runtime (and gettext-tools; an alternative is to use proxy-intl during the GLib
   build, at the cost of not having translations being built, VS2015 or later is required for
   0.21.1 and later)
*  libffi (currently, manually adapting the pkg-config .pc.in template is needed; an older
   x86/x64 version can be built in-place if building GLib without libffi installed)

You also need a copy of stdint.h and inttypes.h from msinttypes for Visual Studio 2012
or earlier (stdint.h is optional on VS2010 or later), as well as an implementation of
stdbool.h.

Bleeding-edge versions of the dependencies may require Visual Studio 2015/2017 or later.

ARM64 builds are supported in addition to x86 and x64 builds, albeit without SIMD optimizations
in pixman and libjpeg-turbo (SIMD support may need to be explicitly disabled).  Please see
the Meson documentation on how to set up a cross-build from an x86-based Windows system.
Introspection support is not supported in this configuration.

Building just using Meson without the dependencies installed may work if the following
conditions are met:

* Visual Studio 2017 15.9.x or later is installed
* `git` is accessible in the `%PATH%`, to pull in the depedencies
* The CMake-built dependencies should be pre-built.
* Only building for x86/x64 is supported this way, ARM64 builds should at least have
  pixman and libffi prebuilt.
* librsvg and adwaita-icon-theme must be built separately
* gettext-runtime and libiconv must also be prebuilt if translations support is desired.

Notes on certain dependencies:
---

*  (*)DirectWrite support in Pango requires pango-1.50.12 or later with Cairo
   1.17.6 or later.  Visual Studio 2015 or later is required to build Pango
   1.50.x or later.
* (**)HarfBuzz is required if using Pango-1.44.x or later, or if building
   PangoFT2.  If using Visual Studio 2013 or earlier, only HarfBuzz 2.4.0
   or earlier is supported.  Visual Studio 2017 15.9.x or later is required
   for 3.3.0 or later.  You may wish to build FreeType prior to building
   HarfBuzz, and then building FreeType again linking to HarfBuzz for a
   more comprehensive FreeType build.  Font features support is only enabled
   if PangoFT2 is built or Pango-1.44.x and HarfBuzz 2.2.0 or later are installed.
*  (***)For building with GLES support (currently supported via libANGLE), you
   will need to obtain libANGLE from its latest GIT checkout or from QT 5.10.x.
   You will need to build libepoxy with EGL enabled using `-Degl=yes` when
   configuring the build.

Some outdated builds of the dependencies may be found at
http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies .

For people compiling GTK+ with Visual C++, it is recommended that
the same compiler is used for at least GDK-Pixbuf, Pango, atk and glib
so that crashes and errors caused by different CRTs can be avoided. 

For Visual Studio 2008 and 2010, a special setup making use of the Windows 
8.0 SDK is required, see at the bottom of this document for guidance.
Interchanging between Visual Studio 2015, 2017, 2019 and 2022 builds 
should be fine as they use the same CRT (UCRT) DLLs.

The following describes how one can build GTK with MinGW or Visual Studio
2008 or later using Meson.

Using Meson (for Visual Studio and MinGW builds)
===

Meson can now be used to build GTK+-3.x with either MinGW or Visual Studio.
You will need the following items in addition to all the dependencies
listed above:

* Python 3.6.x or later (later Meson versions require Python 3.7.x)
* Meson build system, 0.60.0 or later
* Ninja (if not using the Visual Studio project generator for
         Visual Studio 2010 or later)
* CMake (recommended for Visual Studio builds, used for dependency searching)
* pkg-config (or some compatible tool, highly recommended)

For all Windows builds, note that unless `-Dbuiltin_immodules=no` is 
specified, the input modules (immodules) are built directly into the GTK 
DLL.

For building with Meson using Visual Studio, do the following:

* Create an empty build directory somewhere that is on the same drive
  as the source tree, and launch the Visual Studio command prompt that
  matches the build configuration (Visual Studio version and architecture),
  and run the following:

* Ensure that both the installation directory of Python 3.6+ and its script
  directory is in your `%PATH%`, as well as the Ninja, CMake and pkg-config
  executables (if used).  If a pkg-config compatible drop-in replacement
  tool is being used, ensure that `PKG_CONFIG` is set to point to the
  executable of that tool as well.

* For non-GNOME dependencies (such as Cairo and Harfbuzz), where pkg-config
  files or CMake files may not be properly located, set `%INCLUDE%` and 
  `%LIB%` to ensure that their header files and .lib files can be found 
  respectively. The DLLs of those dependencies should also be in the 
  `%PATH%` during the build as well, especially if introspection files ar
  to be built.

* For GNOME dependencies, the pkg-config files for those dependencies 
  should be searchable by `pkg-config` (or a compatible tool).  Verify
  this by running `$(PKG_CONFIG) --modversion <dependency>`.

* Run the following:
  `meson <path_to_directory_of_this_file> --buildtype=... --prefix=...,
  where `buildtype` can be:

  * release
  * debugoptimized
  * debug
  * plain.

  Please refer to the Meson documentation for more details.  You may also 
  wish to pass in `-Dbroadway_backend=true` if building the Broadway GDK 
  backend is desired, and/or pass in `-Dbuiltin_immodules=no` to build the
  immodules as standalone DLLs that can be loaded by GTK dynamically.  For
  Visual Studio 2010 or later builds, you may pass in --backend=vs to 
  generate Visual Studio project files to be used to carry out the builds.

  If you are building with Visual Studio 2008, note the following items as
  well:

   * For x64 builds, the compiler may hang when building the certain 
     files, due to optimization issues in the compiler.  If this happens, 
     use the Windows Task Manager and terminate all `cl.exe` processes, 
     and the build will fail with the source files that did not finish 
     compiling due to the hang. Look for them in build.ninja in the build 
     directory, and change their compiler
     flag `/O2` to `/O1`, and the compilation and linking should proceed 
	 normally.

   * At this time of writing, the following files are known to cause this 
     hang:

      * gtk\gtkfilechoosernativewin32.c
      * gtk\gtkfilesystemmodel.c
      * gtk\gtktextsegment.c
      * gtk\gtktextbtree.c
      * gtk\gtkrbtree.c
      * testsuite\gtk\treemodel.c
      * testsuite\gtk\textbuffer.c
      * testsuite\gtk\rbtree.c
      * testsuite\gtk\icontheme.c
   *  Upon running install (via "ninja install"), it is likely that
      `gtk-query-immodules-3.0.exe` will fail to run as it cannot find 
      `msvcr90.dll` or `msvcr90D.dll`.  You can ignore this if you did not
      specify `-Dbuiltin_immodules=no` when configuring via Meson.
      If `-Dbuiltin_immodules=no` is specified, you need to run the 
      following after embedding the manifests as outlined in the next 
      point:
    `$(gtk_install_prefix)\bin\gtk-query-immodules-3.0.exe > $(gtk_install_prefix)\lib\gtk-3.0\3.0.0\immodules.cache`

   *  You will need to run the following upon completing install, from the 
      build directory in the Visual Studio 2008/SDK 6.0 command prompt 
      (third line is not needed unless `-Dbuiltin_immodules=no` is 
      specified) so that the built binaries can run:
```
      for /r %f in (*.dll.manifest) do if exist $(gtk_install_prefix)\bin\%~nf mt /manifest %f /outputresource:$(gtk_install_prefix)\bin\%~nf;2
      for /r %f in (*.exe.manifest) do if exist $(gtk_install_prefix)\bin\%~nf mt /manifest %f /outputresource:$(gtk_install_prefix)\bin\%~nf;1
      for /r %f in (*.dll.manifest) do if exist $(gtk_install_prefix)\lib\gtk-3.0\3.0.0\immodules\%~nf mt /manifest %f /outputresource:$(gtk_install_prefix)\lib\gtk-3.0\3.0.0\immodules\%~nf;2
```

   * The more modern visual style for the print dialog is not applied for 
     Visual Studio 2008 builds.  Any solutions to this is really 
     appreciated.

Support for all pre-2012 Visual Studio builds
---

This release of GTK+ requires at least the Windows 8.0 or later SDK in 
order to be  built successfully using Visual Studio, which means that 
building with Visual Studio 2008 or 2010 is possible only with a special 
setup and must be done in the command line with Ninja, if using Meson.  
Please see
https://devblogs.microsoft.com/cppblog/using-the-windows-software-development-kit-sdk-for-windows-8-consumer-preview-with-visual-studio-2010/
for references; basically, assuming that your Windows 8.0 SDK is installed 
in `C:\Program Files (x86)\Windows Kits\8.0` (`$(WIN8SDKDIR)` in short), 
you need to ensure the following before invoking Meson to configure the build.  Your project files or Visual Studio IDE must also be similarly
configured (using the Windows 8.1 SDK is also possible for Visual Studio 
2008~2012, replacing `$(WIN8SDKDIR)` with `$(WIN81SDKDIR)`, which is in 
`C:\Program Files (x86)\Windows Kits\8.1` unless otherwise indicated):

*  Your `%INCLUDE%` (i.e. "Additional Include Directories" in the IDE) 
   must not include the Windows 7.0/7.1 SDK include directories,
   and `$(WIN8SDKDIR)\include\um`, `$(WIN8SDKDIR)\include\um\share` and
   `$(WIN8SDKDIR)\include\winrt` (in this order) must be before your stock
    Visual Studio 2008/2010 header directories.  If you have the DirectX 
	SDK (2010 June or earlier) installed, you should remove its include 
	directory from your `%INCLUDE%` as well.
*  You must replace the Windows 7.0/7.1 SDK library directory in `%LIB%` 
   (i.e. "Additional Library Paths" in the IDE) with the Windows 8.0/8.1 
   SDK library directory, i.e. `$(WIN8SDKDIR)\lib\win8\um\[x86|x64]` or 
   `$(WIN81SDKDIR)\lib\winv6.3\um\[x86|x64]`.
   If you have the DirectX SDK installed, you should remove its library 
   directory from your `%LIB%` as well.
*  You must replace the Windows 7.0/7.1 SDK tools directory from your 
   `%PATH%` ("Executables Directories" in the IDE) with the Windows 8.0 
   SDK tools directory, i.e.  `$(WIN8SDKDIR)\bin\[x86|x64]`. If you have 
   the DirectX SDK installed, you should remove its utility directory from
   your `%PATH%` as well.

*  The Windows 8.0/8.1 SDK headers may contain an `roapi.h` that cannot be 
   used under plain C, so to remedy that, change the following lines
   (around lines 55-57):

```
  // RegisterActivationFactory/RevokeActivationFactory registration cookie
  typedef struct {} *RO_REGISTRATION_COOKIE;
  // RegisterActivationFactory/DllGetActivationFactory callback
```

to

```
  // RegisterActivationFactory/RevokeActivationFactory registration cookie
  #ifdef __cplusplus
  typedef struct {} *RO_REGISTRATION_COOKIE;
  #else
  typedef struct _RO_REGISTRATION_COOKIE *RO_REGISTRATION_COOKIE; /* make this header includable in C files */
  #endif
  // RegisterActivationFactory/DllGetActivationFactory callback
```

This follows what is done in the Windows 8.1 SDK, which contains an 
`roapi.h` that is usable under plain C.  Please note that you might need 
to copy that file into a location that is in your `%INCLUDE%` which 
precedes the include path for the Windows 8.0 SDK headers, if you do not 
have administrative privileges.

Visual Studio 2008 hacks
---
(Please see the section on Meson builds which touch on this topic)

Multi-threaded use of GTK+ on Win32
---

Multi-threaded GTK+ programs might work on Windows in special simple
cases, but not in general. Sorry. If you have all GTK+ and GDK calls
in the same thread, it might work. Otherwise, probably not at
all. Possible ways to fix this are being investigated.

   * Tor Lillqvist <tml@iki.fi>, <tml@novell.com>
   * Updated by Fan, Chun-wei <fanc999@yahoo.com.tw>
