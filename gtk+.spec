# Note that this is NOT a relocatable package
%define ver      0.99.5
%define rel      SNAP
%define prefix   /usr

Summary: The Gimp Toolkit
Name: gtk+
Version: %ver
Release: %rel
Copyright: LGPL
Group: X11/Libraries
Source: ftp://ftp.gimp.org/pub/gtk/gtk+-%{ver}.tar.gz
BuildRoot: /tmp/gtk-root
Obsoletes: gtk
Packager: Marc Ewing <marc@redhat.com>
URL: http://www.gimp.org/gtk
Prereq: /sbin/install-info
Docdir: %{prefix}/doc

%description
The X libraries originally written for the GIMP, which are now used by
several other programs as well.

%package devel
Summary: GIMP Toolkit and GIMP Drawing Kit
Group: X11/Libraries
Requires: gtk+
Obsoletes: gtk-devel
PreReq: /sbin/install-info

%description devel
Static libraries and header files for the GIMP's X libraries, which are
available as public libraries.  GLIB includes generally useful data
structures, GDK is a drawing toolkit which provides a thin layer over
Xlib to help automate things like dealing with different color depths,
and GTK is a widget set for creating user interfaces.

%changelog

* Thu Mar 12 1998 Marc Ewing <marc@redhat.com>

- Reworked to integrate into gtk+ source tree

- Truncated ChangeLog.  Previous Authors:
  Trond Eivind Glomsrod <teg@pvv.ntnu.no>
  Michael K. Johnson <johnsonm@redhat.com>
  Otto Hammersmith <otto@redhat.com>
  
%prep
%setup

%build
# Needed for snapshot releases.
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%prefix
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

gzip -9n $RPM_BUILD_ROOT%{prefix}/info/*

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%post devel
/sbin/install-info %{prefix}/info/gdk.info.gz %{prefix}/info/dir
/sbin/install-info %{prefix}/info/gtk.info.gz %{prefix}/info/dir

%postun devel
if [ $1 = 0 ]; then
    /sbin/install-info --delete %{prefix}/info/gdk.info.gz %{prefix}/info/dir
    /sbin/install-info --delete %{prefix}/info/gtk.info.gz %{prefix}/info/dir
fi

%files
%attr(-, root, root) %doc AUTHORS COPYING ChangeLog NEWS README TODO
%attr(-, root, root) %{prefix}/lib/lib*.so.*

%files devel
%attr(-, root, root) %{prefix}/lib/lib*.so
%attr(-, root, root) %{prefix}/lib/*a
%attr(-, root, root) %{prefix}/include/*
%attr(-, root, root) %{prefix}/info/*
