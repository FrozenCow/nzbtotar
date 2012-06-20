#include "nzbparser.h"
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlerror.h>
#include <libxml/entities.h>
#include <vector>
#include <string>
#include <sstream>
#include <string.h>
#include "common.h"

using namespace std;

struct ParseContext {
Nzb *currentNzb;
NzbFile *currentFile;
NzbSegment *currentSegment;

stringstream tagcontent;
};

void SAX_StartElement(ParseContext* context, const char *name, const char **atts)
{
	//printf("SAX_StartElement %s\n",name);
	if (!strcmp("nzb",name)) {
		if (context->currentNzb) { DIE(); }
		context->currentNzb = new Nzb();
	} else if (!strcmp("file", name)) {
		if (context->currentFile) { DIE(); }
		context->currentFile = new NzbFile();

    	for (int i = 0; atts[i]; i += 2) {
    		const char* attrname = atts[i];
    		const char* attrvalue = atts[i + 1];
			if (!strcmp("subject", attrname)) {
				context->currentFile->subject = string(attrvalue);
			}
			if (!strcmp("date", attrname)) {
			}
			if (!strcmp("poster", attrname)) {
				context->currentFile->poster = string(attrvalue);
			}
		}
	}
	else if (!strcmp("segment", name)) {
		if (context->currentSegment) { DIE(); }
		context->currentSegment = new NzbSegment();

    	for (int i = 0; atts[i]; i += 2) {
    		const char* attrname = atts[i];
    		const char* attrvalue = atts[i + 1];
			if (!strcmp("bytes", attrname))
			{
				context->currentSegment->bytes = atol(attrvalue);
			}
			if (!strcmp("number", attrname))
			{
				context->currentSegment->number = atol(attrvalue);
			}
		}
	}
}

void SAX_EndElement(ParseContext* context, const char *name)
{
	//printf("SAX_EndElement %s\n",name);
	if (!strcmp("file", name)) {
		if (!context->currentFile) {
			DIE();
		}
		context->currentNzb->files.push_back(context->currentFile);
		context->currentFile = NULL;
	}
	else if (!strcmp("group", name)) {
		if (!context->currentFile) {
			DIE();
		}
		
		context->currentFile->groups.push_back(context->tagcontent.str());
	}
	else if (!strcmp("segment", name)) {
		if (!context->currentFile || !context->currentSegment) {
			DIE();
		}

		context->currentSegment->article = context->tagcontent.str();
		context->currentFile->segments.push_back(context->currentSegment);
		context->currentSegment = NULL;
	}
	context->tagcontent.str(std::string());
}

void Parse_Content(ParseContext *context,const char *buf, int len)
{
	context->tagcontent.write(buf, len);
}

void SAX_characters(ParseContext* context, const char * xmlstr, int len)
{
	//printf("SAX_characters\n");
	char* str = (char*)xmlstr;
	
	// trim starting blanks
	int off = 0;
	for (int i = 0; i < len; i++)
	{
		char ch = str[i];
		if (ch == ' ' || ch == 10 || ch == 13 || ch == 9)
		{
			off++;
		}
		else
		{
			break;
		}
	}
	
	int newlen = len - off;
	
	// trim ending blanks
	for (int i = len - 1; i >= off; i--)
	{
		char ch = str[i];
		if (ch == ' ' || ch == 10 || ch == 13 || ch == 9)
		{
			newlen--;
		}
		else
		{
			break;
		}
	}
	if (newlen > 0)
	{
		// interpret tag content
		Parse_Content(context, str + off, newlen);
	}
}

void* SAX_getEntity(ParseContext* context, const char * name)
{
	xmlEntityPtr e = xmlGetPredefinedEntity((xmlChar* )name);
	if (!e)
	{
		//warn("entity not found");
	}

	return e;
}

void SAX_error(ParseContext* context, const char *msg, ...)
{
	//printf("SAX_ERROR\n");
	/*if (pFile->m_bIgnoreNextError)
	{
		pFile->m_bIgnoreNextError = false;
		return;
	}*/
	
    va_list argp;
    va_start(argp, msg);
    char szErrMsg[1024];
    vsnprintf(szErrMsg, sizeof(szErrMsg), msg, argp);
    szErrMsg[1024-1] = '\0';
    va_end(argp);

	// remove trailing CRLF
	for (char* pend = szErrMsg + strlen(szErrMsg) - 1; pend >= szErrMsg && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';
		DIE();
    //error("Error parsing nzb-file: %s", szErrMsg);
}

Nzb ParseNZB(const char *filename)
{
	xmlSAXHandler SAX_handler = {0};
	SAX_handler.startElement = reinterpret_cast<startElementSAXFunc>(SAX_StartElement);
	SAX_handler.endElement = reinterpret_cast<endElementSAXFunc>(SAX_EndElement);
	SAX_handler.characters = reinterpret_cast<charactersSAXFunc>(SAX_characters);
	SAX_handler.error = reinterpret_cast<errorSAXFunc>(SAX_error);
	SAX_handler.getEntity = reinterpret_cast<getEntitySAXFunc>(SAX_getEntity);

	ParseContext context;
	context.currentNzb = NULL;
	context.currentFile = NULL;
	context.currentSegment = NULL;
	context.tagcontent.str();

	int ret = xmlSAXUserParseFile(&SAX_handler, &context, filename);
    if (ret != 0)
		DIE();
	return *context.currentNzb;
}