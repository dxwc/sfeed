include config.mk

NAME = sfeed
VERSION = 0.9
SRC = \
	sfeed.c\
	sfeed_frames.c\
	sfeed_html.c\
	sfeed_opml_import.c\
	sfeed_plain.c\
	sfeed_web.c\
	sfeed_xmlenc.c\
	util.c\
	xml.c
BIN = \
	sfeed\
	sfeed_frames\
	sfeed_html\
	sfeed_opml_import\
	sfeed_plain\
	sfeed_web\
	sfeed_xmlenc
SCRIPTS = \
	sfeed_opml_export\
	sfeed_update
MAN1 = \
	sfeed.1\
	sfeed_frames.1\
	sfeed_html.1\
	sfeed_opml_export.1\
	sfeed_opml_import.1\
	sfeed_plain.1\
	sfeed_update.1\
	sfeed_web.1\
	sfeed_xmlenc.1
DOC = \
	CHANGELOG\
	CREDITS\
	LICENSE\
	README\
	README.xml\
	TODO
HDR = \
	compat.h\
	util.h\
	xml.h

LIBCOMPAT = libcompat.a
LIBCOMPATSRC = \
	compat/strlcpy.c
LIBCOMPATOBJ = $(LIBCOMPATSRC:.c=.o)

OBJ = ${SRC:.c=.o} \
	$(LIBCOMPATOBJ)

all: options $(BIN)

options:
	@echo ${NAME} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

dist: $(BIN)
	mkdir -p release/${VERSION}
	# legacy man-pages (add doc-oldman as dependency rule).
	#for m in $(MAN1); do cp -f doc/man/$$m release/${VERSION}/; done
	cp -f ${MAN1} ${HDR} ${SCRIPTS} ${SRC} ${DOC} \
		Makefile config.mk \
		sfeedrc.example style.css \
		release/${VERSION}/
	rm -f sfeed-${VERSION}.tar.gz
	(cd release/${VERSION}; \
	tar -czf ../../sfeed-${VERSION}.tar.gz .)

# man to HTML: make sure to copy the mandoc example stylesheet to
# doc/html/man.css .
doc-html: $(MAN1)
	mkdir -p doc/html
	for m in $(MAN1); do mandoc -Thtml -Ostyle=man.css $$m > doc/html/$$m.html; done

# legacy man pages, if you want semantic mandoc pages just copy them.
doc-oldman: $(MAN1)
	mkdir -p doc/man
	for m in $(MAN1); do mandoc -Tman $$m > doc/man/$$m; done

${OBJ}: config.mk

$(LIBCOMPAT): $(LIBCOMPATDOBJ)
	$(AR) -r -c $@ $?
	$(RANLIB) $@

sfeed: sfeed.o xml.o util.o
	${CC} -o $@ sfeed.o xml.o util.o ${LDFLAGS}

sfeed_opml_import: sfeed_opml_import.o xml.o util.o
	${CC} -o $@ sfeed_opml_import.o xml.o util.o ${LDFLAGS}

sfeed_plain: sfeed_plain.o util.o
	${CC} -o $@ sfeed_plain.o util.o ${LDFLAGS}

sfeed_html: sfeed_html.o util.o
	${CC} -o $@ sfeed_html.o util.o ${LDFLAGS}

sfeed_frames: sfeed_frames.o util.o
	${CC} -o $@ sfeed_frames.o util.o ${LDFLAGS}

sfeed_xmlenc: sfeed_xmlenc.o xml.o
	${CC} -o $@ sfeed_xmlenc.o xml.o ${LDFLAGS}

sfeed_web: sfeed_web.o xml.o util.o
	${CC} -o $@ sfeed_web.o xml.o util.o ${LDFLAGS}

clean:
	rm -f ${BIN} ${OBJ} ${LIBCOMPAT}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${BIN} ${SCRIPTS} ${DESTDIR}${PREFIX}/bin
	for f in $(BIN) $(SCRIPTS); do chmod 755 ${DESTDIR}${PREFIX}/bin/$$f; done
	@echo installing example files to ${DESTDIR}${PREFIX}/share/${NAME}
	mkdir -p ${DESTDIR}${PREFIX}/share/${NAME}
	cp -f sfeedrc.example ${DESTDIR}${PREFIX}/share/${NAME}
	cp -f style.css ${DESTDIR}${PREFIX}/share/${NAME}
	@echo installing manual pages to ${DESTDIR}${MANPREFIX}/man1
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f ${MAN1} ${DESTDIR}${MANPREFIX}/man1
	for m in $(MAN1); do chmod 644 ${DESTDIR}${MANPREFIX}/man1/$$m; done

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	for f in $(BIN) $(SCRIPTS); do @rm -f ${DESTDIR}${PREFIX}/bin/$$f; done
	@echo removing example files from ${DESTDIR}${PREFIX}/share/${NAME}
	@rm -f \
		${DESTDIR}${PREFIX}/share/${NAME}/sfeedrc.example \
		${DESTDIR}${PREFIX}/share/${NAME}/style.css
	@-rmdir ${DESTDIR}${PREFIX}/share/${NAME}
	@echo removing manual pages from ${DESTDIR}${MANPREFIX}/man1
	for m in $(MAN1); do @rm -f ${DESTDIR}${MANPREFIX}/man1/$$m; done

.PHONY: all options clean dist install uninstall
