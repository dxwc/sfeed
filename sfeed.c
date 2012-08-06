#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <expat.h> /* libexpat */

enum { FeedTypeNone = 0, FeedTypeRSS = 1, FeedTypeAtom = 2, FeedTypeLast = 3 };
const char *feedtypes[] = {	"", "rss", "atom" };

enum { ContentTypeNone = 0, ContentTypePlain = 1, ContentTypeHTML = 2, ContentTypeLast = 3 };
const char *contenttypes[] = { "", "plain", "html" };

typedef struct string { /* String data / pool */
	char *data; /* data */
	size_t len; /* string length */
	size_t bufsiz; /* allocated size */
} String;

typedef struct feeditem { /* Feed item */
	String timestamp;
	String title;
	String link;
	String content;
	int contenttype; /* ContentTypePlain or ContentTypeHTML */
	String id;
	String author;
	int feedtype; /* FeedTypeRSS or FeedTypeAtom */
} FeedItem;

void die(const char *s);
void cleanup(void);

const int FieldSeparator = '\t';
FeedItem feeditem; /* data for current feed item */
char tag[1024]; /* current XML tag being parsed. */
char feeditemtag[1024]; /* current tag _inside_ a feeditem */
XML_Parser parser; /* expat XML parser state */
int incdata = 0;
char *standardtz = NULL; /* TZ variable at start of program */

void
string_clear(String *s) {
	if(s->data)
		s->data[0] = '\0'; /* clear string only; don't free, prevents
		                      unnecessary reallocation */
	s->len = 0;
}

void
string_buffer_init(String *s, size_t len) {
	if(!(s->data = malloc(len)))
		die("can't allocate enough memory");
	s->bufsiz = len;
	string_clear(s);
}

void
string_free(String *s) {
	free(s->data);
	s->data = NULL;
	s->bufsiz = 0;
	s->len = 0;
}

int
string_buffer_expand(String *s, size_t newlen) {
	char *p;
	size_t alloclen;
	/* check if allocation is necesary, dont shrink buffer */
	if(!s->data || (newlen > s->bufsiz)) {
		/* should be more than bufsiz ofcourse */
		for(alloclen = 16; alloclen <= newlen; alloclen *= 2);
		if(!(p = realloc(s->data, alloclen))) {
			string_free(s); /* free previous allocation */
			die("can't allocate enough memory");
		}
		s->bufsiz = alloclen;
		s->data = p;
	}
	return s->bufsiz;
}

void
string_append(String *s, const char *data, size_t len) {
	string_buffer_expand(s, s->len + len);
	memcpy(s->data + s->len, data, len);
	s->len += len;
	s->data[s->len] = '\0';
}

void /* cleanup parser, free allocated memory, etc */
cleanup(void) {
	XML_ParserFree(parser);
	string_free(&feeditem.timestamp);
	string_free(&feeditem.title);
	string_free(&feeditem.link);
	string_free(&feeditem.content);
	string_free(&feeditem.id);
	string_free(&feeditem.author);
}

void /* print error message to stderr */
die(const char *s) {
	fputs("sfeed: ", stderr);
	fputs(s, stderr);
	fputc('\n', stderr);
	cleanup();
	exit(EXIT_FAILURE);
}

void
gettimetz(const char *s, char *buf, size_t bufsiz) {
	const char *p = s;
	int tzhour = 0, tzmin = 0;
	char tzname[128] = "", *t = NULL;
	unsigned int i;

	buf[0] = '\0';
	for(; *p && isspace(*p); p++); /* skip whitespace */
	/* detect time offset, assume time offset isn't specified in the first 18 characters */
	for(i = 0; *p && ((*p != '+' && *p != '-') || i <= 18); p++, i++);

	if(isalpha(*p)) {
		if(*p == 'Z' || *p == 'z') {
			strncpy(buf, "GMT+00:00", bufsiz);
			return;
		} else {
			for(i = 0, t = &tzname[0]; i < (sizeof(tzname) - 1) && (*p && isalpha(*p)); i++)
				*(t++) = *(p++);
			*t = '\0';
		}
	} else
		strncpy(tzname, "GMT", sizeof(tzname) - 1);
	if(!(*p)) {
		strncpy(buf, tzname, bufsiz);
		return;
	}
	/* NOTE: reverses time offsets for TZ */
	if((sscanf(p, "+%02d:%02d", &tzhour, &tzmin)) > 0)
		snprintf(buf, bufsiz, "%s-%02d:%02d", tzname, tzhour, tzmin);
	else if((sscanf(p, "-%02d:%02d", &tzhour, &tzmin)) > 0)
		snprintf(buf, bufsiz, "%s+%02d:%02d", tzname, tzhour, tzmin);
	else if((sscanf(p, "+%02d%02d", &tzhour, &tzmin)) > 0)
		snprintf(buf, bufsiz, "%s-%02d:%02d", tzname, tzhour, tzmin);
	else if((sscanf(p, "-%02d%02d", &tzhour, &tzmin)) > 0)
		snprintf(buf, bufsiz, "%s+%02d:%02d", tzname, tzhour, tzmin);
	else if(sscanf(p, "+%d", &tzhour) > 0)
		snprintf(buf, bufsiz, "%s-%02d:00", tzname, tzhour);
	else if(sscanf(p, "-%d", &tzhour) > 0)
		snprintf(buf, bufsiz, "%s+%02d:00", tzname, tzhour);
}

