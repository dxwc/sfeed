/* convert an opml file to sfeedrc file */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <expat.h> /* libexpat */

XML_Parser parser; /* expat XML parser state */

char * /* search for attr value by attr name in attributes list */
getattrvalue(const char **atts, const char *name) {
	const char **attr = NULL, *key, *value;
	if(!atts || !(*atts))
		return NULL;
	for(attr = atts; *attr; ) {
		key = *(attr++);
		value = *(attr++);
		if(key && value && !strcasecmp(key, name))
			return (char *)value;
	}
	return NULL;
}

void XMLCALL
xml_handler_start_element(void *data, const char *name, const char **atts) {
	char *feedurl = NULL, *feedname = NULL, *basesiteurl = NULL;

	if(!strcasecmp(name, "outline")) {
		if(!(feedname = getattrvalue(atts, "text")) &&
		   !(feedname = getattrvalue(atts, "title")))
			feedname = "unnamed";
		if(!(basesiteurl = getattrvalue(atts, "htmlurl")))
			basesiteurl = "";
		if(!(feedurl = getattrvalue(atts, "xmlurl")))
			feedurl = "";
		printf("\tfeed \"%s\" \"%s\" \"%s\"\n", feedname, feedurl, basesiteurl);
	}
}

void XMLCALL
xml_handler_end_element(void *data, const char *name) {
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
			fprintf(stderr, "sfeed_opml_config: error parsing xml %s at line %lu column %lu\n",
			        XML_ErrorString(XML_GetErrorCode(parser)), (unsigned long)XML_GetCurrentLineNumber(parser),
			        (unsigned long)XML_GetCurrentColumnNumber(parser));
			return 0;
		}
	} while(!done);
	return 1;
}

int main(void) {
	int status;

	if(!(parser = XML_ParserCreate("UTF-8"))) {
		fputs("sfeed_opml_config: can't create parser", stderr);
		exit(EXIT_FAILURE);
	}
	XML_SetElementHandler(parser, xml_handler_start_element, xml_handler_end_element);

	fputs(
		"# paths\n"
		"# NOTE: make sure to uncomment all these if you change it.\n"
		"#sfeedpath=\"$HOME/.sfeed\"\n"
		"#sfeedfile=\"$sfeedpath/feeds\"\n"
		"#sfeedfilenew=\"$sfeedfile.new\"\n"
		"\n"
		"# list of feeds to fetch:\n"
		"feeds() {\n"
		"	# feed <name> <url> [encoding]\n", stdout);
	status = xml_parse_stream(parser, stdin);
	fputs("}\n", stdout);

	XML_ParserFree(parser);

	return status ? EXIT_SUCCESS : EXIT_FAILURE;
}
