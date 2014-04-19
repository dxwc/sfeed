#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <errno.h>
#include <limits.h>

#include "util.h"

static unsigned int showsidebar = 1; /* show sidebar ? */

static FILE *fpindex = NULL, *fpitems = NULL, *fpmenu = NULL;
static FILE *fpcontent = NULL;
static char *line = NULL;
static struct feed *feeds = NULL;

/* print string to stderr and exit program with EXIT_FAILURE */
static void
die(const char *s) {
	fputs("sfeed_frames: ", stderr);
	fputs(s, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

static void
cleanup(void) {
	if(fpmenu)
		fclose(fpmenu);
	if(fpitems)
		fclose(fpitems);
	if(fpindex)
		fclose(fpindex);
	if(fpcontent)
		fclose(fpcontent);
	free(line); /* free line */
	feedsfree(feeds); /* free feeds linked-list */
}

/* print text, ignore tabs, newline and carriage return etc
 * print some HTML 2.0 / XML 1.0 as normal text */
static void
printcontent(const char *s, FILE *fp) {
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
makepathname(const char *path, char *buffer, size_t bufsiz) {
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

static int
fileexists(const char *path) {
	return (!access(path, F_OK));
}

int
main(int argc, char **argv) {
	struct feed *f, *fcur = NULL;
	char *fields[FieldLast];
	char name[64]; /* TODO: bigger size? */
	char *basepath = ".";
	char dirpath[PATH_MAX], filepath[PATH_MAX];
	char reldirpath[PATH_MAX], relfilepath[PATH_MAX];
	char *feedname = "";
	unsigned long totalfeeds = 0, totalnew = 0;
	unsigned int isnew;
	time_t parsedtime, comparetime;
	size_t linesize = 0, namelen, basepathlen;
	struct stat st;
	struct utimbuf contenttime;

	atexit(cleanup);
	memset(&contenttime, 0, sizeof(contenttime));

	if(argc > 1 && argv[1][0] != '\0')
		basepath = argv[1];

	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	basepathlen = strlen(basepath);
	if(basepathlen > 0)
		mkdir(basepath, S_IRWXU);

	/* write main index page */
	if(snprintf(dirpath, sizeof(dirpath), "%s/index.html", basepath) <= 0)
		die("snprintf() format error");
	if(!(fpindex = fopen(dirpath, "w+b")))
		die("can't write index.html");
	if(snprintf(dirpath, sizeof(dirpath), "%s/menu.html", basepath) <= 0)
		die("snprintf() format error");
	if(!(fpmenu = fopen(dirpath, "w+b")))
		die("can't write menu.html");
	if(snprintf(dirpath, sizeof(dirpath), "%s/items.html", basepath) <= 0)
		die("snprintf() format error");
	if(!(fpitems = fopen(dirpath, "w+b")))
		die("can't write items.html");
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
			/* TODO: makepathname isnt necesary if fields[FieldFeedName] is the same as the previous line */
			/* TODO: move this part below where FieldFeedName is checked if its different ? */

			/* make directory for feedname */
			if(!(namelen = makepathname(feedname, name, sizeof(name))))
				continue;

			if(snprintf(dirpath, sizeof(dirpath), "%s/%s", basepath, name) <= 0)
				die("snprintf() format error");

			/* directory doesn't exist: try to create it. */
			if(stat(dirpath, &st) == -1) {
				if(mkdir(dirpath, S_IRWXU) == -1) {
					fprintf(stderr, "sfeed_frames: can't make directory '%s': %s\n",
					        dirpath, strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			strlcpy(reldirpath, name, sizeof(reldirpath));

			if(!(f = calloc(1, sizeof(struct feed))))
				die("can't allocate enough memory");

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
				die("can't allocate enough memory");
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
		if(snprintf(filepath, sizeof(filepath), "%s/%s.html", dirpath, name) <= 0)
			die("snprintf() format error");
		if(snprintf(relfilepath, sizeof(relfilepath), "%s/%s.html", reldirpath, name) <= 0)
			die("snprintf() format error");
		if(!fileexists(filepath) && (fpcontent = fopen(filepath, "w+b"))) {
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
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		/* set modified and access time of file to time of item. */
		contenttime.actime = parsedtime;
		contenttime.modtime = parsedtime;
		utime(filepath, &contenttime);

		isnew = (parsedtime >= comparetime);
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

	return EXIT_SUCCESS;
}