time_t
parsetime(const char *s, char *buf, size_t bufsiz) {
	struct tm tm = { 0 };
	time_t t = 0;
	char timebuf[64], tz[256], *p;

	if(buf)
		buf[0] = '\0';
	gettimetz(s, tz, sizeof(tz) - 1);
	if(!standardtz || strcmp(standardtz, tz)) {
		if(!strcmp(tz, "")) { /* restore TZ */
			if(standardtz)
				setenv("TZ", standardtz, 1);
			else
				unsetenv("TZ");
		}
		else
			setenv("TZ", tz, 1);
		tzset();
	}
	if((strptime(s, "%Y-%m-%dT%H:%M:%SZ", &tm)) ||
	   (strptime(s, "%Y-%m-%d %H:%M:%S", &tm)) ||
	   (strptime(s, "%a, %d %b %Y %H:%M:%S", &tm)) ||
	   (strptime(s, "%Y-%m-%dT%H:%M:%S", &tm))) {
		tm.tm_isdst = -1; /* detect Daylight Saving Time */
		if((t = mktime(&tm)) == -1)
			t = 0;
		if(buf && (strftime(timebuf, sizeof(timebuf) - 1,
		           "%Y-%m-%d %H:%M:%S", &tm))) {
			for(p = tz; *p; p++) /* print time offset reverse */
				*p = ((*p == '-') ? '+' : (*p == '+' ? '-' : *p));
			snprintf(buf, bufsiz, "%s %s", timebuf, tz);
		}
	}
	return t;
}

/* print text, ignore tabs, newline and carriage return etc
1 * print some HTML 2.0 / XML 1.0 as normal text */
void
string_print_trimmed(String *s) {
	const char *entities[] = {
		"&amp;", "&", "&lt;", "<", "&gt;", ">",	"&apos;", "'", "&quot;", "\"",
		NULL, NULL
	};
	const char *p, *n, **e;
	unsigned int len, found;
	if(!s->data)
		return;
	for(p = s->data; isspace(*p); p++); /* strip leading whitespace */
	for(; *p; ) { /* ignore tabs, newline and carriage return etc */
		if(!isspace(*p) || *p == ' ') {
			if(*p == '<') { /* skip tags */
				if((n = strchr(p, '>')))
					p = n;
				else
					putchar('<');
			} else if(*p == '&') {
				for(e = entities, found = 0; *e; e += 2) {
					len = strlen(*e);
					if(!strncmp(*e, p, len)) { /* compare entities and "replace" */
						fputs(*(e + 1), stdout);
						p += len;
						found = 1;
						break;
					}
				}
				if(found)
					continue;
				else
					putchar('&');
			} else
				fputc(*p, stdout);
		}
		p++;
	}
}

void /* print text, escape tabs, newline and carriage return etc */
string_print_textblock(String *s) {
	const char *p;
	if(!s->data)
		return;
	for(p = s->data; *p && isspace(*p); p++); /* strip leading whitespace */
	for(; *p; p++) {
		if(*p == '\n') /* escape newline */
			fputs("\\n", stdout);
		else if(*p == '\\') /* escape \ */
			fputs("\\\\", stdout);
		else if(*p == '\t') /* tab */
			fputs("\\t", stdout);
		else if(!isspace(*p) || *p == ' ') /* ignore other whitespace chars */
			fputc(*p, stdout);
	}
}

int
istag(const char *name, const char *name2) {
	return (!strcasecmp(name, name2));
}

int
isattr(const char *name, const char *name2) {
	return (!strcasecmp(name, name2));
}

char * /* search for attr value by attr name in attributes list */
getattrvalue(const char **atts, const char *name) {
	const char **attr = NULL, *key, *value;
	if(!atts || !(*atts))
		return NULL;
	for(attr = atts; *attr; ) {
		key = *(attr++);
		value = *(attr++);
		if(key && value && isattr(key, name))
			return (char *)value;
	}
	return NULL;
}

