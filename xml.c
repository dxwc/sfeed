#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "xml.h"

void
xmlparser_init(XMLParser *x) {
	memset(x, 0, sizeof(XMLParser));
	x->fp = stdin;
}

static int /* like getc(), but do some smart buffering */
xmlparser_getnext(XMLParser *x) {
	if(x->readoffset >= x->readlastbytes) {
		if(feof(x->fp))
			return EOF;
		x->readoffset = 0;
		if(!(x->readlastbytes = fread(x->readbuf, 1, sizeof(x->readbuf), x->fp)))
			return EOF; /* 0 bytes read, assume EOF */
	}
	return (int)x->readbuf[x->readoffset++];
}

static int
xmlparser_parsedata(XMLParser *x, int c) {
	size_t datalen = 0;

	if(x->xmldatastart)
		x->xmldatastart(x);
	do {
		if(x->xmldataentity && c == '&') { /* TODO: test this, entity handler */
			x->data[datalen] = '\0';
			x->xmldata(x, x->data, datalen);
			x->data[0] = c;
			datalen = 1;
			while((c = xmlparser_getnext(x)) != EOF) {
				if(c == '<') { /* TODO: simplify? duplicate code. */
					goto parsedataend;
/*					x->data[datalen] = '\0';
					if(x->xmldata)
						x->xmldata(x, x->data, datalen);
					if(x->xmldataend)
						x->xmldataend(x);
					return c;*/
				}
				if(datalen < sizeof(x->data) - 1)
					x->data[datalen++] = c;
				if(isspace(c))
					break;
				else if(c == ';') {
					x->data[datalen] = '\0';
					x->xmldataentity(x, x->data, datalen);
					datalen = 0;
					break;
				}
			}
		} else if(c == '<') {  /* TODO: simplify? duplicate code. */
parsedataend:
			x->data[datalen] = '\0';
			if(x->xmldata)
				x->xmldata(x, x->data, datalen);
			if(x->xmldataend)
				x->xmldataend(x);
			return c;
		} else {
			if(datalen < sizeof(x->data) - 1) {
				x->data[datalen++] = c;
			} else {
				x->data[datalen] = '\0';  /* TODO: overflow */
				if(x->xmldata)
					x->xmldata(x, x->data, datalen);
				x->data[0] = c;
				datalen = 1;
			}
		}
	} while((c = xmlparser_getnext(x)) != EOF);
	return EOF;
}

static void
xmlparser_parseattrvalue(XMLParser *x, const char *name, size_t namelen, int end) {
	size_t valuelen = 0;
	int c;

	if(x->xmlattrstart)
		x->xmlattrstart(x, x->tag, x->taglen, name, namelen);
	for(valuelen = 0; (c = xmlparser_getnext(x)) != EOF;) {
		if(x->xmlattrentity && c == '&') { /* entities */
			x->data[valuelen] = '\0';
			/* call data function with data before entity if there is data */
			if(x->xmlattr && valuelen)
				x->xmlattr(x, x->tag, x->taglen, name, namelen, x->data, valuelen);
			x->data[0] = c;
			valuelen = 1;
			while((c = xmlparser_getnext(x)) != EOF) {
				if(c == end) { /* TODO: simplify? duplicate code. */
					goto parseattrvalueend;
/*					x->data[valuelen] = '\0';
					if(x->xmlattr)
						x->xmlattr(x, x->tag, x->taglen, name, namelen, x->data, valuelen);
					if(x->xmlattrend)
						x->xmlattrend(x, x->tag, x->taglen, name, namelen);
					return;*/
				}
				if(valuelen < sizeof(x->data) - 1)
					x->data[valuelen++] = c;
				else { /* TODO: entity too long? this should be very strange. */
					x->data[valuelen] = '\0';
					if(x->xmlattr)
						x->xmlattr(x, x->tag, x->taglen, name, namelen, x->data, valuelen);
					valuelen = 0; /* TODO: incorrect ? ';' is read in c below? */
/*							x->data[0] = '\0'; */
					break;
				}
				if(c == ';') {
					x->data[valuelen] = '\0';
					x->xmlattrentity(x, x->tag, x->taglen, name, namelen, x->data, valuelen);
					valuelen = 0; /* TODO: incorrect ? ';' is read in c below? */
					break;
				}
			}
		} else if(c == end) { /* TODO: simplify? duplicate code. */
parseattrvalueend:
			x->data[valuelen] = '\0';
			if(x->xmlattr)
				x->xmlattr(x, x->tag, x->taglen, name, namelen, x->data, valuelen);
			if(x->xmlattrend)
				x->xmlattrend(x, x->tag, x->taglen, name, namelen);
			return;
		} else {
			if(valuelen < sizeof(x->data) - 1) { /* TODO: overflow */
				x->data[valuelen++] = c;
			} else {
				x->data[valuelen] = '\0';
				if(x->xmlattr)
					x->xmlattr(x, x->tag, x->taglen, name, namelen, x->data, valuelen);
				x->data[0] = c;
				valuelen = 1;
			}
		}
	}
}

