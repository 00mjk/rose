include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

AM_CPPFLAGS = $(ROSE_INCLUDES)
AM_LDFLAGS = $(ROSE_RPATHS)
LDADD    = $(ROSE_LIBS)

noinst_LTLIBRARIES = libAstFromString.la
libAstFromString_la_SOURCES = AstFromString.cpp ParserBuilder.cpp

pkginclude_HEADERS = AstFromString.h ParserBuilder.hpp ParserBuilder.tpp

# EXTRA_DIST = AstFromString.h ParserBuilder.hpp ParserBuilder.tpp

clean-local:
	rm -f *.la

