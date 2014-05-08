#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>

#include "util.h"
#include "xml.h"

#define ISWSNOSPACE(c) (((unsigned)c - '\t') < 5) /* isspace(c) && c != ' ' */

enum { FeedTypeNone = 0, FeedTypeRSS = 1, FeedTypeAtom = 2 };
static const char *feedtypes[] = { "", "rss", "atom" };

enum { ContentTypeNone = 0, ContentTypePlain = 1, ContentTypeHTML = 2 };
static const char *contenttypes[] = { "", "plain", "html" };

static const int FieldSeparator = '\t'; /* output field seperator character */

enum {
	TagUnknown = 0,
	/* RSS */
	RSSTagDcdate, RSSTagPubdate, RSSTagTitle,
	RSSTagLink, RSSTagDescription, RSSTagContentencoded,
	RSSTagGuid, RSSTagAuthor, RSSTagDccreator,
	/* Atom */
	AtomTagPublished, AtomTagUpdated, AtomTagTitle,
	AtomTagSummary, AtomTagContent,
	AtomTagId, AtomTagLink, AtomTagAuthor
};

/* String data / memory pool */
typedef struct string {
	char *data; /* data */
	size_t len; /* string length */
	size_t bufsiz; /* allocated size */
} String;

/* Feed item */
typedef struct feeditem {
	String timestamp;
	String title;
	String link;
	String content;
	int contenttype; /* ContentTypePlain or ContentTypeHTML */
	String id;
	String author;
	int feedtype; /* FeedTypeRSS or FeedTypeAtom */
} FeedItem;

typedef struct feedtag {
	char *name;
	size_t namelen;
	int id;
} FeedTag;

typedef struct feedcontext {
	String *field; /* pointer to current FeedItem field String */
	FeedItem item; /* data for current feed item */
	char tag[256]; /* current tag _inside_ a feeditem */
	int tagid;     /* unique number for parsed tag (faster comparison) */
	size_t taglen;
	int iscontent;
	int iscontenttag;
	int attrcount;
} FeedContext;

static void die(const char *s);
static void cleanup(void);

static FeedContext ctx;
static XMLParser parser; /* XML parser state */
static char *append = NULL; /* append string after each output line */

/* unique number for parsed tag (faster comparison) */
static int
gettag(int feedtype, const char *name, size_t namelen) {
	/* RSS, alphabetical order */
	static FeedTag rsstag[] = {
		{ "author", 6, RSSTagAuthor },
		{ "content:encoded", 15, RSSTagContentencoded },
		{ "dc:creator", 10, RSSTagDccreator },
		{ "dc:date", 7, RSSTagDcdate },
		{ "description", 11, RSSTagDescription },
		{ "guid", 4, RSSTagGuid },
		{ "link", 4, RSSTagLink },
		{ "pubdate", 7, RSSTagPubdate },
		{ "title", 5, RSSTagTitle },
		{ NULL, 0, -1 }
	};
	/* Atom, alphabetical order */
	static FeedTag atomtag[] = {
		{ "author", 6, AtomTagAuthor },
		{ "content", 7, AtomTagContent },
		{ "id", 2, AtomTagId },
		{ "link", 4, AtomTagLink },
		{ "published", 9, AtomTagPublished },
		{ "summary", 7, AtomTagSummary },
		{ "title", 5, AtomTagTitle },
		{ "updated", 7, AtomTagUpdated },
		{ NULL, 0, -1 }
	};
	int i, n;

	if(namelen < 2 || namelen > 15) /* optimization */
		return TagUnknown;

	if(feedtype == FeedTypeRSS) {
		for(i = 0; rsstag[i].name; i++) {
			if(!(n = strncasecmp(rsstag[i].name, name, rsstag[i].namelen)))
				return rsstag[i].id;
			/* optimization: it's sorted so nothing after it matches. */
			if(n > 0)
				return TagUnknown;
		}
	} else if(feedtype == FeedTypeAtom) {
		for(i = 0; atomtag[i].name; i++) {
			if(!(n = strncasecmp(atomtag[i].name, name, atomtag[i].namelen)))
				return atomtag[i].id;
			/* optimization: it's sorted so nothing after it matches. */
			if(n > 0)
				return TagUnknown;
		}
	}
	return TagUnknown;
}

