Note that all this is rather experimental.

This VS9 solution and the projects it includes are intented to be used
in a GTK+ source tree unpacked from a tarball. In a git checkout you
first need to use some Unix-like environment or manual work to expand
the files needed, like config.h.win32.in into config.h.win32 and the
.vcprojin files here into corresponding actual .vcproj files.

You will need the parts from below in the GTK+ stack: gdk-pixbuf, pango,
atk and glib. External dependencies are at least zlib, libpng,
proxy-libintl, fontconfig, freetype, expat.  See the corresponding
README.txt file in glib for details where to unpack them.

The "install" project will copy build results and headers into their
appropriate location under <root>\vs9\<PlatformName>. For instance,
built DLLs go into <root>\vs9\<PlatformName>\bin, built LIBs into
<root>\vs9\<PlatformName>\lib and GTK+ headers into
<root>\vs9\<PlatformName>\include\gtk-3.0. This is then from where
project files higher in the stack are supposed to look for them, not
from a specific GLib source tree.

--Tor Lillqvist <tml@iki.fi>
--Updated by Chun-wei Fan <fanc999 --at-- yahoo --dot-- com --dot-- tw>
