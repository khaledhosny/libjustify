bin_SCRIPTS = libhnj-config

noinst_PROGRAMS = psset

lib_LTLIBRARIES = libhnj.la

libhnj_la_SOURCES = \
	hsjust.c \
	hqjust.c \
	hnjalloc.c \
	hnjalloc.h \
	namecontext.c \
	namecontext.h \
	parseAFM.c \
	parseAFM.h \
	z_misc.h

libhnjincdir = $(includedir)/libhnj
libhnjinc_HEADERS = \
	just.h \
	hsjust.h \
	hqjust.h

EXTRA_DIST = hyphen.mashed README.hyphen libhnj.m4

DEPS = $(top_builddir)/libhnj.la
LDADDS = $(top_builddir)/libhnj.la

psset_SOURCES=psset.c
psset_LDFLAGS = 
psset_DEPENDENCIES = $(DEPS)
psset_LDADD= $(LDADDS) \
	     -lm \
	     -lhyphen

tests: psset

m4datadir = $(datadir)/aclocal
m4data_DATA = libhnj.m4