static int
xmlparser_parseattrs(XMLParser *x) {
	char name[1024]; /* TODO: dont overflow this bitch, also make it bigger perhaps? */
	size_t namelen = 0, valuelen = 0;
	int c, shorttag = 0, endname = 0;

	while((c = xmlparser_getnext(x)) != EOF) {
		if(isspace(c) && namelen) /* Do nothing */
			endname = 1;
		if(isspace(c) || c == '?') { /* Do nothing */
		} else if(c == '=') {
			name[namelen] = '\0';
		} else if(namelen && ((endname && isalpha(c)) || (c == '>' || c == '/'))) {
			/* attribute without value */
			name[namelen] = '\0';
			if(x->xmlattrstart)
				x->xmlattrstart(x, x->tag, x->taglen, name, namelen);
			if(x->xmlattr)
				x->xmlattr(x, x->tag, x->taglen, name, namelen, "", 0);
			if(x->xmlattrend)
				x->xmlattrend(x, x->tag, x->taglen, name, namelen);
			endname = 0;
			name[0] = c;
			namelen = 1;
		} else if(namelen && (c == '\'' || c == '"')) {
			/* attribute with value */
			xmlparser_parseattrvalue(x, name, namelen, c);
			namelen = 0;
			valuelen = 0;
			endname = 0;
		} else if(namelen < sizeof(name) - 1)
			name[namelen++] = c;
		if(c == '>') {
			break;
		} else if(c == '/') { /* TODO: cleanup, ugly. */
			shorttag = 1;
			namelen = 0;
			name[0] = '\0';
		}
	}
	return shorttag; /* TODO: cleanup, ugly. */
}

static void
xmlparser_parsecomment(XMLParser *x) {
	size_t datalen = 0, i = 0;
	int c;

	if(x->xmlcommentstart)
		x->xmlcommentstart(x);
	while((c = xmlparser_getnext(x)) != EOF) {
		if(c == '-' && i < 2)
			i++;
		else if(c == '>') {
			if(i == 2) { /* (!memcmp(cd, "-->", strlen("-->"))) { */
				if(datalen >= 2)
					datalen -= 2;
				else
					datalen = 0;
				x->data[datalen] = '\0'; /* TODO: possible buffer underflow < 0 */
				if(x->xmlcomment)
					x->xmlcomment(x, x->data, datalen); /* TODO: possible buffer underflow < 0 */
				if(x->xmlcommentend)
					x->xmlcommentend(x);
				break;
			}
			i = 0;
		}
		if(datalen < sizeof(x->data) - 1) { /* || (c == '-' && d >= sizeof(x->data) - 4)) {  */ /* TODO: what if the end has --, and its cut on the boundary, test this. */
			x->data[datalen++] = c;
		} else {
			x->data[datalen] = '\0';  /* TODO: overflow */
			if(x->xmlcomment)
				x->xmlcomment(x, x->data, datalen);
			x->data[0] = c;
			datalen = 1;
		}
	}
}

