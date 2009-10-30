/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ac-scanner.l
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

#include <stdlib.h>
#include <string.h>

#include "ac-scanner.h"
#include "ac-parser.h"

#include "libanjuta/anjuta-debug.h"

/* Eliminate warning */
#define YY_NO_UNPUT 1 

#define YY_INPUT(buf, result, max_size) result = amp_ac_scanner_input (yyextra, buf, max_size)

#define YY_EXTRA_TYPE  AmpAcScanner*

#define YY_DECL static int ac_yylex (YYSTYPE * yylval_param,YYLTYPE * yylloc_param ,yyscan_t yyscanner)

//#define YY_USER_INIT {yy_flex_debug = 1;}
 
static AnjutaToken* amp_ac_scanner_append_lex_token (AmpAcScanner *scanner, gint token);
static void amp_update_location (YYLTYPE *loc, const gchar *text, gint length);
static gint amp_ac_scanner_input (AmpAcScanner *scanner, gchar *buffer, gsize max_size);
static gint amp_scanner_parse_end (AmpAcScanner *scanner);

gboolean amp_ac_yyparse (AmpAcScanner *scanner);


#define RETURN(tok) *yylval = amp_ac_scanner_append_lex_token (yyextra, tok); \
                    return tok
             
%}

%option reentrant stack noyywrap yylineno

%option prefix="amp_ac_yy"

/* Necessary because autotools wrapper always looks for a file named "lex.yy.c",
 * not "lex.amp_ac_yy.c"
%option outfile="lex.yy.c"*/			

%option bison-bridge bison-locations

%option never-interactive

%option batch

%option debug

