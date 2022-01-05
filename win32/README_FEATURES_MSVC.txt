Preameble
=========
This file attempts to give further info about how to enable features
that are not available in the Visual Studio project files shipped
with the source release archive, i.e. beyond building GTK with the GDK 
Win32 backend, with or without the Broadway GDK backend.

The following also apply to Visual Studio builds done with Meson in terms
of getting the required dependencies for the optional features.

==========================================================================
Notes on enabling EGL (ANGLE/D3D support) for Windows/Visual Studio builds
==========================================================================
There is now support in the GL context creation code for Windows in GDK for
creating and using EGL (OpenGL ES 3) contexts, which can be used instead of
the existing OpenGL (Desktop) support, especially when the graphics drivers
do not support OpenGL adequately.

This support is not enabled by default in the project files.  In order to do
so, please do the following:

-Obtain or compile a build of recent version of ANGLE.  The one that comes
 with QT 5.10.x is sufficiently recent, but not the one that comes with QT-
 5.6.x.  Note that Visual Studio 2013 or later is required for building
 ANGLE from QT-5.10.x, but the Visual Studio 2013-built ANGLE DLLs does 
 work  without problems with GTK+ built with Visual Studio 2008~2013.  
 You may need to obtain D3Dcompiler_[47|43|42].dll if it does not come 
 with the system (which is part of the DirectX runtimes).  Visual Studio 
 2015 or later can use ANGLE from QT 5.11.x or later, or from Google's
 GIT repos, which may require later version of Visual Studio to build.
 Its headers and .lib needs to be set to be found by the compiler and 
 linker   respectively before building libepoxy.
-Build libepoxy with EGL support, which has to be enabled explicitly on
 Windows builds.  Pass in -Degl=yes when building libepoxy using Meson.
 Build and install, making sure the headers and .lib can be located by the
 compiler and linker respectively.
-Open the vsX/gtk+.sln, and open the project properties in the "gdk3-win32"
 project.  Under "C/C++", add GDK_WIN32_ENABLE_EGL in the "Preprocessor
 Definitions" to the existing definitions in there for the configuration
 that is being built.  Then build the solution.
-To force the use of the EGL code, set the envvar GDK_GL=(...,)gles , 
 where (...,) are the other GDK_GL options desired.
 
==============================================================
Enabling the font tweaking features and the font features demo
==============================================================
The font tweaking features in the GTK DLL is enabled automatically if
the Pango 1.44.0 and HarfBuzz 2.2.0 (or later) headers and libraries
(and hence DLLs) are found during compile time.  Check in 
gtkfontchooserwidget.c that the `#pragma comment(lib, "harfbuzz")` line
to ensure that you have your HarfBuzz .lib file named as such, which
is the default .lib name for HarfBuzz builds.

Alternatively, they can be manually enabled by making sure that 
`HAVE_HARFBUZZ` and `HAVE_PANGOFT2` are defined in config.h.win32,
meaning that PangoFT2 must be present, which depends on HarfBuzz, 
FontConfig and FreeType.  You will then need to add to the `gtk3`
projects the .lib's of PangoFT2, HarfBuzz and FreeType in the 
"Additional Libraries" entry under the linker settings.

Please note that the font features demo is not built into gtk3-demo
by default.  To do that, run in a Visual Studio command prompt, go to
$(srcroot)\win32, and run
"nmake /f generate-msvc.mak regenerate-demos-h-win32 FONT_FEATURES_DEMO=1".
To undo that, run that command without "FONT_FEATURES_DEMO=1".  Python must
be present in your PATH or passed in via PYTHON=<path_to_python_interpreter>.
If you are building the font features demo with the older PangoFT2-style
(i.e. pre-Pango-1.44.x and pre-HarfBuzz-2.2.0) support, pass in 
"FONT_FEATURES_USE_PANGOFT2=1" in addition to "FONT_FEATURES_DEMO=1" in the 
NMake command line.  The gtk3-demo project files will also be updated with the 
appropriate dependent libraries linked in-please check that the project settings 
contain the correct .lib file names for your system, as they assume the most 
common names are used there.