static void
xmlparser_parsecdata(XMLParser *x) {
	size_t datalen = 0, i = 0;
	int c;

	if(x->xmlcdatastart)
		x->xmlcdatastart(x);
	while((c = xmlparser_getnext(x)) != EOF) {
		if(c == ']' && i < 2) {
			i++;
		} else if(c == '>') {
			if(i == 2) { /* (!memcmp(cd, "]]", strlen("]]"))) { */
				if(datalen >= 2)
					datalen -= 2;
				else
					datalen = 0;
				x->data[datalen] = '\0'; /* TODO: check d >= 3 */ /* TODO: buffer underflow */
				if(x->xmlcdata && datalen)
					x->xmlcdata(x, x->data, datalen); /* TODO: buffer underflow */
				if(x->xmlcdataend)
					x->xmlcdataend(x);
				break;
			}
			i = 0;
		}
		if(datalen < sizeof(x->data) - 1) { /* TODO: what if the end has ]>, and its cut on the boundary */
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
xmlparser_parsetag(XMLParser *x) {
	size_t datalen, taglen;
	int c, s, isshorttag = 0;

	x->tag[0] = '\0';
	x->taglen = 0;
	while((c = xmlparser_getnext(x)) != EOF && isspace(c));
	if(c == '!') {
		for(datalen = 0; (c = xmlparser_getnext(x)) != EOF;) {
			if(datalen <= strlen("[CDATA[")) /* if(d < sizeof(x->data)) */
				x->data[datalen++] = c; /* TODO: prevent overflow */
			if(c == '>')
				break;
			else if(c == '-' && !memcmp(x->data, "--", strlen("--"))) { /* comment */ /* TODO: optimize this bitch */
				xmlparser_parsecomment(x);
				break;
			} else if(c == '[' && !memcmp(x->data, "[CDATA[", strlen("[CDATA["))) { /* cdata */ /* TODO: optimize this bitch */
				xmlparser_parsecdata(x);
				break;
			}
		}
	} else if(c == '?') {
		while((c = xmlparser_getnext(x)) != EOF) {
			if(c == '"' || c == '\'') {
				s = c;
				while((c = xmlparser_getnext(x)) != EOF) {
					if(c == s)
						break;
				}
			} else if(c == '>')
				break;
		}
	} else if(c != EOF && c != '>') {
		x->tag[0] = c;
		taglen = 1;
		while((c = xmlparser_getnext(x)) != EOF) {
			if(c == '/')
				isshorttag = 1; /* short tag */
			else if(isspace(c) || c == '>') {
				x->tag[taglen] = '\0';
				if(x->tag[0] == '/') { /* end tag */
					x->taglen = --taglen; /* len -1 because of / */
					if(x->xmltagend)
						x->xmltagend(x, &(x->tag)[1], x->taglen, 0);
				} else {
					x->taglen = taglen;
					if(x->xmltagstart)
						x->xmltagstart(x, x->tag, x->taglen); /* start tag */
					if(isspace(c) && xmlparser_parseattrs(x))
						isshorttag = 1;
					if(x->xmltagstartparsed)
						x->xmltagstartparsed(x, x->tag, x->taglen, isshorttag);
				}
				if(isshorttag && x->xmltagend)
					x->xmltagend(x, x->tag, x->taglen, 1);
				break;
			} else if(taglen < sizeof(x->tag) - 1)
				x->tag[taglen++] = c; /* TODO: prevent overflow */
		}
	}
}

void
xmlparser_parse(XMLParser *x) {
	int c;

	while((c = xmlparser_getnext(x)) != EOF) {
		/*if(isspace(c));*/ /* Do nothing */
		/*else*/ if(c == '<') /* tag */
			xmlparser_parsetag(x);
		else {
			xmlparser_parsedata(x, c);
			xmlparser_parsetag(x);
		}
	}
	return;
}
