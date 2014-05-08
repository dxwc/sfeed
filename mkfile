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

sfeed: xml.o
sfeed_opml_import: xml.o
sfeed_web: xml.o
sfeed_xmlenc: xml.o

&: &.o util.o
	cc $prereq -o $target

&.o: &.c
	cc $CFLAGS -c $stem.c
