#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct xmlparser {
	/* handlers */
	void (*xmltagstart)(struct xmlparser *p, const char *tag, size_t taglen);
	void (*xmltagstartparsed)(struct xmlparser *p, const char *tag, size_t taglen, int isshort);
	void (*xmltagend)(struct xmlparser *p, const char *tag, size_t taglen, int isshort);
	void (*xmldatastart)(struct xmlparser *p);
	void (*xmldata)(struct xmlparser *p, const char *data, size_t datalen);
	void (*xmldataend)(struct xmlparser *p);
	void (*xmldataentity)(struct xmlparser *p, const char *data, size_t datalen);
	void (*xmlattrstart)(struct xmlparser *p, const char *tag, size_t taglen, const char *name, size_t namelen);
	void (*xmlattr)(struct xmlparser *p, const char *tag, size_t taglen, const char *name, size_t namelen, const char *value, size_t valuelen);
	void (*xmlattrend)(struct xmlparser *p, const char *tag, size_t taglen, const char *name, size_t namelen);
	void (*xmlattrentity)(struct xmlparser *p, const char *tag, size_t taglen, const char *name, size_t namelen, const char *value, size_t valuelen);
	void (*xmlcdatastart)(struct xmlparser *p);
	void (*xmlcdata)(struct xmlparser *p, const char *data, size_t datalen);
	void (*xmlcdataend)(struct xmlparser *p);
	void (*xmlcommentstart)(struct xmlparser *p);
	void (*xmlcomment)(struct xmlparser *p, const char *comment, size_t commentlen);
	void (*xmlcommentend)(struct xmlparser *p);

	FILE *fp; /* file stream to read from */

	/* private; internal state */
	char tag[1024]; /* current tag */
	int isshorttag; /* current tag is in short form ? */
	size_t taglen;
	char name[256]; /* current attribute name */
	char data[BUFSIZ]; /* data buffer used for tag and attribute data */
	size_t readoffset;
	size_t readlastbytes;
	unsigned char readbuf[BUFSIZ]; /* read buffer used by xmlparser_getnext() */
} XMLParser;

void xmlparser_init(XMLParser *x);
void xmlparser_parse(XMLParser *x);
