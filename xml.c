#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xml.h"

static int
xmlparser_string_getnext(XMLParser *x)
{
	if (!*(x->str))
		return EOF;
	return (int)*(x->str++);
}

static int /* like getc(), but do some smart buffering */
xmlparser_fd_getnext(XMLParser *x)
{
	ssize_t r;

	/* previous read error was set */
	if(x->readerrno)
		return EOF;

	if(x->readoffset >= x->readlastbytes) {
		x->readoffset = 0;
again:
		r = read(x->fd, x->readbuf, sizeof(x->readbuf));
		if(r == -1) {
			if(errno == EINTR)
				goto again;
			x->readerrno = errno;
			x->readlastbytes = 0;
			return EOF;
		} else if(!r) {
			return EOF;
		}
		x->readlastbytes = r;
	}
	return (int)x->readbuf[x->readoffset++];
}

static int
xmlparser_getnext(XMLParser *x)
{
	return x->getnext(x);
}

static __inline__ void
xmlparser_parseattrs(XMLParser *x)
{
	size_t namelen = 0, valuelen;
	int c, endsep, endname = 0;

	while((c = xmlparser_getnext(x)) != EOF) {
		if(isspace(c)) { /* TODO: simplify endname ? */
			if(namelen)
				endname = 1;
			continue;
		}
		if(c == '?')
			; /* ignore */
		else if(c == '=') {
			x->name[namelen] = '\0';
		} else if(namelen && ((endname && isalpha(c)) || (c == '>' || c == '/'))) {
			/* attribute without value */
			x->name[namelen] = '\0';
			if(x->xmlattrstart)
				x->xmlattrstart(x, x->tag, x->taglen, x->name, namelen);
			if(x->xmlattr)
				x->xmlattr(x, x->tag, x->taglen, x->name, namelen, "", 0);
			if(x->xmlattrend)
				x->xmlattrend(x, x->tag, x->taglen, x->name, namelen);
			endname = 0;
			x->name[0] = c;
			namelen = 1;
		} else if(namelen && (c == '\'' || c == '"')) {
			/* attribute with value */
			endsep = c; /* c is end separator */
			if(x->xmlattrstart)
				x->xmlattrstart(x, x->tag, x->taglen, x->name, namelen);
			for(valuelen = 0; (c = xmlparser_getnext(x)) != EOF;) {
				if(c == '&') { /* entities */
					x->data[valuelen] = '\0';
					/* call data function with data before entity if there is data */
					if(valuelen && x->xmlattr)
						x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
					x->data[0] = c;
					valuelen = 1;
					while((c = xmlparser_getnext(x)) != EOF) {
						if(c == endsep)
							break;
						if(valuelen < sizeof(x->data) - 1)
							x->data[valuelen++] = c;
						else {
							/* TODO: entity too long? this should be very strange. */
							x->data[valuelen] = '\0';
							if(x->xmlattr)
								x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
							valuelen = 0;
							break;
						}
						if(c == ';') {
							x->data[valuelen] = '\0';
							if(x->xmlattrentity)
								x->xmlattrentity(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
							valuelen = 0;
							break;
						}
					}
				} else if(c != endsep) {
					if(valuelen < sizeof(x->data) - 1) {
						x->data[valuelen++] = c;
					} else {
						x->data[valuelen] = '\0';
						if(x->xmlattr)
							x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
						x->data[0] = c;
						valuelen = 1;
					}
				}
				if(c == endsep) {
					x->data[valuelen] = '\0';
					if(x->xmlattr)
						x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
					if(x->xmlattrend)
						x->xmlattrend(x, x->tag, x->taglen, x->name, namelen);
					break;
				}
			}
			namelen = 0;
			endname = 0;
		} else if(namelen < sizeof(x->name) - 1) {
			x->name[namelen++] = c;
		}
		if(c == '>') {
			break;
		} else if(c == '/') {
			x->isshorttag = 1;
			namelen = 0;
			x->name[0] = '\0';
		}
	}
}

static __inline__ void
xmlparser_parsecomment(XMLParser *x)
{
	size_t datalen = 0, i = 0;
	int c;

	if(x->xmlcommentstart)
		x->xmlcommentstart(x);
	while((c = xmlparser_getnext(x)) != EOF) {
		if(c == '-' && i < 2)
			i++;
		else if(c == '>') {
			if(i == 2) { /* -- */
				if(datalen >= 2) {
					datalen -= 2;
					x->data[datalen] = '\0';
					if(x->xmlcomment)
						x->xmlcomment(x, x->data, datalen);
				}
				if(x->xmlcommentend)
					x->xmlcommentend(x);
				break;
			}
			i = 0;
		}
		 /* || (c == '-' && d >= sizeof(x->data) - 4)) { */
		/* TODO: what if the end has --, and it's cut on the boundary, test this. */
		if(datalen < sizeof(x->data) - 1)
			x->data[datalen++] = c;
		else {
			x->data[datalen] = '\0';
			if(x->xmlcomment)
				x->xmlcomment(x, x->data, datalen);
			x->data[0] = c;
			datalen = 1;
		}
	}
}

/* TODO:
 * <test><![CDATA[1234567dddd8]]]>
 *
 * with x->data of sizeof(15) gives 2 ] at end of cdata, should be 1
 * test comment function too for similar bug?
 *
 */
static __inline__ void
xmlparser_parsecdata(XMLParser *x)
{
	size_t datalen = 0, i = 0;
	int c;

	if(x->xmlcdatastart)
		x->xmlcdatastart(x);
	while((c = xmlparser_getnext(x)) != EOF) {
		if(c == ']' && i < 2) {
			i++;
		} else if(c == '>') {
			if(i == 2) { /* ]] */
				if(datalen >= 2) {
					datalen -= 2;
					x->data[datalen] = '\0';
					if(x->xmlcdata && datalen)
						x->xmlcdata(x, x->data, datalen);
				}
				if(x->xmlcdataend)
					x->xmlcdataend(x);
				break;
			}
			i = 0;
		}
		/* TODO: what if the end has ]>, and it's cut on the boundary */
		if(datalen < sizeof(x->data) - 1) {
			x->data[datalen++] = c;
		} else {
			x->data[datalen] = '\0';
			if(x->xmlcdata)
				x->xmlcdata(x, x->data, datalen);
			x->data[0] = c;
			datalen = 1;
		}
	}
}

static void
xmlparser_parse(XMLParser *x)
{
	int c, ispi;
	size_t datalen, tagdatalen, taglen;

	while((c = xmlparser_getnext(x)) != EOF && c != '<'); /* skip until < */

	while(c != EOF) {
		if(c == '<') { /* parse tag */
			if((c = xmlparser_getnext(x)) == EOF)
				return;
			x->tag[0] = '\0';
			x->taglen = 0;
			if(c == '!') { /* cdata and comments */
				for(tagdatalen = 0; (c = xmlparser_getnext(x)) != EOF;) {
					if(tagdatalen <= strlen("[CDATA[")) /* if(d < sizeof(x->data)) */
						x->data[tagdatalen++] = c; /* TODO: prevent overflow */
					if(c == '>')
						break;
					else if(c == '-' && tagdatalen == strlen("--") &&
							(x->data[0] == '-')) { /* comment */
						xmlparser_parsecomment(x);
						break;
					} else if(c == '[') {
						if(tagdatalen == strlen("[CDATA[") &&
							x->data[1] == 'C' && x->data[2] == 'D' &&
							x->data[3] == 'A' && x->data[4] == 'T' &&
							x->data[5] == 'A' && x->data[6] == '[') { /* CDATA */
							xmlparser_parsecdata(x);
							break;
						#if 0
						} else {
							/* TODO ? */
							/* markup declaration section */
							while((c = xmlparser_getnext(x)) != EOF && c != ']');
						#endif
						}
					}
				}
			} else { /* normal tag (open, short open, close), processing instruction. */
				if(isspace(c))
					while((c = xmlparser_getnext(x)) != EOF && isspace(c));
				if(c == EOF)
					return;
				x->tag[0] = c;
				ispi = (c == '?') ? 1 : 0;
				x->isshorttag = ispi;
				taglen = 1;
				while((c = xmlparser_getnext(x)) != EOF) {
					if(c == '/') /* TODO: simplify short tag? */
						x->isshorttag = 1; /* short tag */
					else if(c == '>' || isspace(c)) {
						x->tag[taglen] = '\0';
						if(x->tag[0] == '/') { /* end tag, starts with </ */
							x->taglen = --taglen; /* len -1 because of / */
							if(taglen && x->xmltagend)
								x->xmltagend(x, &(x->tag)[1], x->taglen, 0);
						} else {
							x->taglen = taglen;
							/* start tag */
							if(x->xmltagstart)
								x->xmltagstart(x, x->tag, x->taglen);
							if(isspace(c))
								xmlparser_parseattrs(x);
							if(x->xmltagstartparsed)
								x->xmltagstartparsed(x, x->tag, x->taglen, x->isshorttag);
						}
						/* call tagend for shortform or processing instruction */
						if((x->isshorttag || ispi) && x->xmltagend)
							x->xmltagend(x, x->tag, x->taglen, 1);
						break;
					} else if(taglen < sizeof(x->tag) - 1)
						x->tag[taglen++] = c;
				}
			}
		} else {
			/* parse tag data */
			datalen = 0;
			if(x->xmldatastart)
				x->xmldatastart(x);
			while((c = xmlparser_getnext(x)) != EOF) {
				if(c == '&') {
					if(datalen) {
						x->data[datalen] = '\0';
						if(x->xmldata)
							x->xmldata(x, x->data, datalen);
					}
					x->data[0] = c;
					datalen = 1;
					while((c = xmlparser_getnext(x)) != EOF) {
						if(c == '<')
							break;
						if(datalen < sizeof(x->data) - 1)
							x->data[datalen++] = c;
						if(isspace(c))
							break;
						else if(c == ';') {
							x->data[datalen] = '\0';
							if(x->xmldataentity)
								x->xmldataentity(x, x->data, datalen);
							datalen = 0;
							break;
						}
					}
				} else if(c != '<') {
					if(datalen < sizeof(x->data) - 1) {
						x->data[datalen++] = c;
					} else {
						x->data[datalen] = '\0';
						if(x->xmldata)
							x->xmldata(x, x->data, datalen);
						x->data[0] = c;
						datalen = 1;
					}
				}
				if(c == '<') {
					x->data[datalen] = '\0';
					if(x->xmldata && datalen)
						x->xmldata(x, x->data, datalen);
					if(x->xmldataend)
						x->xmldataend(x);
					break;
				}
			}
		}
	}
}

void
xmlparser_parse_string(XMLParser *x, const char *s)
{
	x->str = s;
	x->getnext = xmlparser_string_getnext;
	xmlparser_parse(x);
}

void
xmlparser_parse_fd(XMLParser *x, int fd)
{
	x->fd = fd;
	x->getnext = xmlparser_fd_getnext;
	xmlparser_parse(x);
}
