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
 ANGLE from QT-5.10.x, but the Visual Studio 2013-built ANGLE DLLs does work
 without problems with GTK+ built with Visual Studio 2008~2013.  You may
 need to obtain D3Dcompiler_[47|43|42].dll if it does not come with the
 system (which is part of the DirectX runtimes).  Its headers and .lib
 needs to be set to be found by the compiler and linker respectively before
 building libepoxy.
-Build libepoxy with EGL support, which has to be enabled explicitly on
 Windows builds.  Pass in -Degl=yes when building libepoxy using Meson.
 Build and install, making sure the headers and .lib can be located by the
 compiler and linker respectively.
-Open the vsX/gtk+.sln, and open the project properties in the "gdk3-win32"
 project.  Under "C/C++", add GDK_WIN32_ENABLE_EGL in the "Preprocessor
 Definitions" to the existing definitions in there for the configuration
 that is being built.  Then build the solution.
-To force the use of the EGL code, set the envvar GDK_GL=(...,)gles , where (...,)
 are the other GDK_GL options desired.
