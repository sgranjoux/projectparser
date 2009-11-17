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

#include <stdlib.h>
#include <string.h>


#define YY_INPUT(buf, result, max_size) result = amp_am_scanner_input (yyextra, buf, max_size)

#define YY_EXTRA_TYPE  AmpAmScanner*

#define YY_DECL static int am_yylex (YYSTYPE * yylval_param,YYLTYPE * yylloc_param ,yyscan_t yyscanner)

//#define YY_USER_INIT {yy_flex_debug = 1;}

static AnjutaToken* amp_am_scanner_append_lex_token (AmpAmScanner *scanner, gint token);
static gint amp_am_scanner_input (AmpAmScanner *scanner, gchar *buffer, gsize max_size);
static int amp_am_scanner_parse_end (AmpAmScanner *scanner);

#define RETURN(tok) *yylval = amp_am_scanner_append_lex_token (yyextra, tok); \
                    return tok

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
     
struct _AmpAmScanner
{
    yyscan_t scanner;

	AmpAmBuffer *buffer;

    AmpProject *project;
    AmpGroup *group;
	GHashTable *orphan_sources;
};

/* Private functions
 *---------------------------------------------------------------------------*/

static void
amp_am_scanner_append_token (AmpAmScanner *scanner, AnjutaToken *token)
{
    if (scanner->buffer->last == NULL)
    {
        scanner->buffer->last = anjuta_token_insert_child (scanner->buffer->first, token);
    }
    else
    {
        while (anjuta_token_parent (scanner->buffer->last) != scanner->buffer->first)
        {
            scanner->buffer->last = anjuta_token_parent (scanner->buffer->last);
        }
        scanner->buffer->last = anjuta_token_insert_after (scanner->buffer->last, token);
    }
}
static AnjutaToken*
amp_am_scanner_append_lex_token (AmpAmScanner *scanner, gint token)
{
    AnjutaToken *frag;
    gsize length = yyget_leng (scanner->scanner);
    AnjutaToken *end;

    frag = anjuta_token_new_fragment (token, NULL, 0);

    for (end = scanner->buffer->start; end != NULL;)
    {
        if (anjuta_token_get_type (end) < ANJUTA_TOKEN_PARSED)
        {
            gint toklen = anjuta_token_get_length (end);
            AnjutaToken *copy = anjuta_token_cut (end, scanner->buffer->begin, length);
    
            if (toklen >= (length + scanner->buffer->begin))
            {

                if (end == scanner->buffer->start)
                {
                    /* Get whole token */
                    anjuta_token_free (frag);
                    anjuta_token_set_type (copy, token);
                    frag = copy;
                }
                else
                {
                    /* Get several token */
                    anjuta_token_append_child (frag, copy);
                }

                if (toklen == (length + scanner->buffer->begin))
                {
                    scanner->buffer->start = anjuta_token_next (end);
                    scanner->buffer->begin = 0;
                }
                else
                {
                    scanner->buffer->start = end;
                    scanner->buffer->begin += length;
                }
                break;
            }
            else
            {
                anjuta_token_append_child (frag, copy);
                length -= toklen;
                end = anjuta_token_next (end);
                scanner->buffer->begin = 0;
            }
        }
        else
        {
            end = anjuta_token_next (end);
            scanner->buffer->begin = 0;
        }
    }
    
    amp_am_scanner_append_token (scanner, frag);

    return frag;
}

static gint
amp_am_scanner_input (AmpAmScanner *scanner, gchar *buffer, gsize max_size)
{
    gint result = YY_NULL;

    if (scanner->buffer->next != NULL)
    {
        gsize length = anjuta_token_get_length (scanner->buffer->next);

        if ((anjuta_token_get_type (scanner->buffer->next) >= ANJUTA_TOKEN_PARSED) || (scanner->buffer->pos >= length))
        {
            for (;;)
            {
                /* Last token */
                if (scanner->buffer->next == scanner->buffer->end) return 0;

                if (anjuta_token_get_type (scanner->buffer->next) >= ANJUTA_TOKEN_PARSED)
                {
                    scanner->buffer->next = anjuta_token_next (scanner->buffer->next);
                }
                else
                {
                    scanner->buffer->next = anjuta_token_next (scanner->buffer->next);
                }

                if ((scanner->buffer->next == NULL) || (anjuta_token_get_type (scanner->buffer->next) == ANJUTA_TOKEN_EOV))
                {
                    /* Last token */
                    return 0;
                }
                else if ((anjuta_token_get_length (scanner->buffer->next) != 0) && (anjuta_token_get_type (scanner->buffer->next) < ANJUTA_TOKEN_PARSED))
                {
                    /* Find some data */
                    scanner->buffer->pos = 0;
                    length = anjuta_token_get_length (scanner->buffer->next);
                    break;  
                }
            }
        }

        if (scanner->buffer->pos < length)
        {
            const gchar *start = anjuta_token_get_string (scanner->buffer->next);

            length -= scanner->buffer->pos;
            
            if (length > max_size) length = max_size;
            memcpy (buffer, start + scanner->buffer->pos, length);
            scanner->buffer->pos += length;
            result = length;
        }
    }

    return result;
}

static gint
amp_am_scanner_parse_end (AmpAmScanner *scanner)
{
    AmpAmBuffer *buffer;

    yypop_buffer_state(scanner->scanner);
    buffer = scanner->buffer;
    scanner->buffer = buffer->parent;
    g_free (buffer);

    if (scanner->buffer == NULL)
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
    amp_project_set_am_variable (scanner->project, scanner->group, variable, name, list, scanner->orphan_sources);
}

/* Public functions
 *---------------------------------------------------------------------------*/

AnjutaToken *
amp_am_scanner_parse_token (AmpAmScanner *scanner, AnjutaToken *token, GError **error)
{
    AmpAmBuffer *buffer;

    buffer = g_new (AmpAmBuffer, 1);
    buffer->token = token;
    buffer->pos = 0;
    buffer->begin = 0;
    buffer->parent = NULL;

    buffer->next = anjuta_token_next (token);
    buffer->start = buffer->next;
    buffer->end = anjuta_token_last (token);
    if (buffer->end == token) buffer->end = NULL;

    if (scanner->buffer != NULL)
    {
        /* Parse an included file or a expanded variable */
        buffer->parent = scanner->buffer;
        scanner->buffer = buffer;

        //anjuta_token_dump (token);

        buffer->first = buffer->parent->last;
        buffer->last = NULL;

        yypush_buffer_state(yy_create_buffer(NULL, YY_BUF_SIZE, scanner->scanner), scanner->scanner);

        return buffer->first;    
    }
    else
    {
        amp_am_yypstate *ps;
        gint status;
        YYSTYPE yylval_param;
        YYLTYPE yylloc_param;
        AnjutaToken* yytok;

        buffer->last = NULL;
        buffer->first = anjuta_token_new_static (ANJUTA_TOKEN_FILE, NULL);

        yytok = buffer->first;

        scanner->buffer = buffer;

        ps = amp_am_yypstate_new ();

        do
        {
            gint yychar = am_yylex (&yylval_param, &yylloc_param, scanner->scanner);

			yylloc_param = yylval_param;
            status = amp_am_yypush_parse (ps, yychar, &yylval_param, &yylloc_param, scanner);
 
        } while (status == YYPUSH_MORE);
        amp_am_yypstate_delete (ps);

        return buffer->first;
    }
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
	scanner->orphan_sources = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)free_source_list);

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
	g_hash_table_destroy (scanner->orphan_sources);

	g_free (scanner);
}
