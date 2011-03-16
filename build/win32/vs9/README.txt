Note that all this is rather experimental.

This VS9 solution and the projects it includes are intented to be used
in a GTK+ source tree unpacked from a tarball. In a git checkout you
first need to use some Unix-like environment or manual work to expand
the files needed, like config.h.win32.in into config.h.win32 and the
.vcprojin files here into corresponding actual .vcproj files.

You will need the parts from below in the GTK+ stack: gdk-pixbuf,
pango, atk and glib. External dependencies are at least zlib, libpng,
a gettext/libintl implementation, cairo, plus libtiff, IJG libjpeg and
libjasper (JPEG-2000 library) if GDI+ is not used.
Freetype, fontconfig and expat are not used in GTK+ itself,
but may be used by GTK+-based applications such as GIMP via the usage
of Pango-Freetype and/or Cairo.
See the corresponding README.txt file in glib for details where to
unpack them.

The "install" project will copy build results and headers into their
appropriate location under <root>\vs9\<PlatformName>. For instance,
built DLLs go into <root>\vs9\<PlatformName>\bin, built LIBs into
<root>\vs9\<PlatformName>\lib and GTK+ headers into
<root>\vs9\<PlatformName>\include\gtk-2.0. This is then from where
project files higher in the stack are supposed to look for them, not
from a specific GLib source tree.

--Tor Lillqvist <tml@iki.fi>
--Updated by Fan, Chun-wei <fanc999@yahoo.com.tw>
