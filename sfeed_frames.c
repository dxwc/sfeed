#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "util.h"

static char *line = NULL;
size_t linesize = 0;

static struct utimbuf contenttime;
static time_t comparetime;
static unsigned long totalnew = 0;

static struct feed **feeds = NULL;

/* Unescape / decode fields printed by string_print_encoded()
 * "\\" to "\", "\t", to TAB, "\n" to newline. Unrecognised escape sequences
 * are ignored: "\z" etc. Call `fn` on each escaped character. */
void
printcontent(const char *s, FILE *fp)
{
	for (; *s; s++) {
		if (*s == '\\') {
			switch (*(++s)) {
			case '\0': return; /* ignore */
			case '\\': fputc('\\', fp); break;
			case 't':  fputc('\t', fp); break;
			case 'n':  fputc('\n', fp); break;
			}
		} else {
			fputc((int)*s, fp);
		}
	}
}

/* normalize path names, transform to lower-case and replace non-alpha and
 * non-digit with '-' */
static size_t
normalizepath(const char *path, char *buf, size_t bufsiz)
{
	size_t i, r = 0;

	for (i = 0; *path && i < bufsiz; path++) {
		if (isalpha((int)*path) || isdigit((int)*path)) {
			buf[i++] = tolower((int)*path);
			r = 0;
		} else {
			/* don't repeat '-', don't start with '-' */
			if (!r && i)
				buf[i++] = '-';
			r++;
		}
	}
	/* remove trailing '-' */
	for (; i > 0 && (buf[i - 1] == '-'); i--)
		;

	if (bufsiz > 0)
		buf[i] = '\0';

	return i;
}

static void
printfeed(FILE *fpitems, FILE *fpin, struct feed *f)
{
	char dirpath[PATH_MAX], filepath[PATH_MAX];
	char *fields[FieldLast], *feedname;
	char name[PATH_MAX];
	size_t namelen;
	struct stat st;
	FILE *fpcontent = NULL;
	unsigned int isnew;
	time_t parsedtime;
	int r;

	if (f->name[0])
		feedname = f->name;
	else
		feedname = "unnamed";

	/* make directory for feedname */
	if (!(namelen = normalizepath(feedname, name, sizeof(name))))
		return;

	if (strlcpy(dirpath, name, sizeof(dirpath)) >= sizeof(dirpath))
		errx(1, "strlcpy: path truncation");
	/* directory doesn't exist: try to create it. */
	if (stat(dirpath, &st) == -1 && mkdir(dirpath, S_IRWXU) == -1)
		err(1, "mkdir: %s", dirpath);

	/* menu if not unnamed */
	if (f->name[0]) {
		fputs("<h2 id=\"", fpitems);
		xmlencode(f->name, fpitems);
		fputs("\"><a href=\"#", fpitems);
		xmlencode(f->name, fpitems);
		fputs("\">", fpitems);
		xmlencode(f->name, fpitems);
		fputs("</a></h2>\n", fpitems);
	}

	fputs("<table cellpadding=\"0\" cellspacing=\"0\">\n", fpitems);
	while (parseline(&line, &linesize, fields, fpin) > 0) {
		/* write content */
		if (!(namelen = normalizepath(fields[FieldTitle], name, sizeof(name))))
			continue;
		r = snprintf(filepath, sizeof(filepath), "%s/%s.html", dirpath, name);
		if (r == -1 || (size_t)r >= sizeof(filepath))
			errx(1, "snprintf: path truncation");

		/* file doesn't exist yet and has write access */
		if (access(filepath, F_OK) != 0) {
			if (!(fpcontent = fopen(filepath, "w+b")))
				err(1, "fopen: %s", filepath);
			fputs("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../style.css\" />"
			      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" /></head>\n"
			      "<body class=\"frame\"><div class=\"content\">"
			      "<h2><a href=\"", fpcontent);
			xmlencode(fields[FieldLink], fpcontent);
			fputs("\">", fpcontent);
			xmlencode(fields[FieldTitle], fpcontent);
			fputs("</a></h2>", fpcontent);
			/* NOTE: this prints the raw HTML of the feed, this is
			 * potentially dangerous, it is up to the user / browser
			 * to trust a feed it's HTML content. */
			printcontent(fields[FieldContent], fpcontent);
			fputs("</div></body></html>", fpcontent);
			fclose(fpcontent);
		}

		/* set modified and access time of file to time of item. */
		r = strtotime(fields[FieldUnixTimestamp], &parsedtime);
		if (r != -1) {
			contenttime.actime = parsedtime;
			contenttime.modtime = parsedtime;
			utime(filepath, &contenttime);
		}

		isnew = (r != -1 && parsedtime >= comparetime) ? 1 : 0;
		totalnew += isnew;
		f->totalnew += isnew;
		f->total++;
		if (isnew)
			fputs("<tr class=\"n\">", fpitems);
		else
			fputs("<tr>", fpitems);
		fputs("<td nowrap valign=\"top\">", fpitems);
		fputs(fields[FieldTimeFormatted], fpitems);
		fputs("</td><td nowrap valign=\"top\">", fpitems);
		if (isnew)
			fputs("<b><u>", fpitems);
		fputs("<a href=\"", fpitems);
		fputs(filepath, fpitems);
		fputs("\" target=\"content\">", fpitems);
		xmlencode(fields[FieldTitle], fpitems);
		fputs("</a>", fpitems);
		if (isnew)
			fputs("</u></b>", fpitems);
		fputs("</td></tr>\n", fpitems);
	}
	fputs("</table>\n", fpitems);
}

