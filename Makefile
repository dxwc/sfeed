# sfeed - simple RSS and Atom parser (and programs to add reader functionality).

include config.mk

NAME = sfeed
SRC = sfeed.c sfeed_plain.c sfeed_html.c sfeed_opml_import.c xml.c sfeed_frames.c common.c compat.c
OBJ = ${SRC:.c=.o}

all: options sfeed sfeed_plain sfeed_html sfeed_opml_import sfeed_frames

options:
	@echo ${NAME} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

sfeed: sfeed.o xml.o compat.o
	@echo CC -o $@
	@${CC} -o $@ sfeed.o xml.o compat.o ${LDFLAGS}

sfeed_opml_import: sfeed_opml_import.o xml.o compat.o
	@echo CC -o $@
	@${CC} -o $@ sfeed_opml_import.o xml.o compat.o ${LDFLAGS}

sfeed_plain: sfeed_plain.o common.o compat.o
	@echo CC -o $@
	@${CC} -o $@ sfeed_plain.o common.o compat.o ${LDFLAGS}

sfeed_html: sfeed_html.o common.o compat.o
	@echo CC -o $@
	@${CC} -o $@ sfeed_html.o common.o compat.o ${LDFLAGS}

sfeed_frames: sfeed_frames.o common.o compat.o
	@echo CC -o $@
	@${CC} -o $@ sfeed_frames.o common.o compat.o ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f sfeed sfeed_plain sfeed_html sfeed_frames sfeed_opml_import ${OBJ} ${NAME}-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p ${NAME}-${VERSION}
	@cp -R LICENSE Makefile README config.mk \
		TODO CREDITS sfeedrc.example style.css ${SRC} common.c sfeed_update sfeed_opml_export \
		sfeed.1 sfeed_update.1 sfeed_plain.1 sfeed_html.1 sfeed_opml_import.1 \
		sfeed_frames.c sfeed_frames.1 sfeed_opml_export.1 ${NAME}-${VERSION}
	@tar -cf ${NAME}-${VERSION}.tar ${NAME}-${VERSION}
	@gzip ${NAME}-${VERSION}.tar
	@rm -rf ${NAME}-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f sfeed sfeed_update sfeed_plain sfeed_html sfeed_frames \
		 sfeed_opml_import sfeed_opml_export ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/sfeed \
		${DESTDIR}${PREFIX}/bin/sfeed_update \
		${DESTDIR}${PREFIX}/bin/sfeed_plain \
		${DESTDIR}${PREFIX}/bin/sfeed_html \
		${DESTDIR}${PREFIX}/bin/sfeed_frames \
		${DESTDIR}${PREFIX}/bin/sfeed_opml_import \
		${DESTDIR}${PREFIX}/bin/sfeed_opml_export
	@mkdir -p ${DESTDIR}${PREFIX}/share/sfeed
	@cp -f sfeedrc.example ${DESTDIR}${PREFIX}/share/${NAME}
	@cp -f style.css ${DESTDIR}${PREFIX}/share/${NAME}
	@echo installing manual pages to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < sfeed.1 > ${DESTDIR}${MANPREFIX}/man1/sfeed.1
	@sed "s/VERSION/${VERSION}/g" < sfeed_update.1 > ${DESTDIR}${MANPREFIX}/man1/sfeed_update.1
	@sed "s/VERSION/${VERSION}/g" < sfeed_plain.1 > ${DESTDIR}${MANPREFIX}/man1/sfeed_plain.1
	@sed "s/VERSION/${VERSION}/g" < sfeed_html.1 > ${DESTDIR}${MANPREFIX}/man1/sfeed_html.1
	@sed "s/VERSION/${VERSION}/g" < sfeed_frames.1 > ${DESTDIR}${MANPREFIX}/man1/sfeed_frames.1
	@sed "s/VERSION/${VERSION}/g" < sfeed_opml_import.1 > ${DESTDIR}${MANPREFIX}/man1/sfeed_opml_import.1
	@sed "s/VERSION/${VERSION}/g" < sfeed_opml_export.1 > ${DESTDIR}${MANPREFIX}/man1/sfeed_opml_export.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/sfeed.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_update.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_plain.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_html.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_frames.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_opml_import.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_opml_export.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/sfeed \
		${DESTDIR}${PREFIX}/bin/sfeed_update \
		${DESTDIR}${PREFIX}/bin/sfeed_plain \
		${DESTDIR}${PREFIX}/bin/sfeed_html \
		${DESTDIR}${PREFIX}/bin/sfeed_frames \
		${DESTDIR}${PREFIX}/bin/sfeed_opml_import \
		${DESTDIR}${PREFIX}/bin/sfeed_opml_export \
		${DESTDIR}${PREFIX}/share/${NAME}/sfeedrc.example \
		${DESTDIR}${PREFIX}/share/${NAME}/style.css
	@-rmdir ${DESTDIR}${PREFIX}/share/${NAME}
	@echo removing manual pages from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/sfeed.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_update.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_plain.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_html.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_frames.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_opml_import.1 \
		${DESTDIR}${MANPREFIX}/man1/sfeed_opml_export.1

.PHONY: all options clean dist install uninstall
