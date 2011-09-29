# Note that this is NOT a relocatable package
%define ver      0.1.1
%define rel      1
%define prefix   /usr

Summary: libhnj - Hyphenation and Justification libraries.
Name: libhnj
Version: %ver
Release: %rel
Copyright: LGPL or MPL
Group: X11/gnome
Source: ftp://ftp.gnome.org/pub/GNOME/sources/libhnj/libhnj-%{ver}.tar.gz
BuildRoot: /var/tmp/libhnj-root
Packager: Chris Lahey <clahey@umich.edu>

%description
Hyphenation and Justification libraries.

%package devel
Summary: Hyphenation and Justification libraries.
Group: X11/gnome

%description devel
Hyphenation and Justification libraries.

%changelog

%prep
%setup

%build
# Needed for snapshot releases.
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%prefix --localstatedir=/var/lib
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix --localstatedir=/var/lib
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%post 
if ! grep %{prefix}/lib /etc/ld.so.conf > /dev/null ; then
  echo "%{prefix}/lib" >> /etc/ld.so.conf
fi

/sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)

%doc AUTHORS COPYING ChangeLog NEWS README
%{prefix}/lib/lib*.so.*

%files devel
%defattr(-, root, root)

%{prefix}/bin/*
%{prefix}/lib/lib*.so
%{prefix}/lib/*.a
%{prefix}/lib/*.la
%{prefix}/include/*/*
%{prefix}/share/*/*

