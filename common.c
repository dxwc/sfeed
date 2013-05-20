#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "common.h"

char *
afgets(char **p, size_t *size, FILE *fp) {
	char buf[BUFSIZ], *alloc = NULL;
	size_t n, len = 0, allocsiz;
	int end = 0;

	while(fgets(buf, sizeof(buf), fp)) {
		n = strlen(buf);
		if(buf[n - 1] == '\n') { /* dont store newlines. */
			buf[n - 1] = '\0';
			n--;
			end = 1; /* newline found, end */
		}
		len += n;
		allocsiz = len + 1;
		if(allocsiz > *size) {
			if((alloc = realloc(*p, allocsiz))) {
				*p = alloc;
				*size = allocsiz;
			} else {
				free(*p);
				*p = NULL;
				fputs("error: could not realloc\n", stderr);
				exit(EXIT_FAILURE);
				return NULL;
			}
		}
		strncpy((*p + (len - n)), buf, n);
		if(end || feof(fp))
			break;
	}
	if(*p && len > 0) {
		(*p)[len] = '\0';
		return *p;
	}
	return NULL;
}

void /* print link; if link is relative use baseurl to make it absolute */
printlink(const char *link, const char *baseurl, FILE *fp) {
	const char *ebaseproto, *ebasedomain, *p;
	int isrelative;

	/* protocol part */
	for(p = link; *p && (isalpha((int)*p) || isdigit((int)*p) || *p == '+' || *p == '-' || *p == '.'); p++);
	isrelative = strncmp(p, "://", strlen("://"));
	if(isrelative) { /* relative link (baseurl is used). */
		if((ebaseproto = strstr(baseurl, "://"))) {
			ebaseproto += strlen("://");
			fwrite(baseurl, 1, ebaseproto - baseurl, fp);
		} else {
			ebaseproto = baseurl;
			if(*baseurl || (link[0] == '/' && link[1] == '/'))
				fputs("http://", fp);
		}
		if(link[0] == '/') { /* relative to baseurl domain (not path).  */
			if(link[1] == '/') /* absolute url but with protocol from baseurl. */
				link += 2;
			else if((ebasedomain = strchr(ebaseproto, '/'))) /* relative to baseurl and baseurl path. */
				fwrite(ebaseproto, 1, ebasedomain - ebaseproto, fp);
			else
				fputs(ebaseproto, stdout);
		} else if((ebasedomain = strrchr(ebaseproto, '/'))) /* relative to baseurl and baseurl path. */
			fwrite(ebaseproto, 1, ebasedomain - ebaseproto + 1, fp);
		else {
			fputs(ebaseproto, fp);
			if(*baseurl && *link)
				fputc('/', fp);
		}
	}
	fputs(link, fp);
}

unsigned int
parseline(char **line, size_t *size, char **fields, unsigned int maxfields, int separator, FILE *fp) {
	unsigned int i = 0;
	char *prev, *s;

	if(afgets(line, size, fp)) {
		for(prev = *line; (s = strchr(prev, separator)) && i <= maxfields; i++) {
			*s = '\0'; /* null terminate string. */
			fields[i] = prev;
			prev = s + 1;
		}
		fields[i] = prev;
		for(i++; i < maxfields; i++) /* make non-parsed fields empty. */
			fields[i] = "";
	}
	return i;
}

/* print feed name for id; spaces and tabs in string as "-" (spaces in anchors are not valid). */
void
printfeednameid(const char *s, FILE *fp) {
	for(; *s; s++)
		fputc(isspace((int)*s) ? '-' : tolower((int)*s), fp);
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
feedsfree(struct feed *f) {
	struct feed *next;
	while(f) {
		next = f->next;
		free(f->name);
		free(f);
		f = next;
	}
}
