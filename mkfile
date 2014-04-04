<config.mk

bins=sfeed sfeed_html sfeed_frames sfeed_plain sfeed_stats sfeed_web \
	sfeed_xmlenc sfeed_opml_import

build:Q: $bins

clean:Q:
	rm -f *.o
	rm -f core a.out
	rm -f $bins

install: build
	echo "TODO"

sfeed:Q: sfeed.o xml.o util.o
	cc $prereq -o $target

sfeed_html:Q: sfeed_html.o util.o
	cc $prereq -o $target

sfeed_plain:Q: sfeed_plain.o util.o
	cc $prereq -o $target

sfeed_frames:Q: sfeed_frames.o util.o
	cc $prereq -o $target

sfeed_opml_import:Q: sfeed_opml_import.o util.o xml.o
	cc $prereq -o $target

sfeed_stats:Q: sfeed_stats.o util.o
	cc $prereq -o $target

sfeed_web:Q: sfeed_web.o util.o xml.o
	cc $prereq -o $target

sfeed_xmlenc:Q: sfeed_xmlenc.o util.o xml.o
	cc $prereq -o $target

%.o: %.c
	cc $CFLAGS -c $stem.c
