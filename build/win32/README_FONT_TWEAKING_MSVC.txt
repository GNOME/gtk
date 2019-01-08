Notes on enabling FontTweaking support for Windows/Visual Studio builds
==========================================================================
There is now support in the Font Chooser code for Windows in GTK that would
support tweaking of fonts via setting specific OpenType tags.  This support,
as it involves more dependencies, is not enabled by default in the Visual
Studio project files.

You will need the following to enable this support:
-HarfBuzz 0.9 or later with FreeType support enabled
-FreeType 2.7.1 or later
-(Optional) Pango built with PangoFT2 support (this requires FontConfig
 and therefore libexpat or libxml2 in addition).

Shaping of the tweaking options is currently supported with HarfBuzz/PangoFT2,
but not the PangoWin32 fonts, where work is in progress to do that via Uniscribe,
and eventually HarfBuzz, in Pango.

To enable this support in the Visual Studio projects in GTK:
-Add HAVE_HARFBUZZ (and possibly HAVE_PANGOFT) to the "Preprocessor Definitions"
 under the "C/C++"->"Preprocessor" section of your desired build configuration
 in the "gtk-3" project properties.

-Add the following .lib files to the "Additional Dependencies" under the "Linker"->
 "Input" section in the "gtk-3" project properties:
 pangoft2-1.0.lib (if PangoFT2 is used via defining HAVE_PANGOFT in the above step)
 harfbuzz.lib
 freetype.lib

To build the font_features demo program in gtk3-demo:
-Run the regenerate-demos-h-win32.bat in this directory as follows (you need to
 have PERL installed and the PERL interpreter needs to be in your PATH in the
 cmd shell that you are running this .bat script):
 "regenerate-demos-h-win32.bat font" (running "regenerate-demos-h-win32.bat" removes
 the font features demo from the demo listing, which should restore demos.h.win32
 to be more or less identical to the copy that was shipped with this release tarball).

-Add $(srcroot)\demos\gtk-demo\font_features.c and $(srcroot)\gtk\gtkpangofontutils.c
 to the "Source Files" of the "gtk3-demo" project.

-Add HAVE_HARFBUZZ (and possibly HAVE_PANGOFT) to the "Preprocessor Definitions"
 under the "C/C++"->"Preprocessor" section of your desired build configuration
 in the "gtk3-demo" project properties.

-Add the following .lib files to the "Additional Dependencies" under the "Linker"->
 "Input" section in the "gtk-3" project properties:
 pangoft2-1.0.lib (if PangoFT2 is used via defining HAVE_PANGOFT in the above step)
 pangowin32-1.0.lib
 harfbuzz.lib
 freetype.lib