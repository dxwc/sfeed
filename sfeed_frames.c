#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "util.h"

static int showsidebar = 1; /* show sidebar ? */
static FILE *fpindex = NULL, *fpitems = NULL, *fpmenu = NULL;
static FILE *fpcontent = NULL;
static char *line = NULL;
static struct feed *feeds = NULL;

static void
cleanup(void)
{
	if(fpmenu)
		fclose(fpmenu);
	if(fpitems)
		fclose(fpitems);
	if(fpindex)
		fclose(fpindex);
	if(fpcontent)
		fclose(fpcontent);
	fpmenu = NULL;
	fpitems = NULL;
	fpindex = NULL;
	fpcontent = NULL;
}

/* same as err() but first call cleanup() function */
static void
xerr(int eval, const char *fmt, ...)
{
	int saved_errno = errno;
	va_list ap;

	cleanup();

	errno = saved_errno;
	va_start(ap, fmt);
	verr(eval, fmt, ap);
	va_end(ap);
}

static int
esnprintf(char *str, size_t size, const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vsnprintf(str, size, fmt, ap);
	va_end(ap);

	if(r == -1 || (size_t)r >= size)
		xerr(1, "snprintf");

	return r;
}

/* print text, ignore tabs, newline and carriage return etc
 * print some HTML 2.0 / XML 1.0 as normal text */
static void
printcontent(const char *s, FILE *fp)
{
	const char *p;

	for(p = s; *p; p++) {
		if(*p == '\\') {
			p++;
			if(*p == '\\')
				fputc('\\', fp);
			else if(*p == 't')
				fputc('\t', fp);
			else if(*p == 'n')
				fputc('\n', fp);
			else
				fputc(*p, fp); /* unknown */
		} else {
			fputc(*p, fp);
		}
	}
}

/* TODO: bufsiz - 1 ? */
static size_t
makepathname(const char *path, char *buffer, size_t bufsiz)
{
	size_t i = 0, r = 0;

	for(; *path && i < bufsiz - 1; path++) {
		if(isalpha((int)*path) || isdigit((int)*path)) {
			buffer[i++] = tolower((int)*path);
			r = 0;
		} else {
			if(!r) /* don't repeat '-'. */
				buffer[i++] = '-';
			r++;
		}
	}
	buffer[i] = '\0';
	/* remove trailing - */
	for(; i > 0 && (buffer[i] == '-' || buffer[i] == '\0'); i--)
		buffer[i] = '\0';
	return i;
}

