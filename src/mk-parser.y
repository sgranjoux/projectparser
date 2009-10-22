/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * mk-parser.y
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

#include "mk-scanner.h"
#include "mk-parser.h"

#include <stdlib.h>

#define YYDEBUG 1

%}

%token	EOL	'\n'
%token	SPACE
%token	TAB '\t'
%token  HASH '#'
%token	MACRO
%token	VARIABLE
%token  COMMA ','
%token	COLON ':'
%token	DOUBLE_COLON "::"
%token	ORDER '|'
%token	SEMI_COLON ';'
%token	EQUAL '='
%token	IMMEDIATE_EQUAL ":="
%token	CONDITIONAL_EQUAL "?="
%token	APPEND "+="
%token	CHARACTER
%token	NAME
%token	MK_VARIABLE
%token  _PHONY
%token  _SUFFIXES
%token  _DEFAULT
%token  _PRECIOUS
%token  _INTERMEDIATE
%token  _SECONDARY
%token  _SECONDEXPANSION
%token  _DELETE_ON_ERROR
%token  _IGNORE
%token  _LOW_RESOLUTION_TIME
%token  _SILENT
%token  _EXPORT_ALL_VARIABLES
%token  _NOTPARALLEL
%token FIRST_PASS
%token SECOND_PASS

%defines

%define api.pure
%define api.push_pull "push"


/* Necessary because autotools wrapper always looks for a file named "y.tab.c",
 * not "amp-scanner.c"
%output="y.tab.c"*/

/*%glr-parser*/

%parse-param {void* scanner}
%parse-param {AnjutaToken** last}
%lex-param   {void* scanner}

%name-prefix="mkp_yy"

%locations

%expect 1

%start file

%debug

%{

//mkp_yydebug = 1;

static void mkp_yyerror (YYLTYPE *loc, MkpScanner *scanner, AnjutaToken **last, char const *s);
static void mkp_special_target (AnjutaToken *list);
static gint mkp_special_prerequisite (AnjutaToken *token);

%}


%%

file:
    FIRST_PASS first_pass
    | SECOND_PASS second_pass
    ;

first_pass:
    all_token
    | EOL
    | first_pass all_token
    | first_pass EOL {
        *last = $2;
    }
    ;    

second_pass:
    statement
    | second_pass statement
    ;

statement:
    end_of_line
    | space end_of_line
    | definition end_of_line
    | rule  command_list {
        anjuta_token_group_append_children ($1, $2);
        mkp_scanner_add_rule (scanner, $$);
    }
	;

definition:
    head_list equal_token value {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_DEFINITION, NULL);
        anjuta_token_group_append_children ($$, $1);
        anjuta_token_group_append ($$, $2);
        anjuta_token_group_append ($$, $3);
        mkp_scanner_update_variable (scanner, $$);
    }        
    ;    

rule:
    depend_list  end_of_line
    | depend_list  SEMI_COLON  command_line  EOL {
        anjuta_token_group_append ($1, $3);
    }
    ;

depend_list:
    head_list  rule_token  prerequisite_list {
        $$ = anjuta_token_group_new (MK_TOKEN_RULE, NULL);
        anjuta_token_group_append ($$, $1);
        anjuta_token_set_type ($1, MK_TOKEN_TARGET);
        mkp_special_target ($1);
        switch (anjuta_token_get_type ($2))
        {
        case COLON:
            anjuta_token_set_type ($2, MK_TOKEN_COLON);
            break;
        case DOUBLE_COLON:
            anjuta_token_set_type ($2, MK_TOKEN_DOUBLE_COLON);
            break;
        default:
            break;
        }
        anjuta_token_group_append_token ($$, $2);
        anjuta_token_group_append ($$, $3);
    }
    ;

command_list:
    /* empty */ {
        $$ = NULL;
    }
	| command_list TAB command_line EOL {
        if ($$ == NULL) $$ = anjuta_token_group_new (MK_TOKEN_COMMANDS, NULL);
        anjuta_token_group_append ($$, $3);
    }
	;

	
/* Lists
 *----------------------------------------------------------------------------*/

end_of_line:
    EOL
    | comment
    ;

comment:
    HASH not_eol_list EOL
    ;

not_eol_list:
    /* empty */
    | not_eol_list not_eol_token
    ;