static size_t
codepointtoutf8(unsigned long cp, unsigned long *utf) {
	if(cp >= 0x10000) { /* 4 bytes */
		*utf = 0xf0808080 | ((cp & 0xfc0000) << 6) | ((cp & 0x3f000) << 4) |
		       ((cp & 0xfc0) << 2) | (cp & 0x3f);
		return 4;
	} else if(cp >= 0x00800) { /* 3 bytes */
		*utf = 0xe08080 | ((cp & 0x3f000) << 4) | ((cp & 0xfc0) << 2) |
		       (cp & 0x3f);
		return 3;
	} else if(cp >= 0x80) { /* 2 bytes */
		*utf = 0xc080 | ((cp & 0xfc0) << 2) | (cp & 0x3f);
		return 2;
	}
	*utf = cp & 0xff;
	return *utf ? 1 : 0; /* 1 byte */
}

static size_t
namedentitytostr(const char *e, char *buffer, size_t bufsiz) {
	char *entities[6][2] = {
		{ "&lt;", "<" },
		{ "&gt;", ">" },
		{ "&apos;", "'" },
		{ "&amp;", "&" },
		{ "&quot;", "\"" },
		{ NULL, NULL }
	};
	size_t i;

	if(*e != '&' || bufsiz < 2) /* doesn't start with & */
		return 0;
	for(i = 0; entities[i][0]; i++) {
		/* NOTE: compares max 7 chars */
		if(!strncasecmp(e, entities[i][0], 6)) {
			buffer[0] = *(entities[i][1]);
			buffer[1] = '\0';
			return 1;
		}
	}
	return 0;
}

/* convert named- or numeric entity string to buffer string
 * returns byte-length of string. */
static size_t
entitytostr(const char *e, char *buffer, size_t bufsiz) {
	unsigned long l = 0, cp = 0, b;
	size_t len;

	if(*e != '&' || bufsiz < 5) /* doesn't start with & */
		return 0;
	if(e[1] == '#') {
		e += 2; /* skip &# */
		if(*e == 'x') {
			e++;
			l = strtol(e, NULL, 16); /* hex */
		} else
			l = strtol(e, NULL, 10); /* decimal */
		if(!(len = codepointtoutf8(l, &cp)))
			return 0;
		/* make string */
		for(b = 0; b < len; b++)
			buffer[b] = (cp >> (8 * (len - 1 - b))) & 0xff;
		buffer[len] = '\0';
		/* escape whitespace */
		if(ISWSNOSPACE(buffer[0])) { /* isspace(c) && c != ' ' */
			if(buffer[0] == '\n') { /* escape newline */
				buffer[0] = '\\';
				buffer[1] = 'n';
				buffer[2] = '\0';
				return 2; /* len */
			} else if(buffer[0] == '\\') { /* escape \ */
				buffer[0] = '\\';
				buffer[1] = '\\';
				buffer[2] = '\0';
				return 2; /* len */
			} else if(buffer[0] == '\t') { /* escape tab */
				buffer[0] = '\\';
				buffer[1] = 't';
				buffer[2] = '\0';
				return 2; /* len */
			}
		}
		return len;
	} else /* named entity */
		return namedentitytostr(e, buffer, bufsiz);
	return 0;
}

static void
string_clear(String *s) {
	if(s->data)
		s->data[0] = '\0'; /* clear string only; don't free, prevents
		                      unnecessary reallocation */
	s->len = 0;
}

static void
string_buffer_init(String *s, size_t len) {
	if(!(s->data = malloc(len)))
		die("can't allocate enough memory");
	s->bufsiz = len;
	string_clear(s);
}

static void
string_free(String *s) {
	free(s->data);
	s->data = NULL;
	s->bufsiz = 0;
	s->len = 0;
}

static int
string_buffer_realloc(String *s, size_t newlen) {
	char *p;
	size_t alloclen;
	for(alloclen = 16; alloclen <= newlen; alloclen *= 2);
	if(!(p = realloc(s->data, alloclen))) {
		string_free(s); /* free previous allocation */
		die("can't allocate enough memory");
	}
	s->bufsiz = alloclen;
	s->data = p;
	return s->bufsiz;
}

static void
string_append(String *s, const char *data, size_t len) {
	if(!len || *data == '\0')
		return;
	/* check if allocation is necesary, dont shrink buffer
	   should be more than bufsiz ofcourse */
	if(s->len + len > s->bufsiz)
		string_buffer_realloc(s, s->len + len);
	memcpy(s->data + s->len, data, len);
	s->len += len;
	s->data[s->len] = '\0';
}