int
main(int argc, char *argv[])
{
	struct feed *f, *fcur = NULL;
	char *fields[FieldLast];
	char name[64]; /* TODO: bigger size? */
	char dirpath[PATH_MAX], filepath[PATH_MAX];
	char reldirpath[PATH_MAX], relfilepath[PATH_MAX];
	char *feedname = "", *basepath = ".";
	unsigned long totalfeeds = 0, totalnew = 0;
	unsigned int isnew;
	time_t parsedtime, comparetime;
	size_t linesize = 0, namelen, basepathlen;
	struct stat st;
	struct utimbuf contenttime;
	int r;

	memset(&contenttime, 0, sizeof(contenttime));

	if(argc > 1 && argv[1][0] != '\0')
		basepath = argv[1];

	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	basepathlen = strlen(basepath);
	if(basepathlen > 0)
		mkdir(basepath, S_IRWXU);
	/* write main index page */
	esnprintf(dirpath, sizeof(dirpath), "%s/index.html", basepath);
	if(!(fpindex = fopen(dirpath, "w+b")))
		xerr(1, "fopen");
	esnprintf(dirpath, sizeof(dirpath), "%s/menu.html", basepath);
	if(!(fpmenu = fopen(dirpath, "w+b")))
		xerr(1, "fopen");
	esnprintf(dirpath, sizeof(dirpath), "%s/items.html", basepath);
	if(!(fpitems = fopen(dirpath, "w+b")))
		xerr(1, "fopen");
	fputs("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />"
	      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" /></head>"
	      "<body class=\"frame\"><div id=\"items\">", fpitems);

	while(parseline(&line, &linesize, fields, FieldLast, '\t', stdin) > 0) {
		feedname = fields[FieldFeedName];
		if(feedname[0] == '\0') {
			feedname = "unknown";
			/* assume single feed (hide sidebar) */
			if(!totalfeeds)
				showsidebar = 0;
		}
		/* first of feed section or new feed section (differ from previous). */
		if(!totalfeeds || (fcur && strcmp(fcur->name, feedname))) {
			/* TODO: makepathname isn't necesary if fields[FieldFeedName] is the same as the previous line */
			/* TODO: move this part below where FieldFeedName is checked if it's different ? */

			/* make directory for feedname */
			if(!(namelen = makepathname(feedname, name, sizeof(name))))
				continue;

			esnprintf(dirpath, sizeof(dirpath), "%s/%s", basepath, name);

			/* directory doesn't exist: try to create it. */
			if(stat(dirpath, &st) == -1 && mkdir(dirpath, S_IRWXU) == -1)
				xerr(1, "sfeed_frames: can't make directory '%s'", dirpath);
			strlcpy(reldirpath, name, sizeof(reldirpath)); /* TODO: check truncation */

			if(!(f = calloc(1, sizeof(struct feed))))
				xerr(1, "calloc");

			if(totalfeeds) { /* end previous one. */
				fputs("</table>\n", fpitems);
				fcur->next = f;
				fcur = fcur->next;
			} else {
				/* first item. */
				fcur = f;
				feeds = fcur;
			}
			/* write menu link if new. */
			if(!(fcur->name = strdup(feedname)))
				xerr(1, "strdup");
			if(fields[FieldFeedName][0] != '\0') {
				fputs("<h2 id=\"", fpitems);
				printfeednameid(fcur->name, fpitems);
				fputs("\"><a href=\"#", fpitems);
				printfeednameid(fcur->name, fpitems);
				fputs("\">", fpitems);
				fputs(fcur->name, fpitems);
				fputs("</a></h2>\n", fpitems);
			}
			fputs("<table cellpadding=\"0\" cellspacing=\"0\">\n", fpitems);
			totalfeeds++;
		}
		/* write content */
		if(!(namelen = makepathname(fields[FieldTitle], name, sizeof(name))))
			continue;
		esnprintf(filepath, sizeof(filepath), "%s/%s.html", dirpath, name);
		esnprintf(relfilepath, sizeof(relfilepath), "%s/%s.html", reldirpath, name);

		/* file doesn't exist yet and has write access */
		if(access(filepath, F_OK) != 0) {
			if(!(fpcontent = fopen(filepath, "w+b")))
				xerr(1, "fopen");
			fputs("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../style.css\" />"
			      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" /></head>\n"
			      "<body class=\"frame\"><div class=\"content\">"
			      "<h2><a href=\"", fpcontent);
			if(fields[FieldBaseSiteUrl][0] != '\0')
				printlink(fields[FieldLink], fields[FieldBaseSiteUrl], fpcontent);
			else
				printlink(fields[FieldLink], fields[FieldFeedUrl], fpcontent);
			fputs("\">", fpcontent);
			printhtmlencoded(fields[FieldTitle], fpcontent);
			fputs("</a></h2>", fpcontent);
			printcontent(fields[FieldContent], fpcontent);
			fputs("</div></body></html>", fpcontent);
			fclose(fpcontent);
			fpcontent = NULL;
		}

		/* write item. */
		r = strtotime(fields[FieldUnixTimestamp], &parsedtime);

		/* set modified and access time of file to time of item. */
		if(r != -1) {
			contenttime.actime = parsedtime;
			contenttime.modtime = parsedtime;
			utime(filepath, &contenttime);
		}

		isnew = (r != -1 && parsedtime >= comparetime) ? 1 : 0;
		totalnew += isnew;
		fcur->totalnew += isnew;
		fcur->total++;
		if(isnew)
			fputs("<tr class=\"n\">", fpitems);
		else
			fputs("<tr>", fpitems);
		fputs("<td nowrap valign=\"top\">", fpitems);
		fputs(fields[FieldTimeFormatted], fpitems);
		fputs("</td><td nowrap valign=\"top\">", fpitems);
		if(isnew)
			fputs("<b><u>", fpitems);
		fputs("<a href=\"", fpitems);
		fputs(relfilepath, fpitems);
		fputs("\" target=\"content\">", fpitems);
		printhtmlencoded(fields[FieldTitle], fpitems);
		fputs("</a>", fpitems);
		if(isnew)
			fputs("</u></b>", fpitems);
		fputs("</td></tr>\n", fpitems);
	}
	if(totalfeeds) {
		fputs("</table>\n", fpitems);
	}
	fputs("\n</div></body>\n</html>", fpitems); /* div items */
	if(showsidebar) {
		fputs("<html><head>"
		      "<link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />\n"
		      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		      "</head><body class=\"frame\"><div id=\"sidebar\">", fpmenu);
		for(fcur = feeds; fcur; fcur = fcur->next) {
			if(!fcur->name || fcur->name[0] == '\0')
				continue;
			if(fcur->totalnew)
				fputs("<a class=\"n\" href=\"items.html#", fpmenu);
			else
				fputs("<a href=\"items.html#", fpmenu);
			printfeednameid(fcur->name, fpmenu);
			fputs("\" target=\"items\">", fpmenu);
			if(fcur->totalnew > 0)
				fputs("<b><u>", fpmenu);
			fputs(fcur->name, fpmenu);
			fprintf(fpmenu, " (%lu)", fcur->totalnew);
			if(fcur->totalnew > 0)
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
	if(showsidebar) {
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

	cleanup();

	return 0;
}
