#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct xmlparser {
	/* handlers */
	void (*xmltagstart)(struct xmlparser *, const char *, size_t);
	void (*xmltagstartparsed)(struct xmlparser *, const char *,
	      size_t, int);
	void (*xmltagend)(struct xmlparser *, const char *, size_t, int);
	void (*xmldatastart)(struct xmlparser *);
	void (*xmldata)(struct xmlparser *, const char *, size_t);
	void (*xmldataend)(struct xmlparser *);
	void (*xmldataentity)(struct xmlparser *, const char *, size_t);
	void (*xmlattrstart)(struct xmlparser *, const char *, size_t,
	      const char *, size_t);
	void (*xmlattr)(struct xmlparser *, const char *, size_t,
	      const char *, size_t, const char *, size_t);
	void (*xmlattrend)(struct xmlparser *, const char *, size_t,
	      const char *, size_t);
	void (*xmlattrentity)(struct xmlparser *, const char *, size_t,
	      const char *, size_t, const char *, size_t);
	void (*xmlcdatastart)(struct xmlparser *);
	void (*xmlcdata)(struct xmlparser *, const char *, size_t);
	void (*xmlcdataend)(struct xmlparser *);
	void (*xmlcommentstart)(struct xmlparser *);
	void (*xmlcomment)(struct xmlparser *, const char *, size_t);
	void (*xmlcommentend)(struct xmlparser *);

	int (*getnext)(struct xmlparser *);

	int readerrno; /* errno set from read(). */
	int fd; /* fd to read from */

	const char *str; /* "read" from string */

	/* private; internal state */
	char tag[1024]; /* current tag */
	int isshorttag; /* current tag is in short form ? */
	size_t taglen;
	char name[256]; /* current attribute name */
	char data[BUFSIZ]; /* data buffer used for tag and attribute data */
	size_t readoffset;
	size_t readlastbytes;
	/* read buffer used by xmlparser_getnext */
	unsigned char readbuf[BUFSIZ];
} XMLParser;

void xmlparser_parse_fd(XMLParser *, int);
void xmlparser_parse_string(XMLParser *, const char *);