int
main(int argc, char *argv[])
{
	FILE *fpindex, *fpitems, *fpmenu, *fp;
	int showsidebar = (argc > 1);
	int i;
	struct feed *f;

	if (!(feeds = calloc(argc, sizeof(struct feed *))))
		err(1, "calloc");

	if ((comparetime = time(NULL)) == -1)
		err(1, "time");
	/* 1 day is old news */
	comparetime -= 86400;

	/* write main index page */
	if (!(fpindex = fopen("index.html", "w+b")))
		err(1, "fopen: index.html");
	if (!(fpmenu = fopen("menu.html", "w+b")))
		err(1, "fopen: menu.html");
	if (!(fpitems = fopen("items.html", "w+b")))
		err(1, "fopen: items.html");
	fputs("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />"
	      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" /></head>"
	      "<body class=\"frame\"><div id=\"items\">", fpitems);

	if (argc == 1) {
		if (!(feeds[0] = calloc(1, sizeof(struct feed))))
			err(1, "calloc");
		feeds[0]->name = "";
		printfeed(fpitems, stdin, feeds[0]);
	} else {
		for (i = 1; i < argc; i++) {
			if (!(feeds[i - 1] = calloc(1, sizeof(struct feed))))
				err(1, "calloc");
			feeds[i - 1]->name = xbasename(argv[i]);

			if (!(fp = fopen(argv[i], "r")))
				err(1, "fopen: %s", argv[i]);
			printfeed(fpitems, fp, feeds[i - 1]);
			if (ferror(fp))
				err(1, "ferror: %s", argv[i]);
			fclose(fp);
		}
	}
	fputs("\n</div></body>\n</html>", fpitems); /* div items */

	if (showsidebar) {
		fputs("<html><head>"
		      "<link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />\n"
		      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		      "</head><body class=\"frame\"><div id=\"sidebar\">", fpmenu);

		for (i = 1; i < argc; i++) {
			f = feeds[i - 1];
			if (f->totalnew)
				fputs("<a class=\"n\" href=\"items.html#", fpmenu);
			else
				fputs("<a href=\"items.html#", fpmenu);
			xmlencode(f->name, fpmenu);
			fputs("\" target=\"items\">", fpmenu);
			if (f->totalnew > 0)
				fputs("<b><u>", fpmenu);
			xmlencode(f->name, fpmenu);
			fprintf(fpmenu, " (%lu)", f->totalnew);
			if (f->totalnew > 0)
				fputs("</u></b>", fpmenu);
			fputs("</a><br/>\n", fpmenu);
		}
		fputs("</div></body></html>", fpmenu);
	}
	fputs("<!DOCTYPE html><html><head>\n\t<title>Newsfeed (", fpindex);
	fprintf(fpindex, "%lu", totalnew);
	fputs(")</title>\n\t<link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />\n"
	      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "</head>\n", fpindex);
	if (showsidebar) {
		fputs("<frameset framespacing=\"0\" cols=\"200,*\" frameborder=\"1\">\n"
		      "	<frame name=\"menu\" src=\"menu.html\" target=\"menu\">\n", fpindex);
	} else {
		fputs("<frameset framespacing=\"0\" cols=\"*\" frameborder=\"1\">\n", fpindex);
	}
	fputs("\t<frameset id=\"frameset\" framespacing=\"0\" cols=\"50%,50%\" frameborder=\"1\">\n"
	      "\t\t<frame name=\"items\" src=\"items.html\" target=\"items\">\n"
	      "\t\t<frame name=\"content\" target=\"content\">\n"
	      "\t</frameset>\n"
	      "</frameset>\n"
	      "</html>", fpindex);

	fclose(fpitems);
	fclose(fpmenu);
	fclose(fpindex);

	return 0;
}
