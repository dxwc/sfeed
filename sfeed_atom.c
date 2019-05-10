#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

static char *line;
static size_t linesize;

static void
printcontent(const char *s)
{
	for (; *s; ++s) {
		switch (*s) {
		case '<':  fputs("&lt;",   stdout); break;
		case '>':  fputs("&gt;",   stdout); break;
		case '\'': fputs("&#39;",  stdout); break;
		case '&':  fputs("&amp;",  stdout); break;
		case '"':  fputs("&quot;", stdout); break;
		case '\\':
			s++;
			switch (*s) {
			case 'n':  putchar('\n'); break;
			case '\\': putchar('\\'); break;
			case 't':  putchar('\t'); break;
			}
			break;
		default:  putchar(*s);
		}
	}
}

static void
printfeed(FILE *fp, const char *feedname)
{
	char *fields[FieldLast];
	struct tm *tm;
	time_t parsedtime;
	ssize_t linelen;

	while ((linelen = getline(&line, &linesize, fp)) > 0) {
		if (line[linelen - 1] == '\n')
			line[--linelen] = '\0';
		if (!parseline(line, fields))
			break;

		parsedtime = 0;
		if (strtotime(fields[FieldUnixTimestamp], &parsedtime))
			continue;
		if (!(tm = localtime(&parsedtime)))
			err(1, "localtime");

		fputs("<entry>\n\t<title>", stdout);
		if (feedname[0]) {
			fputs("[", stdout);
			xmlencode(feedname, stdout);
			fputs("] ", stdout);
		}
		xmlencode(fields[FieldTitle], stdout);
		fputs("</title>\n", stdout);
		if (fields[FieldLink][0]) {
			fputs("\t<link rel=\"alternate\" href=\"", stdout);
			xmlencode(fields[FieldLink], stdout);
			fputs("\" />\n", stdout);
		}
		if (fields[FieldEnclosure][0]) {
			fputs("\t<link rel=\"enclosure\" href=\"", stdout);
			xmlencode(fields[FieldEnclosure], stdout);
			fputs("\" />\n", stdout);
		}
		fprintf(stdout, "\t<published>%04d-%02d-%02dT%02d:%02d:%02dZ</published>\n",
		        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		        tm->tm_hour, tm->tm_min, tm->tm_sec);
		if (fields[FieldAuthor][0]) {
			fputs("\t<author><name>", stdout);
			xmlencode(fields[FieldAuthor], stdout);
			fputs("</name></author>\n", stdout);
		}
		if (fields[FieldContent][0]) {
			if (!strcmp(fields[FieldContentType], "html")) {
				fputs("\t<content type=\"html\">", stdout);
			} else {
				/* NOTE: an RSS/Atom viewer may or may not format
				   whitespace such as newlines.
				   Workaround: type="html" and <![CDATA[<pre></pre>]]> */
				fputs("\t<content type=\"text\">", stdout);
			}
			printcontent(fields[FieldContent]);
			fputs("</content>\n", stdout);
		}
		fputs("</entry>\n", stdout);
	}
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *name;
	int i;

	if (argc == 1) {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
	}

	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	      "<feed xmlns=\"http://www.w3.org/2005/Atom\" xml:lang=\"en\">\n",
	      stdout);

	if (argc == 1) {
		printfeed(stdin, "");
	} else {
		for (i = 1; i < argc; i++) {
			if (!(fp = fopen(argv[i], "r")))
				err(1, "fopen: %s", argv[i]);
			name = ((name = strrchr(argv[i], '/'))) ? name + 1 : argv[i];
			printfeed(fp, name);
			if (ferror(fp))
				err(1, "ferror: %s", argv[i]);
			fclose(fp);
		}
	}

	fputs("</feed>\n", stdout);

	return 0;
}