prerequisite_list:
    /* empty */ {
        $$ = anjuta_token_group_new (MK_TOKEN_PREREQUISITE, NULL);
    }
    | space {
        $$ = anjuta_token_group_new (MK_TOKEN_PREREQUISITE, NULL);
    }
    | optional_space  prerequisite_list_body  optional_space {
        $$ = $2;
    }
    ;

prerequisite_list_body:
	prerequisite {
        $$ = anjuta_token_group_new (MK_TOKEN_PREREQUISITE, NULL);
        anjuta_token_group_append ($$, $1);
    }
	| prerequisite_list_body  space  prerequisite {
        anjuta_token_group_append ($1, $3);
	}
	;

head_list:
    optional_space head_list_body optional_space {
        $$ = $2;
    }
    ;

head_list_body:
    head {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_NAME, NULL);
        anjuta_token_group_append ($$, $1);
    }
    | head_list_body  space  head {
        anjuta_token_group_append ($1, $2);
        anjuta_token_group_append ($1, $3);
    }
    ;

command_line:
    /* empty */ {
        $$ = anjuta_token_group_new (MK_TOKEN_COMMAND, NULL);
    }
    | command_line command_token {
        anjuta_token_group_append_token ($1, $2);
    }
    ;

/* Items
 *----------------------------------------------------------------------------*/

optional_space:
	/* empty */ {
		$$ = NULL;
	}
	| space
	;

space:
	space_token {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_SPACE, $1);
    }
	| space space_token	{
        anjuta_token_group_append_token ($1, $2);
	}
	;

head:
    head_token {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_NAME, $1);    
    }
    | head head_token {
        anjuta_token_group_append_token ($1, $2);
    }
    ;

value:
    value_token {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_VALUE, $1);
    }
    | space_token {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_VALUE, $1);
    }
    | value value_token {
        anjuta_token_group_append_token ($1, $2);
    }
    | value space_token {
        anjuta_token_group_append_token ($1, $2);
    }
    ;     

prerequisite:
    name_prerequisite
    | var_prerequisite
    | var_prerequisite name_prerequisite {
        $$ = $2;
    }
    ;

name_prerequisite:
    prerequisite_token {
        $$ = anjuta_token_group_new (mkp_special_prerequisite ($1), $1);
    }
    | name_prerequisite prerequisite_token {
        anjuta_token_group_append_token ($1, $2);
    }
    | name_prerequisite variable_token {
        anjuta_token_group_append_token ($1, $2);
    }
    ;     

var_prerequisite:
    variable_token {
        $$ = anjuta_token_group_new (MK_TOKEN_PREREQUISITE, $1);
    }
    | var_prerequisite variable_token {
        anjuta_token_group_append_token ($1, $2);
    }
    ;

/* Tokens
 *----------------------------------------------------------------------------*/
		
not_eol_token:
    word_token
    | space_token
    ;

all_token:
    name_token
    | space_token
    | variable_token
    | equal_token
    | rule_token
    | depend_token
    | HASH
    ;    

prerequisite_token:
    name_token
    | equal_token
    | rule_token
	;

command_token:
    name_token
    | variable_token
    | equal_token
    | rule_token
    | depend_token
    | space_token
    ;

value_token:
    name_token
    | variable_token
    | equal_token
    | rule_token
    | depend_token
    ;

head_token:
    name_token
    | variable_token
    | depend_token
    ;

variable_token:
	VARIABLE {
        mkp_scanner_parse_variable (scanner, $$);
    }
    ;

name_token:
	NAME
	| CHARACTER
    | COMMA
    | ORDER
    | _PHONY
    | _SUFFIXES
    | _DEFAULT
    | _PRECIOUS
    | _INTERMEDIATE
    | _SECONDARY
    | _SECONDEXPANSION
    | _DELETE_ON_ERROR
    | _IGNORE
    | _LOW_RESOLUTION_TIME
    | _SILENT
    | _EXPORT_ALL_VARIABLES
    | _NOTPARALLEL
    ;

rule_token:
	COLON
	| DOUBLE_COLON
	;

depend_token:
    SEMI_COLON
    ;

