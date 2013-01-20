#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <utime.h>

#include "common.c"

/* Feed info. */
struct feed {
	char *name; /* feed name */
	unsigned long new; /* amount of new items per feed */
	unsigned long total; /* total items */
	struct feed *next; /* linked list */
};

static int showsidebar = 1; /* show sidebar ? */

void /* print error message to stderr */
die(const char *s) {
	fputs("sfeed_html: ", stderr);
	fputs(s, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

struct feed *
feednew(void) {
	struct feed *f;
	if(!(f = calloc(1, sizeof(struct feed))))
		die("can't allocate enough memory");
	return f;
}

void
feedsfree(struct feed *f) {
	struct feed *next;
	while(f) {
		next = f->next;
		free(f->name);
		free(f);
		f = next;
	}
}

/* print text, ignore tabs, newline and carriage return etc
 * print some HTML 2.0 / XML 1.0 as normal text */
void
print_content_test(const char *s, FILE *fp) {
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

/* print feed name for id; spaces and tabs in string as "-" (spaces in anchors are not valid). */
void
printfeednameid(const char *s, FILE *fp) {
	for(; *s; s++)
		fputc(isspace((int)*s) ? '-' : *s, fp);
}

void
printhtmlencoded(const char *s, FILE *fp) {
	for(; *s; s++) {
		switch(*s) {
		case '<': fputs("&lt;", fp); break;
		case '>': fputs("&gt;", fp); break;
/*		case '&': fputs("&amp;", fp); break;*/
		default:
			fputc(*s, fp);
		}
	}
}

void
makepathname(char *buffer, size_t bufsiz, const char *path) {
	const char *p = path;
	size_t i = 0, r = 0;
	/*for(p = path; isspace((int)*p); p++);*/ /* remove leading whitespace */
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
}

int
fileexists(const char *path) {
	return (!access(path, F_OK));
}

int
main(int argc, char **argv) {
	char *line = NULL, *fields[FieldLast];
	unsigned long totalfeeds = 0, totalnew = 0;
	unsigned int islink, isnew;
	struct feed *feedcurrent = NULL, *feeds = NULL; /* start of feeds linked-list. */
	time_t parsedtime, comparetime;
	size_t size = 0;
	char name[256];
	char dirpath[256];
	char filepath[1024];
	char reldirpath[1024];
	char relfilepath[1024];
	FILE *fpindex, *fpitems, *fpmenu, *fpcontent;
	char feedname[1024];
	char *basepath = "feeds";
	struct utimbuf contenttime = { 0 };

	/* TODO: write menu to file */
	/* TODO: write items to file */
	/* TODO: write items per category to file? */

	if(argc > 1 && argv[1][0] != '\0')
		basepath = argv[1];

/*	tzset();*/
	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */

	mkdir(basepath, S_IRWXU);

	/* write main index page */
	snprintf(dirpath, sizeof(dirpath) - 1, "%s/index.html", basepath);
	if((fpindex = fopen(dirpath, "w+b"))) {
	}
	snprintf(dirpath, sizeof(dirpath) - 1, "%s/menu.html", basepath);
	if(!(fpmenu = fopen(dirpath, "w+b"))) {
		/* TODO: error */
		fclose(fpindex);
		return EXIT_FAILURE;
	}
	snprintf(dirpath, sizeof(dirpath) - 1, "%s/items.html", basepath);
	if(!(fpitems = fopen(dirpath, "w+b"))) {
		/* TODO: error */
		fclose(fpmenu);
		fclose(fpindex);
		return EXIT_FAILURE;
	}

	feedname[0] = '\0';
	while(parseline(&line, &size, fields, FieldLast, stdin, FieldSeparator) > 0) {
		dirpath[0] = '\0';
		filepath[0] = '\0';
		reldirpath[0] = '\0';
		relfilepath[0] = '\0';
		makepathname(name, sizeof(name) - 1, fields[FieldFeedName]);
		if(name[0] != '\0') {
			snprintf(dirpath, sizeof(dirpath) - 1, "%s/%s", basepath, name);
			/* TODO: handle error. */
			if(mkdir(dirpath, S_IRWXU) != -1) {
			}
/*			snprintf(reldirpath, sizeof(reldirpath) - 1, "%s", name);*/
			strncpy(reldirpath, name, sizeof(reldirpath) - 1);	
			makepathname(name, sizeof(name), fields[FieldTitle]);
			if(name[0] != '\0') {
				snprintf(filepath, sizeof(filepath) - 1, "%s/%s.html", dirpath, name);
				snprintf(relfilepath, sizeof(relfilepath) - 1, "%s/%s.html", reldirpath, name);

				/* TODO: if file exists, dont overwrite. */
				/* TODO: give file title and timestamp of article, touch() ?. */
				if(!fileexists(filepath) && (fpcontent = fopen(filepath, "w+b"))) {
					fputs("<html><body>", fpcontent);
					fputs("<h2>", fpcontent);
					printhtmlencoded(fields[FieldTitle], fpcontent);
					fputs("</h2>", fpcontent);
					print_content_test(fields[FieldContent], fpcontent);
					fputs("</body></html>", fpcontent);
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
							fputs("-nosidebar", fpitems); /* set other id on div if no sidebar for styling */
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
					fputs("<table>", fpitems);
					totalfeeds++;
				}
				/* write item. */
				parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
				/* set modified and access time of file to time of item. */
				contenttime.actime = parsedtime;
				contenttime.modtime = parsedtime;
				utime(filepath, &contenttime);

				isnew = (parsedtime >= comparetime);
				islink = (strlen(fields[FieldLink]) > 0);
				totalnew += isnew;
				feedcurrent->new += isnew;
				feedcurrent->total++;

				fputs("<tr><td nowrap valign=\"top\">", fpitems);
				fputs(fields[FieldTimeFormatted], fpitems);
				fputs("</td><td valign=\"top\">", fpitems);

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
		fputs("\t\t</div>\n", fpitems); /* div items */
	}
	if(showsidebar) {
/*		fputs("\t\t<div id=\"sidebar\">\n\t\t\t<ul>\n", fpmenu);*/
		for(feedcurrent = feeds; feedcurrent; feedcurrent = feedcurrent->next) {
			if(!feedcurrent->name || feedcurrent->name[0] == '\0')
				continue;
			fputs("<a href=\"items.html#", fpmenu);
			printfeednameid(feedcurrent->name, fpmenu);
			fputs("\" target=\"items\">", fpmenu);
			if(feedcurrent->new > 0)
				fputs("<b><u>", fpmenu);
			fputs(feedcurrent->name, fpmenu);
			fprintf(fpmenu, " (%lu)", feedcurrent->new);
			if(feedcurrent->new > 0)
				fputs("</u></b>", fpmenu);
			fputs("</a><br/>\n", fpmenu);
		}
/*		fputs("\t\t\t</ul>\n\t\t</div>\n", fpmenu);*/
	}

	fputs("<!DOCTYPE html>"
		"<html>"
		"    <head>"
		"		<title>Newsfeeds (", fpindex);
	fprintf(fpindex, "%lu", totalnew);
	fputs(")</title>"
		"	</head>"
		"	<frameset framespacing=\"0\" cols=\"200,*\" frameborder=\"1\">"
		"		<frame name=\"menu\" src=\"menu.html\" target=\"menu\">"
		"		<frameset id=\"frameset\" framespacing=\"0\" cols=\"50%,50%\" frameborder=\"1\">"
		"			<frame name=\"items\" src=\"items.html\" target=\"items\">"
		"			<frame name=\"content\" target=\"content\">"
		"		</frameset>"
		"  	 </frameset>"
		"</html>", fpindex);

	fclose(fpmenu);
	fclose(fpitems);
	fclose(fpindex);

	free(line); /* free line */
	feedsfree(feeds); /* free feeds linked-list */

	return EXIT_SUCCESS;
}