void XMLCALL
xml_handler_start_element(void *data, const char *name, const char **atts) {
	const char *value;

	strncpy(tag, name, sizeof(tag) - 1); /* set tag */
	if(feeditem.feedtype != FeedTypeNone) { /* in item */
		if(feeditem.feedtype == FeedTypeAtom) {
			if(istag(feeditemtag, "content") || istag(feeditemtag, "summary")) {
				XML_DefaultCurrent(parser); /* pass to default handler to process inline HTML etc */
			} else if(istag(name, "link")) { /* link href attribute */
				if((value = getattrvalue(atts, "href")))
					string_append(&feeditem.link, value, strlen(value));
			} else if(istag(name, "content") || istag(name, "summary")) {
				if((value = getattrvalue(atts, "type"))) {  /* content type is HTML or plain text */
					if(!strcasecmp(value, "xhtml") || !strcasecmp(value, "text/xhtml") ||
					   !strcasecmp(value, "html") || !strcasecmp(value, "text/html"))
						feeditem.contenttype = ContentTypeHTML;
				}
			}
		} else if(feeditem.feedtype == FeedTypeRSS) {
			if((istag(feeditemtag, "description") && !feeditem.content.len) || istag(feeditemtag, "content:encoded")) {
				string_clear(&feeditem.content);
				XML_DefaultCurrent(parser); /* pass to default handler to process inline HTML etc */
			}
		}
		if(feeditemtag[0] == '\0') /* set tag if not already set. */
			strncpy(feeditemtag, name, sizeof(feeditemtag) - 1);
	} else { /* start of RSS or Atom entry / item */
		if(istag(name, "entry")) { /* Atom */
			feeditem.feedtype = FeedTypeAtom;
			feeditem.contenttype = ContentTypePlain; /* Default content type */
		} else if(istag(name, "item")) { /* RSS */
			feeditem.feedtype = FeedTypeRSS;
			feeditem.contenttype = ContentTypeHTML; /* Default content type */
		}
	}
}

void XMLCALL
xml_handler_end_element(void *data, const char *name) {
	char timebuf[64];

	if(feeditem.feedtype != FeedTypeNone) {
		/* end of RSS or Atom entry / item */
		if((istag(name, "entry") && (feeditem.feedtype == FeedTypeAtom)) || /* Atom */
		  (istag(name, "item") && (feeditem.feedtype == FeedTypeRSS))) { /* RSS */
			printf("%ld", (long)parsetime((&feeditem.timestamp)->data, timebuf,
			       sizeof(timebuf) - 1));
			fputc(FieldSeparator, stdout);
			printf("%s", timebuf);
			fputc(FieldSeparator, stdout);
			string_print_trimmed(&feeditem.title);
			fputc(FieldSeparator, stdout);
			string_print_trimmed(&feeditem.link);
			fputc(FieldSeparator, stdout);
			string_print_textblock(&feeditem.content);
			fputc(FieldSeparator, stdout);
			fputs(contenttypes[feeditem.contenttype], stdout);
			fputc(FieldSeparator, stdout);
			string_print_trimmed(&feeditem.id);
			fputc(FieldSeparator, stdout);
			string_print_trimmed(&feeditem.author);
			fputc(FieldSeparator, stdout);
			fputs(feedtypes[feeditem.feedtype], stdout);
			fputc('\n', stdout);

			/* clear strings */
			string_clear(&feeditem.timestamp);
			string_clear(&feeditem.title);
			string_clear(&feeditem.link);
			string_clear(&feeditem.content);
			string_clear(&feeditem.id);
			string_clear(&feeditem.author);
			feeditem.feedtype = FeedTypeNone;
			feeditem.contenttype = ContentTypePlain;
			incdata = 0;
			feeditemtag[0] = '\0'; /* unset tag */
		} else if(!strcmp(feeditemtag, name)) { /* clear */
			feeditemtag[0] = '\0'; /* unset tag */
		} else {
			if(feeditem.feedtype == FeedTypeAtom) {
				if(istag(feeditemtag, "content") || istag(feeditemtag, "summary")) {
					/* pass to default handler to process inline HTML etc */
					XML_DefaultCurrent(parser);
					return;
				}
			}
		}
	}
	tag[0] = '\0'; /* unset tag */
}

/* NOTE: this handler can be called multiple times if the data in this block
 * is bigger than the buffer */
