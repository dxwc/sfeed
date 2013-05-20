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

__inline__ int /* like getc(), but do some smart buffering */
xmlparser_getnext(XMLParser *x) {
	if(x->readoffset >= x->readlastbytes) {
		x->readoffset = 0;
		if(!(x->readlastbytes = fread(x->readbuf, 1, sizeof(x->readbuf), x->fp)))
			return EOF; /* 0 bytes read, assume EOF */
	}
	return (int)x->readbuf[x->readoffset++];
}

__inline__ void
xmlparser_parseattrvalue(XMLParser *x, const char *name, size_t namelen, int end) {
	size_t valuelen = 0;
	int c;

	if(x->xmlattrstart)
		x->xmlattrstart(x, x->tag, x->taglen, name, namelen);
	for(valuelen = 0; (c = xmlparser_getnext(x)) != EOF;) {
		if(c == '&' && x->xmlattrentity) { /* entities */
			x->data[valuelen] = '\0';
			/* call data function with data before entity if there is data */
			if(valuelen && x->xmlattr)
				x->xmlattr(x, x->tag, x->taglen, name, namelen, x->data, valuelen);
			x->data[0] = c;
			valuelen = 1;
			while((c = xmlparser_getnext(x)) != EOF) {
				if(c == end)
					goto parseattrvalueend;
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
		} else if(c == end) { /* TODO: ugly, remove goto?, simplify? duplicate code. */
parseattrvalueend:
			x->data[valuelen] = '\0';
			if(x->xmlattr)
				x->xmlattr(x, x->tag, x->taglen, name, namelen, x->data, valuelen);
			if(x->xmlattrend)
				x->xmlattrend(x, x->tag, x->taglen, name, namelen);
			return;
		} else {
			if(valuelen < sizeof(x->data) - 1) {
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

__inline__ void
xmlparser_parseattrs(XMLParser *x, int *isshorttag) {
	size_t namelen = 0;
	int c, endname = 0;

	while((c = xmlparser_getnext(x)) != EOF) {
		if(isspace(c)) {
			if(namelen) /* Do nothing */
				endname = 1;
			else
				continue;
		}
		if(c == '?' && isspace(c)) { /* Do nothing */
		} else if(c == '=') {
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
			xmlparser_parseattrvalue(x, x->name, namelen, c);
			namelen = 0;
			endname = 0;
		} else if(namelen < sizeof(x->name) - 1)
			x->name[namelen++] = c;
		if(c == '>') {
			break;
		} else if(c == '/') {
			*isshorttag = 1;
			namelen = 0;
			x->name[0] = '\0';
		}
	}
}

__inline__ void
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
				if(datalen >= 2) {
					datalen -= 2;
					x->data[datalen] = '\0';
					if(x->xmlcomment)
						x->xmlcomment(x, x->data, datalen);
/*				} else {
					datalen = 0;
					x->data[datalen] = '\0';*/
				}
				if(x->xmlcommentend)
					x->xmlcommentend(x);
				break;
			}
			i = 0;
		}
		if(datalen < sizeof(x->data) - 1) { /* || (c == '-' && d >= sizeof(x->data) - 4)) { */ /* TODO: what if the end has --, and its cut on the boundary, test this. */
			x->data[datalen++] = c;
		} else {
			x->data[datalen] = '\0';
			if(x->xmlcomment)
				x->xmlcomment(x, x->data, datalen);
			x->data[0] = c;
			datalen = 1;
		}
	}
}

__inline__ void
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
				if(datalen >= 2) {
					datalen -= 2;
					x->data[datalen] = '\0';
					if(x->xmlcdata)
						x->xmlcdata(x, x->data, datalen);
/*				} else {
					datalen = 0;
					x->data[datalen] = '\0';*/
				}
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

__inline__ void
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
			else if(c == '-' && datalen == strlen("--") &&
			        (x->data[0] == '-')) { /* comment */ /* TODO: optimize this bitch */
				xmlparser_parsecomment(x);
				break;
			} else if(c == '[' && datalen == strlen("[CDATA[") &&
				x->data[1] == 'C' && x->data[2] == 'D' &&
				x->data[3] == 'A' && x->data[4] == 'T' &&
				x->data[5] == 'A' && x->data[6] == '[') { /* cdata */
				xmlparser_parsecdata(x);
				break;
			}
		}
	} else if(c == '?') {
		while((c = xmlparser_getnext(x)) != EOF) {
			if(c == '"' || c == '\'')
				for(s = c; (c = xmlparser_getnext(x)) != EOF && c != s;);
			else if(c == '>')
				break;
		}
	/* TODO: find out why checking isalpha(c) gives "not enough memory"
	 * also check if maybe when there is << or <> it might go into an infinite loop (unsure) */
	} else if(c != EOF && c != '>') { /* TODO: optimize and put above the other conditions ? */
		x->tag[0] = c;
		taglen = 1;
		while((c = xmlparser_getnext(x)) != EOF) {
			if(c == '/')
				isshorttag = 1; /* short tag */
			else if(c == '>' || isspace(c)) {
				x->tag[taglen] = '\0';
				if(x->tag[0] == '/') { /* end tag, starts with </ */
					x->taglen = --taglen; /* len -1 because of / */
					if(x->xmltagend)
						x->xmltagend(x, &(x->tag)[1], x->taglen, 0);
				} else {
					x->taglen = taglen;
					if(x->xmltagstart)
						x->xmltagstart(x, x->tag, x->taglen); /* start tag */
					if(isspace(c))
						xmlparser_parseattrs(x, &isshorttag);
					if(x->xmltagstartparsed)
						x->xmltagstartparsed(x, x->tag, x->taglen, isshorttag);
				}
				if(isshorttag && x->xmltagend)
					x->xmltagend(x, x->tag, x->taglen, 1);
				break;
			} else if(taglen < sizeof(x->tag) - 1)
				x->tag[taglen++] = c;
		}
	}
}

void
xmlparser_parsedata(XMLParser *x, int c) { /* TODO: remove int c, ugly */
	size_t datalen = 0;

	if(x->xmldatastart)
		x->xmldatastart(x);
	do {
		if(c == '&' && x->xmldataentity) { /* TODO: test this, entity handler */
			x->data[datalen] = '\0';
			x->xmldata(x, x->data, datalen);
			x->data[0] = c;
			datalen = 1;
			while((c = xmlparser_getnext(x)) != EOF) {
				if(c == '<')
					goto parsedataend;
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
		} else if(c == '<') { /* TODO: ugly, remove goto ? simplify? duplicate code. */
parsedataend:
			x->data[datalen] = '\0';
			if(x->xmldata)
				x->xmldata(x, x->data, datalen);
			if(x->xmldataend)
				x->xmldataend(x);
			break;
		} else {
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
	} while((c = xmlparser_getnext(x)) != EOF);
}

void
xmlparser_parse(XMLParser *x) {
	int c;

	while((c = xmlparser_getnext(x)) != EOF) {
		if(c == '<') /* tag */
			xmlparser_parsetag(x);
		else {
			xmlparser_parsedata(x, c);
			xmlparser_parsetag(x);
		}
	}
	return;
}