WS          [ \t\r\v]+
NL          \n
WSNL        [ \t\v\r\n]+
COMMENT     #
OPENQUOTE   \[
CLOSEQUOTE  \]
OPENPARG   \(
CLOSEPARG  \)
COMMA       ,
EQUAL       =
LOWER       <
GREATER     >
NAME        [A-Za-z_][A-Za-z0-9_]*
VARIABLE    $[A-Za-z_0-9]+
OTHER       [^ \t\r\v\n#\[\]\\(),=><$_A-Za-z_]+

%x SPACE_LIST

%%

{WS}                    { RETURN (SPACE); }

\\\n                    { RETURN (SPACE); }

{NL}                    { RETURN (EOL); }

{COMMENT}               { RETURN (HASH); }

{OPENQUOTE}             { RETURN (LEFT_BRACE); }

{CLOSEQUOTE}            { RETURN (RIGHT_BRACE); }

{OPENPARG}              { RETURN (LEFT_PAREN); }

{CLOSEPARG}             { RETURN (RIGHT_PAREN); }

{COMMA}                 { RETURN (COMMA); }

{EQUAL}                 { RETURN (EQUAL); }

{LOWER}                 { RETURN (LOWER); }

{GREATER}                 { RETURN (GREATER); }

dnl                     { RETURN (DNL); }

PKG_CHECK_MODULES\(     { RETURN (PKG_CHECK_MODULES); }

AC_OUTPUT\(             { RETURN (OBSOLETE_AC_OUTPUT); }
 
AC_OUTPUT               { RETURN (AC_OUTPUT); }

AC_INIT\(               { RETURN (AC_INIT); }
 
AC_CONFIG_FILES\(       { RETURN (AC_CONFIG_FILES); }

{NAME}                  { RETURN (NAME); }

{VARIABLE}              { RETURN (VARIABLE); }

{OTHER}|\$|\\           { RETURN (WORD); }


<SPACE_LIST>{

{WSNL}                    { RETURN (SPACE); }

=|<|>|<=|>=             { RETURN (OPERATOR); }

{NAME}                    { RETURN (WORD); }

.                           {RETURN (WORD);}
}

%%

typedef struct _AmpAcBuffer AmpAcBuffer;
       
struct _AmpAcBuffer
{
    AnjutaToken *token;

    /* Beginning of current token */
    AnjutaToken *start;
    gsize begin;

    /* Next data read buffer */
    AnjutaToken *next;
    gsize pos;

    /* Place to put new token */
    AnjutaToken *first;
    AnjutaToken *last;

    AmpAcBuffer *parent;    
};
     
struct _AmpAcScanner
{
    yyscan_t scanner;

    AmpAcBuffer *buffer;

    AmpProject *project;
};


/* Private functions
 *---------------------------------------------------------------------------*/

static void
amp_ac_scanner_append_token (AmpAcScanner *scanner, AnjutaToken *token)
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
amp_ac_scanner_append_lex_token (AmpAcScanner *scanner, gint token)
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
            end = anjuta_token_next_sibling (end);
            scanner->buffer->begin = 0;
        }
    }
    
    amp_ac_scanner_append_token (scanner, frag);

    return frag;
}

void
amp_ac_scanner_yyerror (YYLTYPE *loc, AmpAcScanner *scanner, char const *s)
{
    AnjutaTokenFileLocation location;
    gboolean found;

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

static gint
amp_ac_scanner_input (AmpAcScanner *scanner, gchar *buffer, gsize max_size)
{
    gint result = YY_NULL;

    if (scanner->buffer->next)
    {
        gsize length = anjuta_token_get_length (scanner->buffer->next);

        if ((anjuta_token_get_type (scanner->buffer->next) >= ANJUTA_TOKEN_PARSED) || (scanner->buffer->pos >= length))
        {
            for (;;)
            {
                if (anjuta_token_get_type (scanner->buffer->next) >= ANJUTA_TOKEN_PARSED)
                {
                    scanner->buffer->next = anjuta_token_next_sibling (scanner->buffer->next);
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

    //fprintf (stdout, "input \"%.*s\"\n", result, buffer);

    return result;
}

static gint
amp_ac_scanner_parse_end (AmpAcScanner *scanner)
{
    AmpAcBuffer *buffer;

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

/* Public functions
 *---------------------------------------------------------------------------*/

void
amp_ac_scanner_load_module (AmpAcScanner *scanner, AnjutaTokenGroup *module)
{
    amp_project_load_module (scanner->project, module);
}

void
amp_ac_scanner_load_config (AmpAcScanner *scanner, AnjutaTokenGroup *list)
{
    amp_project_load_config (scanner->project, list);
}

AnjutaToken *
amp_ac_scanner_parse_token (AmpAcScanner *scanner, AnjutaToken *token, gint start, GError **error)
{
    AnjutaToken *child;
    AnjutaToken *first = NULL;
    gboolean ok;
    AmpAcBuffer *buffer;

    buffer = g_new (AmpAcBuffer, 1);
    buffer->token = token;
    buffer->pos = 0;
    buffer->begin = 0;
    buffer->parent = NULL;

    buffer->next = token;
    buffer->start = buffer->next;

    if (scanner->buffer != NULL)
    {
        /* Parse an included file or a expanded variable */
        buffer->parent = scanner->buffer;
        scanner->buffer = buffer;

        anjuta_token_dump (token);

        buffer->first = buffer->parent->last;
        buffer->last = NULL;

        yypush_buffer_state(yy_create_buffer(NULL, YY_BUF_SIZE, scanner->scanner), scanner->scanner);

        return buffer->first;    
    }
    else
    {
        amp_ac_yypstate *ps1;
        //amp_ac_yypstate *ps2;
        gint status;
        YYSTYPE yylval_param;
        YYLTYPE yylloc_param;
        AnjutaToken* yytok;

        buffer->last = NULL;
        buffer->first = anjuta_token_new_static (ANJUTA_TOKEN_FILE, NULL);

        yytok = buffer->first;

        scanner->buffer = buffer;

        ps1 = amp_ac_yypstate_new ();
        //ps2 = amp_ac_yypstate_new ();
        yylval_param = NULL;
        yylloc_param = NULL;
        //amp_ac_yypush_parse (ps1, FIRST_PASS, &yylval_param, &yylloc_param, scanner, NULL);
        switch (start)
        {
        case AC_SPACE_LIST_STATE:
            amp_ac_yypush_parse (ps1, START_SPACE_LIST, &yylval_param, &yylloc_param, scanner, NULL);
            yy_push_state (SPACE_LIST, scanner->scanner);
            break;
        default:
            break;
        }
        
        //fprintf(stdout, "\n\n");

        do
        {
            gint yychar = ac_yylex (&yylval_param, &yylloc_param, scanner->scanner);
            AnjutaToken *last = NULL;
        
            yylloc_param = yylval_param;
            status = amp_ac_yypush_parse (ps1, yychar, &yylval_param, &yylloc_param, scanner, &last);

            #if 0 
            if ((status == 0) || (last != NULL))
            {
                //printf ("\n: ");
                for (;;)
                {
                    YYSTYPE yylval_param;
                    YYLTYPE yylloc_param;
                    gint yychar;
                    AnjutaToken *next;

                    yytok = anjuta_token_next_lex (yytok);
                    if (yytok != NULL)
                    {
                        yychar = anjuta_token_get_type (yytok);
                    }
                    else
                    {
                        yychar = 0;
                    }

                    yylval_param = yytok;
                    //printf ("2:%d (%.*s)  ", yychar, yytok == NULL ? 0 : anjuta_token_get_length (yytok), yytok == NULL ? "" : anjuta_token_evaluate (yytok));
                    yylloc_param = yylval_param;
                    status = amp_ac_yypush_parse (ps2, yychar, &yylval_param, &yylloc_param, scanner, NULL);
                    if (status != YYPUSH_MORE) break;
                    if (yytok == last) break;
                }
                //printf ("\n");
        
                last = NULL;
            }
            #endif
        } while (status == YYPUSH_MORE);
        amp_ac_yypstate_delete (ps1);
        //amp_ac_yypstate_delete (ps2);

        return buffer->first;
    }
}

/* Constructor & Destructor
 *---------------------------------------------------------------------------*/

AmpAcScanner *
amp_ac_scanner_new (AmpProject *project)
{
	AmpAcScanner *scanner;

	scanner = g_new0 (AmpAcScanner, 1);

    yylex_init(&scanner->scanner);
    yyset_extra (scanner, scanner->scanner);

    scanner->project = project;

	return scanner;
};

void
amp_ac_scanner_free (AmpAcScanner *scanner)
{
	g_return_if_fail (scanner != NULL);

    //if (scanner->buffer != NULL) yy_delete_buffer (scanner->buffer, scanner->scanner);
    yylex_destroy(scanner->scanner);

	g_free (scanner);
}
