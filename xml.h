typedef struct xmlparser {
	/* handlers */
	void (*xmlattr)(struct xmlparser *, const char *, size_t,
	      const char *, size_t, const char *, size_t);
	void (*xmlattrend)(struct xmlparser *, const char *, size_t,
	      const char *, size_t);
	void (*xmlattrstart)(struct xmlparser *, const char *, size_t,
	      const char *, size_t);
	void (*xmlattrentity)(struct xmlparser *, const char *, size_t,
	      const char *, size_t, const char *, size_t);
	void (*xmlcdatastart)(struct xmlparser *);
	void (*xmlcdata)(struct xmlparser *, const char *, size_t);
	void (*xmlcdataend)(struct xmlparser *);
	void (*xmlcommentstart)(struct xmlparser *);
	void (*xmlcomment)(struct xmlparser *, const char *, size_t);
	void (*xmlcommentend)(struct xmlparser *);
	void (*xmldata)(struct xmlparser *, const char *, size_t);
	void (*xmldataend)(struct xmlparser *);
	void (*xmldataentity)(struct xmlparser *, const char *, size_t);
	void (*xmldatastart)(struct xmlparser *);
	void (*xmltagend)(struct xmlparser *, const char *, size_t, int);
	void (*xmltagstart)(struct xmlparser *, const char *, size_t);
	void (*xmltagstartparsed)(struct xmlparser *, const char *,
	      size_t, int);

	int (*getnext)(struct xmlparser *);

	/* for use with xmlparser_parse_fd */
	/* errno set from read(). */
	int readerrno;
	int fd;

	/* for use with "read" from string: xmlparser_parse_string */
	const char *str;

	/* private; internal state */

	/* current tag */
	char tag[1024];
	size_t taglen;
	/* current tag is in short form ? <tag /> */
	int isshorttag;
	/* current attribute name */
	char name[256];
	/* data buffer used for tag data, cdata and attribute data */
	char data[BUFSIZ];

	size_t readoffset;
	size_t readlastbytes;
	/* read buffer used by xmlparser_parse_fd */
	unsigned char readbuf[BUFSIZ];
} XMLParser;

int     xml_codepointtoutf8(uint32_t, uint32_t *);
ssize_t xml_entitytostr(const char *, char *, size_t);
ssize_t xml_namedentitytostr(const char *, char *, size_t);
ssize_t xml_numericetitytostr(const char *, char *, size_t);

void xmlparser_parse_fd(XMLParser *, int);
void xmlparser_parse_string(XMLParser *, const char *);