void XMLCALL
xml_handler_data(void *data, const XML_Char *s, int len) {
	if(feeditem.feedtype == FeedTypeRSS) {
		if(istag(feeditemtag, "pubdate") || istag(feeditemtag, "dc:date"))
			string_append(&feeditem.timestamp, s, len);
		else if(istag(feeditemtag, "title"))
			string_append(&feeditem.title, s, len);
		else if(istag(feeditemtag, "link"))
			string_append(&feeditem.link, s, len);
		else if(istag(feeditemtag, "description")) {
			if(incdata)
				XML_DefaultCurrent(parser); /* pass to default handler to process inline HTML etc */
			else
				string_append(&feeditem.content, s, len);
		} else if(istag(feeditemtag, "guid"))
			string_append(&feeditem.id, s, len);
		else if(istag(feeditemtag, "author") || istag(feeditemtag, "dc:creator"))
			string_append(&feeditem.author, s, len);
	} else if(feeditem.feedtype == FeedTypeAtom) {
		if(istag(feeditemtag, "published") || istag(feeditemtag, "updated"))
			string_append(&feeditem.timestamp, s, len);
		else if(istag(feeditemtag, "title")) {
			string_append(&feeditem.title, s, len);
		} else if(istag(feeditemtag, "summary") || istag(feeditemtag, "content")) {
			if(feeditem.contenttype == ContentTypeHTML) {
				if(incdata)
					XML_DefaultCurrent(parser); /* pass to default handler to process inline HTML etc */
				else
					string_append(&feeditem.content, s, len);
			} else
				XML_DefaultCurrent(parser); /* pass to default handler to process inline HTML etc */
		} else if(istag(feeditemtag, "id"))
			string_append(&feeditem.id, s, len);
		else if(istag(feeditemtag, "name")) /* assume this is: <author><name></name></author> */
			string_append(&feeditem.author, s, len);
	}
}

int /* parse XML from stream using setup parser, return 1 on success, 0 on failure. */
xml_parse_stream(XML_Parser parser, FILE *fp) {
	char buffer[BUFSIZ];
	int done = 0, len = 0;

	while(!feof(fp)) {
		len = fread(buffer, 1, sizeof(buffer), fp);
		done = (feof(fp) || ferror(fp));
		if(XML_Parse(parser, buffer, len, done) == XML_STATUS_ERROR && (len > 0)) {
			if(XML_GetErrorCode(parser) == XML_ERROR_NO_ELEMENTS)
				return 1; /* Ignore "no elements found" / empty document as an error */
			fprintf(stderr, "sfeed: error parsing xml %s at line %lu column %lu\n",
			        XML_ErrorString(XML_GetErrorCode(parser)), (unsigned long)XML_GetCurrentLineNumber(parser),
			        (unsigned long)XML_GetCurrentColumnNumber(parser));
			return 0;
		}
	} while(!done);
	return 1;
}

void
xml_handler_default(void *data, const XML_Char *s, int len) {
	if((feeditem.feedtype == FeedTypeAtom && (istag(feeditemtag, "summary") || istag(feeditemtag, "content"))) ||
	   (feeditem.feedtype == FeedTypeRSS && (istag(feeditemtag, "description") || istag(feeditemtag, "content:encoded"))))
		/*if(!istag(tag, "script") && !istag(tag, "style"))*/ /* ignore data in inline script and style */
			string_append(&feeditem.content, s, len);
}

void /* NOTE: data is null terminated. */
xml_handler_comment(void *data, const XML_Char *s) {
}

void
xml_cdata_section_handler_start(void *userdata) {
	incdata = 1;
}

void
xml_cdata_section_handler_end(void *userdata) {
	incdata = 0;
}

int
main(void) {
	int status;
	standardtz = getenv("TZ");

	/* init strings and initial memory pool size */
	string_buffer_init(&feeditem.timestamp, 64);
	string_buffer_init(&feeditem.title, 256);
	string_buffer_init(&feeditem.link, 1024);
	string_buffer_init(&feeditem.content, 4096);
	string_buffer_init(&feeditem.id, 1024);
	string_buffer_init(&feeditem.author, 256);
	feeditem.contenttype = ContentTypePlain;
	feeditem.feedtype = FeedTypeNone;
	feeditemtag[0] = '\0'; /* unset tag */
	tag[0] = '\0'; /* unset tag */

	if(!(parser = XML_ParserCreate("UTF-8")))
		die("can't create parser");

	XML_SetElementHandler(parser, xml_handler_start_element, xml_handler_end_element);
	XML_SetCharacterDataHandler(parser, xml_handler_data);
	XML_SetCommentHandler(parser, xml_handler_comment);
	XML_SetCdataSectionHandler(parser, xml_cdata_section_handler_start, xml_cdata_section_handler_end);
	XML_SetDefaultHandler(parser, xml_handler_default);

	status = xml_parse_stream(parser, stdin);
	cleanup();

	return status ? EXIT_SUCCESS : EXIT_FAILURE;
}