word_token:
	VARIABLE
	| NAME
	| CHARACTER
	| ORDER
    | HASH
    | COMMA
    | COLON
    | DOUBLE_COLON
	| SEMI_COLON
	| EQUAL
	| IMMEDIATE_EQUAL
	| CONDITIONAL_EQUAL
	| APPEND
    | _PHONY
    | _SUFFIXES
    | _DEFAULT
    | _PRECIOUS
    | _INTERMEDIATE
    | _SECONDARY
    | _SECONDEXPANSION
    | _DELETE_ON_ERROR
    | _IGNORE
    | _LOW_RESOLUTION_TIME
    | _SILENT
    | _EXPORT_ALL_VARIABLES
    | _NOTPARALLEL
    ;
		
space_token:
	SPACE
	| TAB
	;

equal_token:
	EQUAL {
        $$ = anjuta_token_group_new (MK_TOKEN_EQUAL, $1);
    }
	| IMMEDIATE_EQUAL {
        $$ = anjuta_token_group_new (MK_TOKEN_IMMEDIATE_EQUAL, $1);
    }
	| CONDITIONAL_EQUAL {
        $$ = anjuta_token_group_new (MK_TOKEN_CONDITIONAL_EQUAL, $1);
    }
	| APPEND {
        $$ = anjuta_token_group_new (MK_TOKEN_APPEND, $1);
    }
	;
		
%%

static void
mkp_yyerror (YYLTYPE *loc, MkpScanner *scanner, AnjutaToken **last, char const *s)
{
    const gchar *filename;

	g_message ("scanner %p", scanner);
    filename = mkp_scanner_get_filename ((MkpScanner *)scanner);
    if (filename == NULL) filename = "?";
    g_message ("%s (%d:%d-%d:%d) %s\n", filename, loc->first_line, loc->first_column, loc->last_line, loc->last_column, s);
}

static gint
mkp_special_prerequisite (AnjutaToken *token)
{
    switch (anjuta_token_get_type (token))
    {
        case ORDER:
            return MK_TOKEN_ORDER;
        default:
            return ANJUTA_TOKEN_NAME;
    }
}

static void
mkp_special_target (AnjutaToken *list)
{
    AnjutaToken *arg;

    for (arg = anjuta_token_group_first (list); arg != NULL; arg = anjuta_token_group_next (arg))
    {
        AnjutaToken *target = anjuta_token_group_first (arg);

        if ((target != NULL) && (anjuta_token_group_next (target) == NULL))
        {
            AnjutaToken *token = anjuta_token_group_get_token (target);
            gint mk_token = 0;

            switch (anjuta_token_get_type (token))
            {
            case _PHONY:
                mk_token = MK_TOKEN__PHONY;
                break;
            case _SUFFIXES:
                mk_token = MK_TOKEN__SUFFIXES;
                break;
            case _DEFAULT:
                mk_token = MK_TOKEN__DEFAULT;
                break;
            case _PRECIOUS:
                mk_token = MK_TOKEN__PRECIOUS;
                break;
            case _INTERMEDIATE:
                mk_token = MK_TOKEN__INTERMEDIATE;
                break;
            case _SECONDARY:
                mk_token = MK_TOKEN__SECONDARY;
                break;
            case _SECONDEXPANSION:
                mk_token = MK_TOKEN__SECONDEXPANSION;
                break;
            case _DELETE_ON_ERROR:
                mk_token = MK_TOKEN__DELETE_ON_ERROR;
                break;
            case _IGNORE:
                mk_token = MK_TOKEN__IGNORE;
                break;
            case _LOW_RESOLUTION_TIME:
                mk_token = MK_TOKEN__LOW_RESOLUTION_TIME;
                break;
            case _SILENT:
                mk_token = MK_TOKEN__SILENT;
                break;
            case _EXPORT_ALL_VARIABLES:
                mk_token = MK_TOKEN__EXPORT_ALL_VARIABLES;
                break;
            case _NOTPARALLEL:
                mk_token = MK_TOKEN__NOTPARALLEL;
                break;
            case ORDER:
                mk_token = MK_TOKEN_ORDER;
                break;
            default:
                break;
            }
        
            if (mk_token)
            {
                anjuta_token_set_type (anjuta_token_group_get_token (arg), mk_token);
            }
        }
    }
}
     
/* Public functions
 *---------------------------------------------------------------------------*/

