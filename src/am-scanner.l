/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * am-scanner.l
 * Copyright (C) Sébastien Granjoux 2009 <seb.sfo@free.fr>
 * 
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
%{

#include "am-scanner.h"
#include "am-parser.h"

#include "libanjuta/anjuta-debug.h"
#include "libanjuta/anjuta-token-stream.h"

#include <stdlib.h>
#include <string.h>


#define YY_INPUT(buffer, result, max_size) result = anjuta_token_stream_read (yyextra->stream, buffer, max_size)

#define YY_EXTRA_TYPE  AmpAmScanner*

#define YY_DECL static int am_yylex (YYSTYPE * yylval_param,YYLTYPE * yylloc_param ,yyscan_t yyscanner)

//#define YY_USER_INIT {yy_flex_debug = 1;}

static int amp_am_scanner_parse_end (AmpAmScanner *scanner);

#define RETURN(tok) *yylval = anjuta_token_stream_tokenize (yyextra->stream, tok, yyleng); \
                    return tok

struct _AmpAmScanner
{
    yyscan_t scanner;

	AnjutaTokenStream *stream;

    AmpProject *project;
    AmpGroup *group;
	GHashTable *orphan_properties;
};

%}

%option reentrant noyywrap yylineno

/* Remove some warnings */
%option nounput noinput noyy_pop_state noyy_top_state

%option prefix="amp_am_yy"

%option bison-bridge bison-locations

%option never-interactive

%option batch

%option debug

