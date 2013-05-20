#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include "common.h"
#include "compat.h"

static int showsidebar = 1; /* show sidebar ? */

void /* print error message to stderr */
die(const char *s) {
	fputs("sfeed_frames: ", stderr);
	fputs(s, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

/* print text, ignore tabs, newline and carriage return etc
 * print some HTML 2.0 / XML 1.0 as normal text */
void
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

size_t
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

int
fileexists(const char *path) {
	return (!access(path, F_OK));
}

int
main(int argc, char **argv) {
	char *line = NULL, *fields[FieldLast];
	unsigned long totalfeeds = 0, totalnew = 0;
	unsigned int isnew;
	struct feed *feedcurrent = NULL, *feeds = NULL; /* start of feeds linked-list. */
	time_t parsedtime, comparetime;
	size_t size = 0;
	char name[256];
	char dirpath[1024];
	char filepath[1024];
	char reldirpath[1024];
	char relfilepath[1024];
	FILE *fpindex, *fpitems, *fpmenu, *fpcontent;
	char *basepath = "feeds";
	struct utimbuf contenttime;
	size_t namelen = 0;

	memset(&contenttime, 0, sizeof(contenttime));

	if(argc > 1 && argv[1][0] != '\0')
		basepath = argv[1];

	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	xmkdir(basepath, S_IRWXU);

	/* write main index page */
	if(strlen(basepath) + strlen("/index.html") < sizeof(dirpath) - 1)
		sprintf(dirpath, "%s/index.html", basepath);
	if((fpindex = fopen(dirpath, "w+b"))) {
	}
	if(strlen(basepath) + strlen("/menu.html") < sizeof(dirpath) - 1)
		sprintf(dirpath, "%s/menu.html", basepath);
	if(!(fpmenu = fopen(dirpath, "w+b"))) {
		/* TODO: error */
		fclose(fpindex);
		return EXIT_FAILURE;
	}
	if(strlen(basepath) + strlen("/items.html") < sizeof(dirpath) - 1)
		sprintf(dirpath, "%s/items.html", basepath);
	if(!(fpitems = fopen(dirpath, "w+b"))) {
		/* TODO: error */
		fclose(fpmenu);
		fclose(fpindex);
		return EXIT_FAILURE;
	}
	fclose(fpcontent);
	fputs("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" /></head>", fpitems);
	fputs("<body class=\"frame\"><div id=\"items\">", fpitems);

	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		dirpath[0] = '\0';
		filepath[0] = '\0';
		reldirpath[0] = '\0';
		relfilepath[0] = '\0';
		namelen = makepathname(name, sizeof(name) - 1, fields[FieldFeedName]);
		if(namelen) {
			if(strlen(basepath) + namelen + 1 < sizeof(dirpath) - 1)
				sprintf(dirpath, "%s/%s", basepath, name);
			/* TODO: handle error. */
			if(xmkdir(dirpath, S_IRWXU) != -1) {
			}
			strncpy(reldirpath, name, sizeof(reldirpath) - 1);	
			namelen = makepathname(name, sizeof(name), fields[FieldTitle]);
			if(namelen) {
				if(strlen(dirpath) + namelen + strlen("/.html") < sizeof(filepath) - 1)
					sprintf(filepath, "%s/%s.html", dirpath, name);
				if(strlen(reldirpath) + namelen + strlen("/.html") < sizeof(relfilepath) - 1)
					sprintf(relfilepath, "%s/%s.html", reldirpath, name);
				if(!fileexists(filepath) && (fpcontent = fopen(filepath, "w+b"))) {
					fputs("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../style.css\" /></head>", fpcontent);
					fputs("<body class=\"frame\"><div class=\"content\">", fpcontent);
					fputs("<h2><a href=\"", fpcontent);
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

				/* first of feed section or new feed section. */
				if(!totalfeeds || strcmp(feedcurrent->name, fields[FieldFeedName])) {
					if(totalfeeds) { /* end previous one. */
						fputs("</table>\n", fpitems);
						if(!(feedcurrent->next = calloc(1, sizeof(struct feed))))
							die("can't allocate enough memory");
						feedcurrent = feedcurrent->next;
					} else {
						if(!(feedcurrent = calloc(1, sizeof(struct feed))))
							die("can't allocate enough memory");
						feeds = feedcurrent; /* first item. */
						if(fields[FieldFeedName][0] == '\0') {
							showsidebar = 0;
						}
					}
					/* write menu link if new. */
					if(!(feedcurrent->name = xstrdup(fields[FieldFeedName])))
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
					fputs("<tr class=\"n\"><td nowrap valign=\"top\">", fpitems);
				else
					fputs("<tr><td nowrap valign=\"top\">", fpitems);
				fputs("<tr><td nowrap valign=\"top\">", fpitems);
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
		}
	}
	if(totalfeeds) {
		fputs("</table>\n", fpitems);
	}
	fputs("\n</div></body>\n</html>", fpitems); /* div items */
	if(showsidebar) {
		fputs("<html><head>", fpmenu);
		fputs("<link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />", fpmenu);
		fputs("</head><body class=\"frame\"><div id=\"sidebar\">", fpmenu);
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

	fputs("<!DOCTYPE html><html><head>\n", fpindex);
	fprintf(fpindex, "\t<title>Newsfeed (%lu)</title>\n", totalnew);
	fputs("\t<link rel=\"stylesheet\" type=\"text/css\" href=\"../style.css\" />\n", fpindex);
	fputs("</head>\n", fpindex);
	if(showsidebar) {
		fputs(
			"<frameset framespacing=\"0\" cols=\"200,*\" frameborder=\"1\">"
			"	<frame name=\"menu\" src=\"menu.html\" target=\"menu\">", fpindex);
	} else {
		fputs(
			"<frameset framespacing=\"0\" cols=\"*\" frameborder=\"1\">", fpindex);
	}
	fputs(
		"	<frameset id=\"frameset\" framespacing=\"0\" cols=\"50%,50%\" frameborder=\"1\">"
		"		<frame name=\"items\" src=\"items.html\" target=\"items\">"
		"		<frame name=\"content\" target=\"content\">"
		"	</frameset>"
		"</frameset>"
		"</html>", fpindex);

	fclose(fpmenu);
	fclose(fpitems);
	fclose(fpindex);

	free(line); /* free line */
	feedsfree(feeds); /* free feeds linked-list */

	return EXIT_SUCCESS;
}