/* cleanup, free allocated memory, etc */
static void
cleanup(void) {
	string_free(&ctx.item.timestamp);
	string_free(&ctx.item.title);
	string_free(&ctx.item.link);
	string_free(&ctx.item.content);
	string_free(&ctx.item.id);
	string_free(&ctx.item.author);
}

/* print error message to stderr */
static void
die(const char *s) {
	fputs("sfeed: ", stderr);
	fputs(s, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

/* get timezone from string, return as formatted string and time offset,
 * for the offset it assumes GMT */
static int
gettimetz(const char *s, char *buf, size_t bufsiz) {
	const char *p = s;
	char tzname[16] = "", *t = NULL;
	int tzhour = 0, tzmin = 0;
	unsigned int i;
	char c;

	buf[0] = '\0';
	if(bufsiz < sizeof(tzname) + strlen(" -00:00"))
		return 0;
	for(; *p && isspace((int)*p); p++); /* skip whitespace */
	/* loop until some common timezone delimiters are found */
	for(; *p && (*p != '+' && *p != '-' && *p != 'Z' && *p != 'z'); p++);

	/* TODO: cleanup / simplify */
	if(isalpha((int)*p)) {
		if(*p == 'Z' || *p == 'z') {
			memcpy(buf, "GMT+00:00", strlen("GMT+00:00") + 1);
			return 0;
		} else {
			for(i = 0, t = &tzname[0]; i < (sizeof(tzname) - 1) &&
				(*p && isalpha((int)*p)); i++)
				*(t++) = *(p++);
			*t = '\0';
		}
	} else
		memcpy(tzname, "GMT", strlen("GMT") + 1);
	if(!(*p)) {
		strlcpy(buf, tzname, bufsiz);
		return 0;
	}
	if((sscanf(p, "%c%02d:%02d", &c, &tzhour, &tzmin)) > 0);
	else if(sscanf(p, "%c%02d%02d", &c, &tzhour, &tzmin) > 0);
	else if(sscanf(p, "%c%d", &c, &tzhour) > 0)
		tzmin = 0;
	sprintf(buf, "%s%c%02d%02d", tzname, c, tzhour, tzmin);
	/* TODO: test + or - offset */
	return (tzhour * 3600) + (tzmin * 60) * (c == '-' ? -1 : 1);
}

static time_t
parsetime(const char *s, char *buf, size_t bufsiz) {
	time_t t = -1; /* can't parse */
	char tz[64] = "";
	struct tm tm;
	char *formats[] = {
		"%a, %d %b %Y %H:%M:%S",
		"%Y-%m-%d %H:%M:%S",
		"%Y-%m-%dT%H:%M:%S",
		NULL
	};
	char *p;
	unsigned int i;

	if(buf)
		buf[0] = '\0';
	memset(&tm, 0, sizeof(tm));
	for(i = 0; formats[i]; i++) {
		if((p = strptime(s, formats[i], &tm))) {
			tm.tm_isdst = -1; /* don't use DST */
			if((t = mktime(&tm)) == -1) /* error */
				return t;
			t -= gettimetz(p, tz, sizeof(tz));
			if(buf)
				snprintf(buf, bufsiz, "%04d-%02d-%02d %02d:%02d:%02d %-.16s",
				         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				         tm.tm_hour, tm.tm_min, tm.tm_sec, tz);
			break;
		}
	}
	return t;
}

/* print text, escape tabs, newline and carriage return etc */
static void
string_print(String *s) {
	const char *p;
/*	char buffer[BUFSIZ + 4];
	size_t i;*/

	if(!s->len)
		return;
	/* skip leading whitespace */
	for(p = s->data; *p && isspace((int)*p); p++);
	for(; *p; p++) {
		if(ISWSNOSPACE(*p)) {
			switch(*p) {
			case '\n': fputs("\\n", stdout); break;
			case '\\': fputs("\\\\", stdout); break;
			case '\t': fputs("\\t", stdout); break;
			default: break; /* ignore other whitespace chars */
			}
		} else
			putchar(*p);
	}
#if 0
	/* NOTE: optimized string output, re-test this later */
	for(i = 0; *p; p++) {
		if(ISWSNOSPACE(*p)) { /* isspace(c) && c != ' ' */
			if(*p == '\n') { /* escape newline */
				buffer[i++] = '\\';
				buffer[i++] = 'n';
			} else if(*p == '\\') { /* escape \ */
				buffer[i++] = '\\';
				buffer[i++] = '\\';
			} else if(*p == '\t') { /* tab */
				buffer[i++] = '\\';
				buffer[i++] = 't';
			}
			/* ignore other whitespace chars, except space */
		} else {
			buffer[i++] = *p;
		}
		if(i >= BUFSIZ) { /* align write size with BUFSIZ */
			fwrite(buffer, 1, BUFSIZ, stdout);
			i -= BUFSIZ;
		}
	}
	if(i) /* write remaining */
		fwrite(buffer, 1, i, stdout);
#endif
}

static int
istag(const char *name, size_t len, const char *name2, size_t len2) {
	return (len == len2 && !strcasecmp(name, name2));
}

static int
isattr(const char *name, size_t len, const char *name2, size_t len2) {
	return (len == len2 && !strcasecmp(name, name2));
}

/* NOTE: this handler can be called multiple times if the data in this
 * block is bigger than the buffer */
static void
xml_handler_data(XMLParser *p, const char *s, size_t len) {
	if(ctx.field) {
		/* add only data from <name> inside <author> tag
		 * or any other non-<author> tag  */
		if(ctx.tagid != AtomTagAuthor || !strcmp(p->tag, "name"))
			string_append(ctx.field, s, len);
	}
}

static void
xml_handler_cdata(XMLParser *p, const char *s, size_t len) {
	if(ctx.field)
		string_append(ctx.field, s, len);
}

static void
xml_handler_attr_start(struct xmlparser *p, const char *tag, size_t taglen,
                       const char *name, size_t namelen) {
	if(ctx.iscontent && !ctx.iscontenttag) {
		if(!ctx.attrcount)
			xml_handler_data(p, " ", 1);
		ctx.attrcount++;
		xml_handler_data(p, name, namelen);
		xml_handler_data(p, "=\"", 2);
		return;
	}
}

static void
xml_handler_attr_end(struct xmlparser *p, const char *tag, size_t taglen,
                     const char *name, size_t namelen) {
	if(ctx.iscontent && !ctx.iscontenttag) {
		xml_handler_data(p, "\"", 1);
		ctx.attrcount = 0;
	}
}

static void
xml_handler_start_element_parsed(XMLParser *p, const char *tag, size_t taglen,
                                 int isshort) {
	if(ctx.iscontent && !ctx.iscontenttag) {
		if(isshort)
			xml_handler_data(p, "/>", 2);
		else
			xml_handler_data(p, ">", 1);
	}
}

static void
xml_handler_attr(XMLParser *p, const char *tag, size_t taglen,
                 const char *name, size_t namelen, const char *value,
                 size_t valuelen) {
	if(ctx.iscontent && !ctx.iscontenttag) {
		xml_handler_data(p, value, valuelen);
		return;
	}
	if(ctx.item.feedtype == FeedTypeAtom) {
		/*if(ctx.tagid == AtomTagContent || ctx.tagid == AtomTagSummary) {*/
		if(ctx.iscontenttag) {
			if(isattr(name, namelen, "type", strlen("type")) &&
			   (isattr(value, valuelen, "xhtml", strlen("xhtml")) ||
			   isattr(value, valuelen, "text/xhtml", strlen("text/xhtml")) ||
			   isattr(value, valuelen, "html", strlen("html")) ||
			   isattr(value, valuelen, "text/html", strlen("text/html"))))
			{
				ctx.item.contenttype = ContentTypeHTML;
				ctx.iscontent = 1;
/*				p->xmldataentity = NULL;*/
				p->xmlattrstart = xml_handler_attr_start;
				p->xmlattrend = xml_handler_attr_end;
				p->xmltagstartparsed = xml_handler_start_element_parsed;
			}
		} else if(ctx.tagid == AtomTagLink &&
		          isattr(name, namelen, "href", strlen("href")))
		{
			/* link href attribute */
			string_append(&ctx.item.link, value, valuelen);
		}
	}
}

static void
xml_handler_start_element(XMLParser *p, const char *name, size_t namelen) {
	if(ctx.iscontenttag) {
		/* starts with div, handle as XML, don't convert entities */
		/* TODO: test properly */
		if(ctx.item.feedtype == FeedTypeAtom &&
		   !strncmp(name, "div", strlen("div")))
			p->xmldataentity = NULL;
	}
	if(ctx.iscontent) {
		ctx.attrcount = 0;
		ctx.iscontenttag = 0;
		xml_handler_data(p, "<", 1);
		xml_handler_data(p, name, namelen);
		return;
	}

	/* TODO: cleanup, merge with code below ?, return function if FeedTypeNone */
/*	ctx.iscontenttag = 0;*/

	/* start of RSS or Atom item / entry */
	if(ctx.item.feedtype == FeedTypeNone) {
		if(istag(name, namelen, "entry", strlen("entry"))) { /* Atom */
			ctx.item.feedtype = FeedTypeAtom;
			ctx.item.contenttype = ContentTypePlain; /* Default content type */
			ctx.field = NULL; /* XXX: optimization */
		} else if(istag(name, namelen, "item", strlen("item"))) { /* RSS */
			ctx.item.feedtype = FeedTypeRSS;
			ctx.item.contenttype = ContentTypeHTML; /* Default content type */
			ctx.field = NULL; /* XXX: optimization */
		}
		return;
	}

	/* tag already set: return */
	if(ctx.tag[0] != '\0')
		return;
	/* in item */
	if(namelen >= sizeof(ctx.tag) - 2) /* check overflow */
		return;
	memcpy(ctx.tag, name, namelen + 1); /* copy including nul byte */
	ctx.taglen = namelen;
	ctx.tagid = gettag(ctx.item.feedtype, ctx.tag, ctx.taglen);
	if(ctx.tagid == TagUnknown)
		ctx.field = NULL;
	if(ctx.item.feedtype == FeedTypeRSS) {
		if(ctx.tagid == RSSTagPubdate || ctx.tagid == RSSTagDcdate)
			ctx.field = &ctx.item.timestamp;
		else if(ctx.tagid == RSSTagTitle)
			ctx.field = &ctx.item.title;
		else if(ctx.tagid == RSSTagLink)
			ctx.field = &ctx.item.link;
		else if(ctx.tagid == RSSTagDescription ||
				ctx.tagid == RSSTagContentencoded) {
			/* clear content, assumes previous content was not a summary text */
			if(ctx.tagid == RSSTagContentencoded && ctx.item.content.len)
				string_clear(&ctx.item.content);
			/* ignore, prefer content:encoded over description */
			if(!(ctx.tagid == RSSTagDescription && ctx.item.content.len)) {
				ctx.iscontenttag = 1;
				ctx.field = &ctx.item.content;
			}
		} else if(ctx.tagid == RSSTagGuid)
			ctx.field = &ctx.item.id;
		else if(ctx.tagid == RSSTagAuthor || ctx.tagid == RSSTagDccreator)
			ctx.field = &ctx.item.author;
	} else if(ctx.item.feedtype == FeedTypeAtom) {
		if(ctx.tagid == AtomTagPublished || ctx.tagid == AtomTagUpdated)
			ctx.field = &ctx.item.timestamp;
		else if(ctx.tagid == AtomTagTitle)
			ctx.field = &ctx.item.title;
		else if(ctx.tagid == AtomTagSummary || ctx.tagid == AtomTagContent) {
			/* clear content, assumes previous content was not a summary text */
			if(ctx.tagid == AtomTagContent && ctx.item.content.len)
				string_clear(&ctx.item.content);
			/* ignore, prefer content:encoded over description */
			if(!(ctx.tagid == AtomTagSummary && ctx.item.content.len)) {
				ctx.iscontenttag = 1;
				ctx.field = &ctx.item.content;
			}
		} else if(ctx.tagid == AtomTagId)
			ctx.field = &ctx.item.id;
		else if(ctx.tagid == AtomTagLink)
			ctx.field = &ctx.item.link;
		else if(ctx.tagid == AtomTagAuthor)
			ctx.field = &ctx.item.author;
	}
}

static void
xml_handler_data_entity(XMLParser *p, const char *data, size_t datalen) {
	char buffer[16];
	size_t len;

	/* try to translate entity, else just pass as data */
	if((len = entitytostr(data, buffer, sizeof(buffer))) > 0)
		xml_handler_data(p, buffer, len);
	else
		xml_handler_data(p, data, datalen);
}

static void
xml_handler_end_element(XMLParser *p, const char *name, size_t namelen, int isshort) {
	char timebuf[64];
	int tagid;

	if(ctx.iscontent) {
		ctx.attrcount = 0;
		/* TODO: optimize */
		tagid = gettag(ctx.item.feedtype, name, namelen);
		if(ctx.tagid == tagid) { /* close content */
			ctx.iscontent = 0;
			ctx.iscontenttag = 0;

			p->xmldataentity = xml_handler_data_entity;
			p->xmlattrstart = NULL;
			p->xmlattrend = NULL;
			p->xmltagstartparsed = NULL;

			ctx.tag[0] = '\0'; /* unset tag */
			ctx.taglen = 0;
			ctx.tagid = TagUnknown;

			return; /* TODO: not sure if !isshort check below should be skipped */
		}
		if(!isshort) {
			xml_handler_data(p, "</", 2);
			xml_handler_data(p, name, namelen);
			xml_handler_data(p, ">", 1);
		}
		return;
	}
	if(ctx.item.feedtype == FeedTypeNone)
		return;
	/* end of RSS or Atom entry / item */
	/* TODO: optimize, use gettag() ? to tagid? */
	if((ctx.item.feedtype == FeedTypeAtom &&
	   istag(name, namelen, "entry", strlen("entry"))) || /* Atom */
	   (ctx.item.feedtype == FeedTypeRSS &&
	   istag(name, namelen, "item", strlen("item")))) /* RSS */
	{
		printf("%ld", (long)parsetime((&ctx.item.timestamp)->data,
					  timebuf, sizeof(timebuf)));
		putchar(FieldSeparator);
		fputs(timebuf, stdout);
		putchar(FieldSeparator);
		string_print(&ctx.item.title);
		putchar(FieldSeparator);
		string_print(&ctx.item.link);
		putchar(FieldSeparator);
		string_print(&ctx.item.content);
		putchar(FieldSeparator);
		fputs(contenttypes[ctx.item.contenttype], stdout);
		putchar(FieldSeparator);
		string_print(&ctx.item.id);
		putchar(FieldSeparator);
		string_print(&ctx.item.author);
		putchar(FieldSeparator);
		fputs(feedtypes[ctx.item.feedtype], stdout);
		if(append) {
			putchar(FieldSeparator);
			fputs(append, stdout);
		}
		putchar('\n');

		/* clear strings */
		string_clear(&ctx.item.timestamp);
		string_clear(&ctx.item.title);
		string_clear(&ctx.item.link);
		string_clear(&ctx.item.content);
		string_clear(&ctx.item.id);
		string_clear(&ctx.item.author);
		ctx.item.feedtype = FeedTypeNone;
		ctx.item.contenttype = ContentTypePlain;
		ctx.tag[0] = '\0'; /* unset tag */
		ctx.taglen = 0;
		ctx.tagid = TagUnknown;

		/* not sure if needed */
		ctx.iscontenttag = 0;
		ctx.iscontent = 0;
	} else if(!strcmp(ctx.tag, name)) { /* clear */ /* XXX: optimize ? */
		ctx.field = NULL;
		ctx.tag[0] = '\0'; /* unset tag */
		ctx.taglen = 0;
		ctx.tagid = TagUnknown;

		/* not sure if needed */
		ctx.iscontenttag = 0;
		ctx.iscontent = 0;
	}
}

int
main(int argc, char **argv) {
	atexit(cleanup);

	if(argc > 1)
		append = argv[1];

	memset(&ctx, 0, sizeof(ctx));

	/* init strings and initial memory pool size */
	string_buffer_init(&ctx.item.timestamp, 64);
	string_buffer_init(&ctx.item.title, 256);
	string_buffer_init(&ctx.item.link, 1024);
	string_buffer_init(&ctx.item.content, 4096);
	string_buffer_init(&ctx.item.id, 1024);
	string_buffer_init(&ctx.item.author, 256);
	ctx.item.contenttype = ContentTypePlain;
	ctx.item.feedtype = FeedTypeNone;

	xmlparser_init(&parser, stdin);
	parser.xmltagstart = xml_handler_start_element;
	parser.xmltagend = xml_handler_end_element;
	parser.xmldata = xml_handler_data;
	parser.xmldataentity = xml_handler_data_entity;
	parser.xmlattr = xml_handler_attr;
	parser.xmlcdata = xml_handler_cdata;
	xmlparser_parse(&parser);

	return EXIT_SUCCESS;
}
