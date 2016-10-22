ACLOCAL_AMFLAGS = -I m4

bin_SCRIPTS = libjustify-config

noinst_PROGRAMS = psset

lib_LTLIBRARIES = libjustify.la

libjustify_la_SOURCES = \
	hsjust.c \
	hqjust.c \
	namecontext.c \
	namecontext.h \
	parseAFM.c \
	parseAFM.h \
	z_misc.h

libjustifyincdir = $(includedir)/libjustify
libjustifyinc_HEADERS = \
	just.h \
	hsjust.h \
	hqjust.h

EXTRA_DIST = hyphen.mashed README.hyphen libjustify.m4

DEPS = $(top_builddir)/libjustify.la
LDADDS = $(top_builddir)/libjustify.la

psset_SOURCES=psset.c
psset_LDFLAGS = 
psset_DEPENDENCIES = $(DEPS)
psset_LDADD= $(LDADDS) \
	     -lm \
	     -lhyphen

tests: psset

m4datadir = $(datadir)/aclocal
m4data_DATA = libjustify.m4
