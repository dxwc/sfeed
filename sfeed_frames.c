#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>

#include "util.h"

static unsigned int showsidebar = 1; /* show sidebar ? */

static FILE *fpindex = NULL, *fpitems = NULL, *fpmenu = NULL, *fpcontent = NULL;
static char *line = NULL;
static struct feed *feeds = NULL; /* start of feeds linked-list. */

static void /* print error message to stderr */
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
	int len = 0;
	for(p = s; *p; p++) {
		if(*p == '\\') {
			p++;
			if(*p == '\\')
				fputc('\\', fp);
			else if(*p == 't' && len)
				fputc('\t', fp);
			else if(*p == 'n' && len)
				fputc('\n', fp);
		} else {
			fputc(*p, fp);
			len++;
		}
	}
}

/* TODO: bufsiz - 1 ? */
static size_t
makepathname(char *buffer, size_t bufsiz, const char *path) {
	const char *p = path;
	size_t i = 0, r = 0;
	for(; *p && i < bufsiz; p++) {
		if(isalpha((int)*p) || isdigit((int)*p)) {
			buffer[i++] = tolower((int)*p);
			r = 0;
		} else {
			if(!r) { /* dont repeat '-'. */
				buffer[i++] = '-';
			}
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
	char *fields[FieldLast];
	char name[256]; /* TODO: bigger size? */
	char *basepath = "feeds";
	 /* TODO: max path size? */
	char dirpath[1024], filepath[1024], reldirpath[1024], relfilepath[1024];
	unsigned long totalfeeds = 0, totalnew = 0;
	unsigned int isnew;
	struct feed *f, *feedcurrent = NULL;
	time_t parsedtime, comparetime;
	size_t size = 0, namelen = 0, basepathlen = 0;

	struct utimbuf contenttime;

	atexit(cleanup);
	memset(&contenttime, 0, sizeof(contenttime));

	if(argc > 1 && argv[1][0] != '\0')
		basepath = argv[1];

	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	mkdir(basepath, S_IRWXU);

	/* write main index page */
	basepathlen = strlen(basepath);
	if(basepathlen + strlen("/index.html") < sizeof(dirpath) - 1)
		sprintf(dirpath, "%s/index.html", basepath);
	if(!(fpindex = fopen(dirpath, "w+b")))
		die("can't write index.html");
	if(basepathlen + strlen("/menu.html") < sizeof(dirpath) - 1)
		sprintf(dirpath, "%s/menu.html", basepath);
	if(!(fpmenu = fopen(dirpath, "w+b")))
		die("can't write menu.html");
	if(basepathlen + strlen("/items.html") < sizeof(dirpath) - 1)
		sprintf(dirpath, "%s/items.html", basepath);
	if(!(fpitems = fopen(dirpath, "w+b")))
		die("can't write items.html");
	fputs("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" /></head>"
	      "<body class=\"frame\"><div id=\"items\">", fpitems);
	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {


/*

		dirpath[0] = '\0';
		filepath[0] = '\0';
		reldirpath[0] = '\0';
		relfilepath[0] = '\0';


*/







		/* first of feed section or new feed section. */
		if(!totalfeeds || strcmp(feedcurrent->name, fields[FieldFeedName])) {


			/* TODO: makepathname isnt necesary if fields[FieldFeedName] is the same as the previous line */
			/* TODO: move this part below where FieldFeedName is checked if its different ? */

			/* make directory for feedname */
			namelen = makepathname(name, sizeof(name) - 1, fields[FieldFeedName]);
			if(!namelen)
				continue;

			if(basepathlen + namelen + 1 < sizeof(dirpath) - 1)
				sprintf(dirpath, "%s/%s", basepath, name);
			/* TODO: handle error. */
			if(mkdir(dirpath, S_IRWXU) != -1) {
				fprintf(stderr, "sfeed_frames: can't write '%s'\n", dirpath);
				exit(EXIT_FAILURE);
			}
			/* TODO: test, replaces strncpy (strncpy is slow) */
			reldirpath[0] = '\0';
			if(namelen < sizeof(reldirpath) - 2) {
				memcpy(reldirpath, name, namelen + 1); /* copy including nul byte */
	/*			reldirpath[namelen] = '\0';*/
			}
	/*		strncpy(reldirpath, name, sizeof(reldirpath) - 1);*/





			if(!(f = calloc(1, sizeof(struct feed))))
				die("can't allocate enough memory");



			if(totalfeeds) { /* end previous one. */
				fputs("</table>\n", fpitems);


				feedcurrent->next = f;
				feedcurrent = feedcurrent->next;



			} else {


				feedcurrent = f;


				feeds = feedcurrent; /* first item. */
				if(fields[FieldFeedName][0] == '\0') {
					showsidebar = 0;
				}
			}
			/* write menu link if new. */
			if(!(feedcurrent->name = strdup(fields[FieldFeedName])))
				die("can't allocate enough memory");
			if(fields[FieldFeedName][0] != '\0') {
				fputs("<h2 id=\"", fpitems);
				printfeednameid(feedcurrent->name, fpitems);
				fputs("\"><a href=\"#", fpitems);
				printfeednameid(feedcurrent->name, fpitems);
				fputs("\">", fpitems);
				fputs(feedcurrent->name, fpitems);
				fputs("</a></h2>\n", fpitems);
			}
			fputs("<table cellpadding=\"0\" cellspacing=\"0\">\n", fpitems);
			totalfeeds++;
		}



		/* write content */
		namelen = makepathname(name, sizeof(name), fields[FieldTitle]);
		if(!namelen)
			continue;
		if(strlen(dirpath) + namelen + strlen("/.html") < sizeof(filepath) - 1)
			sprintf(filepath, "%s/%s.html", dirpath, name);
		if(strlen(reldirpath) + namelen + strlen("/.html") < sizeof(relfilepath) - 1)
			sprintf(relfilepath, "%s/%s.html", reldirpath, name);
		if(!fileexists(filepath) && (fpcontent = fopen(filepath, "w+b"))) {
			fputs("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../style.css\" /></head>"
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
		}




		/* write item. */
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		/* set modified and access time of file to time of item. */
		contenttime.actime = parsedtime;
		contenttime.modtime = parsedtime;
		utime(filepath, &contenttime);

		isnew = (parsedtime >= comparetime);
		totalnew += isnew;
		feedcurrent->totalnew += isnew;
		feedcurrent->total++;
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
		      "<link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />"
		      "</head><body class=\"frame\"><div id=\"sidebar\">", fpmenu);
		for(feedcurrent = feeds; feedcurrent; feedcurrent = feedcurrent->next) {
			if(!feedcurrent->name || feedcurrent->name[0] == '\0')
				continue;
			if(feedcurrent->totalnew)
				fputs("<a class=\"n\" href=\"items.html#", fpmenu);
			else
				fputs("<a href=\"items.html#", fpmenu);
			printfeednameid(feedcurrent->name, fpmenu);
			fputs("\" target=\"items\">", fpmenu);
			if(feedcurrent->totalnew > 0)
				fputs("<b><u>", fpmenu);
			fputs(feedcurrent->name, fpmenu);
			fprintf(fpmenu, " (%lu)", feedcurrent->totalnew);
			if(feedcurrent->totalnew > 0)
				fputs("</u></b>", fpmenu);
			fputs("</a><br/>\n", fpmenu);
		}
		fputs("</div></body></html>", fpmenu);
	}
	fputs("<!DOCTYPE html><html><head>\n\t<title>Newsfeed (", fpindex);
	fprintf(fpindex, "%lu", totalnew);
	fputs(")</title>\n\t<link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />\n"
	      "</head>\n", fpindex);
	if(showsidebar) {
		fputs(
			"<frameset framespacing=\"0\" cols=\"200,*\" frameborder=\"1\">\n"
			"	<frame name=\"menu\" src=\"menu.html\" target=\"menu\">\n", fpindex);
	} else {
		fputs("<frameset framespacing=\"0\" cols=\"*\" frameborder=\"1\">\n", fpindex);
	}
	fputs(
		"	<frameset id=\"frameset\" framespacing=\"0\" cols=\"50%,50%\" frameborder=\"1\">\n"
		"		<frame name=\"items\" src=\"items.html\" target=\"items\">\n"
		"		<frame name=\"content\" target=\"content\">\n"
		"	</frameset>\n"
		"</frameset>\n"
		"</html>", fpindex);

	return EXIT_SUCCESS;
}
