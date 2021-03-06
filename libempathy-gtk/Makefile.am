AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(EMPATHY_CFLAGS)				\
	$(ENCHANT_CFLAGS)				\
	$(WARN_CFLAGS)

BUILT_SOURCES = 					\
	empathy-gtk-marshal.h				\
	empathy-gtk-marshal.c				\
	empathy-gtk-marshal.list			\
	empathy-gtk-enum-types.h			\
	empathy-gtk-enum-types.c

lib_LTLIBRARIES = libempathy-gtk.la

libempathy_gtk_la_SOURCES =             	\
	empathy-account-chooser.c		\
	empathy-chat.c				\
	empathy-irc-network-dialog.c		\
	empathy-spell-dialog.c			\
	empathy-accounts-dialog.c		\
	empathy-chat-view.c			\
	empathy-log-window.c			\
	empathy-theme-boxes.c			\
	empathy-account-widget.c		\
	empathy-conf.c				\
	empathy-theme.c				\
	empathy-account-widget-irc.c		\
	empathy-account-widget-sip.c		\
	empathy-contact-dialogs.c		\
	empathy-new-message-dialog.c		\
	empathy-theme-irc.c			\
	empathy-avatar-chooser.c		\
	empathy-contact-list-store.c		\
	empathy-presence-chooser.c		\
	empathy-theme-manager.c			\
	empathy-avatar-image.c			\
	empathy-contact-list-view.c		\
	empathy-ui-utils.c			\
	empathy-cell-renderer-activatable.c	\
	empathy-contact-widget.c		\
	empathy-profile-chooser.c		\
	empathy-cell-renderer-expander.c	\
	empathy-geometry.c			\
	empathy-smiley-manager.c		\
	empathy-cell-renderer-text.c		\
	empathy-spell.c				\
	empathy-contact-menu.c			\
	totem-subtitle-encoding.c totem-subtitle-encoding.h

# do not distribute generated files
nodist_libempathy_gtk_la_SOURCES =\
	$(BUILT_SOURCES)

libempathy_gtk_la_LIBADD =			\
	$(EMPATHY_LIBS)				\
	$(ENCHANT_LIBS)				\
	$(top_builddir)/libempathy/libempathy.la

libempathy_gtk_la_LDFLAGS =                \
       -version-info ${LIBEMPATHY_GTK_CURRENT}:${LIBEMPATHY_GTK_REVISION}:${LIBEMPATHY_GTK_AGE} \
       -export-symbols-regex ^empathy_

libempathy_gtk_headers =			\
	empathy-images.h			\
	empathy-account-chooser.h		\
	empathy-chat.h				\
	empathy-irc-network-dialog.h		\
	empathy-spell-dialog.h			\
	empathy-accounts-dialog.h		\
	empathy-chat-view.h			\
	empathy-log-window.h			\
	empathy-theme-boxes.h			\
	empathy-account-widget.h		\
	empathy-conf.h				\
	empathy-theme.h				\
	empathy-account-widget-irc.h		\
	empathy-account-widget-sip.h		\
	empathy-contact-dialogs.h		\
	empathy-new-message-dialog.h		\
	empathy-theme-irc.h			\
	empathy-avatar-chooser.h		\
	empathy-contact-list-store.h		\
	empathy-presence-chooser.h		\
	empathy-theme-manager.h			\
	empathy-avatar-image.h			\
	empathy-contact-list-view.h		\
	empathy-ui-utils.h			\
	empathy-cell-renderer-activatable.h	\
	empathy-contact-widget.h		\
	empathy-profile-chooser.h		\
	empathy-cell-renderer-expander.h	\
	empathy-geometry.h			\
	empathy-smiley-manager.h		\
	empathy-cell-renderer-text.h		\
	empathy-spell.h				\
	empathy-contact-menu.h

libempathy_gtk_includedir = $(includedir)/libempathy-gtk/
libempathy_gtk_include_HEADERS =		\
	$(libempathy_gtk_headers)		\
	empathy-gtk-enum-types.h

gladedir = $(datadir)/empathy
glade_DATA = 					\
	empathy-contact-widget.glade		\
	empathy-contact-dialogs.glade		\
	empathy-presence-chooser.glade		\
	empathy-accounts-dialog.glade		\
	empathy-account-widget-generic.glade	\
	empathy-account-widget-jabber.glade	\
	empathy-account-widget-msn.glade	\
	empathy-account-widget-sip.glade	\
	empathy-account-widget-salut.glade	\
	empathy-account-widget-irc.glade	\
	empathy-account-widget-icq.glade	\
	empathy-account-widget-yahoo.glade	\
	empathy-account-widget-groupwise.glade	\
	empathy-account-widget-aim.glade	\
	empathy-spell-dialog.glade		\
	empathy-log-window.glade		\
	empathy-chat.glade			\
	empathy-new-message-dialog.glade

empathy-gtk-marshal.list: $(libempathy_gtk_la_SOURCES) Makefile.am
	( cd $(srcdir) && \
	sed -n -e 's/.*empathy_gtk_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libempathy_gtk_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.h: %-marshal.list Makefile
	$(GLIB_GENMARSHAL) --header --prefix=_$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile
	echo "#include \"empathy-gtk-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=_$(subst -,_,$*)_marshal $< >> $*-marshal.c

empathy-gtk-enum-types.h: stamp-empathy-gtk-enum-types.h
	@true
stamp-empathy-gtk-enum-types.h: Makefile $(libempathy_gtk_headers)
	(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#ifndef __LIBEMPATHY_GTK_ENUM_TYPES_H__\n" \
			--fhead "#define __LIBEMPATHY_GTK_ENUM_TYPES_H__ 1\n\n" \
			--fhead "#include <glib-object.h>\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __LIBEMPATHY_GTK_ENUM_TYPES_H__ */\n" \
			--fprod "#include <libempathy-gtk/@filename@>\n" \
			--eprod "#define EMPATHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(libempathy_gtk_headers) ) > xgen-gth \
	&& (cmp -s xgen-gth empathy-gtk-enum-type.h || cp xgen-gth empathy-gtk-enum-types.h) \
	&& rm -f xgen-gth \
	&& echo timestamp > $(@F)

empathy-gtk-enum-types.c: Makefile $(libempathy_gtk_headers)
	(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#include <config.h>\n" \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"empathy-gtk-enum-types.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(libempathy_gtk_headers) ) > xgen-gtc \
	&& cp xgen-gtc $(@F) \
	&& rm -f xgen-gtc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libempathy-gtk.pc

EXTRA_DIST =			\
	$(glade_DATA)

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	stamp-empathy-gtk-enum-types.h
