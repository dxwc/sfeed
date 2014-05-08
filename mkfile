CC = cc

<config.mk

TARG = sfeed sfeed_html sfeed_frames sfeed_plain sfeed_stats sfeed_web \
	sfeed_xmlenc sfeed_opml_import

all: $TARG
	

clean:
	rm -f *.o core a.out $TARG

sfeed: xml.o
sfeed_opml_import: xml.o
sfeed_web: xml.o
sfeed_xmlenc: xml.o

&: &.o util.o
	$CC $prereq -o $target

&.o: &.c
	$CC $CFLAGS -c $stem.c