NAME          [^ \t\n\r:#=$"'`&@\\]*

%%

<INITIAL>\n 						{ RETURN (EOL); }

<INITIAL>([ ]|\\\n)([ \t]|\\\n)* 	{ RETURN (SPACE); }

<INITIAL>([ \t])*#.*\n 				{ RETURN (EOL); }

<INITIAL>\t 						{ RETURN (TAB); }

<INITIAL>@{NAME}@ 					{ RETURN (MACRO); }

<INITIAL>\$\([^ \t\n\r:#=$)]+\) 	{ RETURN (VARIABLE); }

<INITIAL>\$\{[^ \t\n\r:#=$}]+\} 	{ RETURN (VARIABLE); }

<INITIAL>\$[^ \t\n\r\(\{] 			{ RETURN (VARIABLE); }

<INITIAL>: 							{ RETURN (COLON); }

<INITIAL>:: 						{ RETURN (DOUBLE_COLON); }

<INITIAL>; 							{ RETURN (SEMI_COLON); }

<INITIAL>\| 						{ RETURN (ORDER); }

<INITIAL>\= 						{ RETURN (EQUAL); }

<INITIAL>:= 						{ RETURN (IMMEDIATE_EQUAL); }

<INITIAL>\?= 						{ RETURN (CONDITIONAL_EQUAL); }

<INITIAL>\+= 						{ RETURN (APPEND); }

<INITIAL>\\[ ] 						{ RETURN (CHARACTER); }

<INITIAL>\\: 						{ RETURN (CHARACTER); }

<INITIAL>\\= 						{ RETURN (CHARACTER); }

<INITIAL>\\# 						{ RETURN (CHARACTER); }

<INITIAL>SUBDIRS 					{ RETURN (SUBDIRS); }

<INITIAL>DIST_SUBDIRS 				{ RETURN (DIST_SUBDIRS); }

<INITIAL>{NAME}_DATA 				{ RETURN (_DATA); }

<INITIAL>{NAME}_HEADERS 			{ RETURN (_HEADERS); }

<INITIAL>{NAME}_LIBRARIES 			{ RETURN (_LIBRARIES); }

<INITIAL>{NAME}_LISP 				{ RETURN (_LISP); }

<INITIAL>{NAME}_LTLIBRARIES 		{ RETURN (_LTLIBRARIES); }

<INITIAL>{NAME}_MANS 				{ RETURN (_MANS); }

<INITIAL>{NAME}_PROGRAMS 			{ RETURN (_PROGRAMS); }

<INITIAL>{NAME}_PYTHON 				{ RETURN (_PYTHON); }

<INITIAL>{NAME}_JAVA 				{ RETURN (_JAVA); }

<INITIAL>{NAME}_SCRIPTS 			{ RETURN (_SCRIPTS); }

<INITIAL>{NAME}_SOURCES 			{ RETURN (_SOURCES); }

<INITIAL>{NAME}_TEXINFOS 			{ RETURN (_TEXINFOS); }

<INITIAL>{NAME}dir					{ RETURN (_DIR); }

<INITIAL>AM_LDFLAGS					{ RETURN (_LDFLAGS);}

<INITIAL>AM_CPPFLAGS				{ RETURN (_CPPFLAGS);}

<INITIAL>AM_CFLAGS					{ RETURN (_CFLAGS);}

<INITIAL>AM_CXXFLAGS				{ RETURN (_CXXFLAGS);}

<INITIAL>AM_JAVACFLAGS				{ RETURN (_JAVACFLAGS);}

<INITIAL>AM_FCFLAGS					{ RETURN (_FCFLAGS);}

<INITIAL>AM_OBJCFLAGS				{ RETURN (_OBJCFLAGS);}

<INITIAL>AM_LFLAGS					{ RETURN (_LFLAGS);}

<INITIAL>AM_YFLAGS					{ RETURN (_YFLAGS);}

<INITIAL>{NAME}_LDFLAGS				{ RETURN (TARGET_LDFLAGS);}

<INITIAL>{NAME}_CPPFLAGS			{ RETURN (TARGET_CPPFLAGS);}

<INITIAL>{NAME}_CFLAGS				{ RETURN (TARGET_CFLAGS);}

<INITIAL>{NAME}_CXXFLAGS			{ RETURN (TARGET_CXXFLAGS);}

<INITIAL>{NAME}_JAVACFLAGS			{ RETURN (TARGET_JAVACFLAGS);}

<INITIAL>{NAME}_FCFLAGS				{ RETURN (TARGET_FCFLAGS);}

<INITIAL>{NAME}_OBJCFLAGS			{ RETURN (TARGET_OBJCFLAGS);}

<INITIAL>{NAME}_LFLAGS				{ RETURN (TARGET_LFLAGS);}

<INITIAL>{NAME}_YFLAGS				{ RETURN (TARGET_YFLAGS);}

<INITIAL>{NAME}_DEPENDENCIES		{ RETURN (TARGET_DEPENDENCIES);}

<INITIAL>{NAME} 					{ RETURN (NAME); }

<INITIAL>. 							{ RETURN (CHARACTER); }

%%

typedef struct _AmpAmBuffer AmpAmBuffer;
       
struct _AmpAmBuffer
{
    AnjutaToken *token;

    /* Beginning of current token */
    AnjutaToken *start;
    gsize begin;

    AnjutaToken *end;   /* Last token */

    /* Next data read buffer */
    AnjutaToken *next;
    gsize pos;

    /* Place to put new token */
    AnjutaToken *first;
    AnjutaToken *last;

    AmpAmBuffer *parent;    
};
     
/* Private functions
 *---------------------------------------------------------------------------*/

static gint
amp_am_scanner_parse_end (AmpAmScanner *scanner)
{
    yypop_buffer_state(scanner->scanner);
    scanner->stream = anjuta_token_stream_pop (scanner->stream);

    if (scanner->stream == NULL)
    {
        yyterminate();
    }
    else
    {
        return 1;
    }
}

/* Parser functions
 *---------------------------------------------------------------------------*/

void
amp_am_yyerror (YYLTYPE *loc, AmpAmScanner *scanner, char const *s)
{
    AnjutaTokenFileLocation location;

    if (amp_project_get_token_location (scanner->project, &location, *loc))
    {
        g_message ("%s:%d.%d %s\n", location.filename, location.line, location.column, s);
        g_free (location.filename);
    }
    else
    {
        g_message ("%s \n", s);
    }
}

void
amp_am_scanner_set_am_variable (AmpAmScanner *scanner, AnjutaTokenType variable, AnjutaToken *name, AnjutaToken *list)
{
    amp_project_set_am_variable (scanner->project, scanner->group, variable, name, list, scanner->orphan_properties);
}

/* Public functions
 *---------------------------------------------------------------------------*/

AnjutaToken *
amp_am_scanner_parse_token (AmpAmScanner *scanner, AnjutaToken *token, GError **error)
{
    AnjutaToken *first;
    AnjutaTokenStream *stream;

    stream = anjuta_token_stream_push (scanner->stream, token);
    first = anjuta_token_stream_get_root (stream);

    if (scanner->stream != NULL)
    {
        /* Parse an included file or a expanded variable */

        scanner->stream = stream;
        yypush_buffer_state(yy_create_buffer(NULL, YY_BUF_SIZE, scanner->scanner), scanner->scanner);
    }
    else
    {
        amp_am_yypstate *ps;
        gint status;

        scanner->stream = stream;
        ps = amp_am_yypstate_new ();
        do
        {
            YYSTYPE yylval_param;
            YYLTYPE yylloc_param;
            gint yychar = am_yylex (&yylval_param, &yylloc_param, scanner->scanner);

			yylloc_param = yylval_param;
            status = amp_am_yypush_parse (ps, yychar, &yylval_param, &yylloc_param, scanner);
 
        } while (status == YYPUSH_MORE);
        amp_am_yypstate_delete (ps);

    }

    return first;
}

/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

static void
free_source_list (GList *source_list)
{
	g_list_foreach (source_list, (GFunc)amp_source_free, NULL);
	g_list_free (source_list);
}

AmpAmScanner *
amp_am_scanner_new (AmpProject *project, AmpGroup *group)
{
	AmpAmScanner *scanner;

	scanner = g_new0 (AmpAmScanner, 1);

    scanner->project = project;
    scanner->group = group;

	/* Create hash table for sources list */
	scanner->orphan_properties = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)amp_target_property_buffer_free);

    yylex_init(&scanner->scanner);
    yyset_extra (scanner, scanner->scanner);

	return scanner;
};

void
amp_am_scanner_free (AmpAmScanner *scanner)
{
	g_return_if_fail (scanner != NULL);

    yylex_destroy(scanner->scanner);

	/* Free unused sources files */
	g_hash_table_destroy (scanner->orphan_properties);

	g_free (scanner);
}
