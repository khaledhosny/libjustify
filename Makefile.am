ACLOCAL_AMFLAGS = -I m4

noinst_PROGRAMS = psset

lib_LTLIBRARIES = libjustify.la

libjustify_la_SOURCES = \
	hsjust.c \
	hqjust.c

libjustifyincdir = $(includedir)/libjustify
libjustifyinc_HEADERS = \
	just.h \
	hsjust.h \
	hqjust.h

EXTRA_DIST = hyphen.mashed README.hyphen

DEPS = $(top_builddir)/libjustify.la
LDADDS = $(top_builddir)/libjustify.la

psset_SOURCES=psset.c
psset_LDFLAGS =
psset_CFLAGS = \
	     $(FREETYPE_CFLAGS) \
	     $(CAIRO_CFLAGS)
psset_DEPENDENCIES = $(DEPS)
psset_LDADD= $(LDADDS) \
	     -lm \
	     -lhyphen \
	     $(FREETYPE_LIBS) \
	     $(CAIRO_LIBS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libjustify.pc
EXTRA_DIST += libjustify.pc.in

%.pc: %.pc.in $(top_builddir)/config.status
	$(AM_V_GEN) \
	$(SED)	-e 's@%prefix%@$(prefix)@g' \
		-e 's@%exec_prefix%@$(exec_prefix)@g' \
		-e 's@%libdir%@$(libdir)@g' \
		-e 's@%includedir%@$(includedir)@g' \
		-e 's@%libs_private%@$(HBNONPCLIBS)@g' \
		-e 's@%requires_private%@$(HBDEPS)@g' \
		-e 's@%VERSION%@$(VERSION)@g' \
	"$<" > "$@" \
	|| ($(RM) "$@"; false)

CLEANFILES = $(pkgconfig_DATA)

tests: psset
