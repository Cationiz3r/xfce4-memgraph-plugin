plugindir = $(libdir)/xfce4/panel/plugins
plugin_LTLIBRARIES = libmemgraph.la

libmemgraph_la_CFLAGS =							\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	@LIBXFCE4UI_CFLAGS@							\
	@LIBXFCE4PANEL_CFLAGS@

libmemgraph_la_LIBADD =							\
	@LIBXFCE4UI_LIBS@							\
	@LIBXFCE4PANEL_LIBS@						\
	-lm

libmemgraph_la_SOURCES =						\
	mode.c										\
	mode.h										\
	mem.c										\
	mem.h										\
	os.c										\
	os.h										\
	properties.c								\
	properties.h								\
	settings.c									\
	settings.h

libmemgraph_la_LDFLAGS =	\
	-avoid-version	\
	-module	\
	-no-undefined	\
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)'	\
	$(PLATFORM_LDFLAGS)

#
# .desktop file
#
desktop_in_files = memgraph.desktop.in

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(desktop_in_files)

DISTCLEANFILES = $(desktop_DATA)
