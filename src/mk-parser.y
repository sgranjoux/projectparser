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
%token  DUMMY

%defines

%pure_parser

/* Necessary because autotools wrapper always looks for a file named "y.tab.c",
 * not "amp-scanner.c"
%output="y.tab.c"*/

/*%glr-parser*/

%parse-param {void* scanner}
%lex-param   {void* scanner}

%name-prefix="mkp_yy"

%locations

%expect 1

%start file

%debug

%{

//mkp_yydebug = 1;

static void mkp_yyerror (YYLTYPE *loc, MkpScanner *scanner, char const *s);

%}


%%

file:
    statement
    | file statement
    ;

statement:
    end_of_line
    | space end_of_line
    | definition end_of_line
    | rule command_list 
	;

definition:
    head_list equal_token value_list {
		$$ = anjuta_token_merge (
			anjuta_token_insert_before ($1,
					anjuta_token_new_static (ANJUTA_TOKEN_DEFINITION, NULL)),
			$3 != NULL ? $3 : $2);
        mkp_scanner_update_variable (scanner, $$);
    }        
    ;    

rule:
    depend_list end_of_line
    | depend_list SEMI_COLON command_line EOL
    ;

depend_list:
    head_list rule_token prerequisite_list order_prerequisite_list
    ;

command_list:
    /* empty */
	| command_list TAB command_line EOL
	;
		
order_prerequisite_list:
    /* empty */
    | ORDER prerequisite_list
    ;
		

/* Lists
 *----------------------------------------------------------------------------*/

end_of_line:
    EOL
    | comment
    ;

comment:
    HASH not_eol_list EOL {
        anjuta_token_set_type ($1, ANJUTA_TOKEN_COMMENT);
        anjuta_token_group ($1, $3);
    }
    ;

not_eol_list:
    /* empty */
    | not_eol_list not_eol_token
    ;

value_list:
    /* empty */ {
        $$ = NULL;
    }
    | space {
        $$ = $1;
    }
    | optional_space value_list_body optional_space {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_VALUE, $1 != NULL ? $1 : $2);
        if ($1) anjuta_token_set_type ($1, ANJUTA_TOKEN_START);
        if ($3) anjuta_token_set_type ($1, ANJUTA_TOKEN_START);
        anjuta_token_group ($$, $3 != NULL ? $3 : $2);
    }
    ;

value_list_body:
    value
    | value_list_body space value {
        anjuta_token_set_type ($2, ANJUTA_TOKEN_NEXT);
    }
    ;

prerequisite_list:
    /* empty */ {
        $$ = NULL;
    }
    | optional_space prerequisite_list_body optional_space {
        $$ = anjuta_token_group_new (MK_TOKEN_PREREQUISITE, $1 != NULL ? $1 : $2);
        if ($1) anjuta_token_set_type ($1, ANJUTA_TOKEN_START);
        if ($3) anjuta_token_set_type ($3, ANJUTA_TOKEN_START);
        anjuta_token_group ($$, $3 != NULL ? $3 : $2);
    }
    ;

prerequisite_list_body:
	prerequisite
	| prerequisite_list_body space prerequisite {
        anjuta_token_set_type ($2, ANJUTA_TOKEN_NEXT);
	}
	;

head_list:
    optional_space head_list_body optional_space {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_NAME, $1 != NULL ? $1 : $2);
        if ($1) anjuta_token_set_type ($1, ANJUTA_TOKEN_START);
        if ($3) anjuta_token_set_type ($3, ANJUTA_TOKEN_START);
        anjuta_token_group ($$, $3 != NULL ? $3 : $2);
    }
    ;

head_list_body:
    head
    | head_list_body space head {
        anjuta_token_set_type ($2, ANJUTA_TOKEN_NEXT);
    }
    ;

command_line:
    /* empty */
    | command_line command_token
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
	space_token
	| space space_token	{
		anjuta_token_merge ($1, $2);
	}
	;

head:
    head_token {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_NAME, $1);    
    }
    | head head_token {
        anjuta_token_group ($$, $2);
    }
    ;

value:
    value_token {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_VALUE, $1);
    }
    | value value_token {
        anjuta_token_group ($$, $2);
    }
    ;     

prerequisite:
    prerequisite_token {
        $$ = anjuta_token_group_new (ANJUTA_TOKEN_VALUE, $1);
    }
    | prerequisite prerequisite_token {
        anjuta_token_group ($$, $2);
    }
    ;     

/* Tokens
 *----------------------------------------------------------------------------*/
		
not_eol_token:
    word_token
    | space_token
    ;

prerequisite_token:
    name_token
    | equal_token
    | rule_token
	;

command_token:
    name_token
    | equal_token
    | rule_token
    | depend_token
    | space_token
    ;

value_token:
    name_token
    | equal_token
    | rule_token
    | depend_token
    ;

head_token:
    name_token
    | depend_token
    ;

name_token:
	VARIABLE
	| NAME
	| CHARACTER
    | COMMA
    ;

rule_token:
	COLON
	| DOUBLE_COLON
	;

depend_token:
    ORDER
    | SEMI_COLON
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
    ;
		
space_token:
	SPACE
	| TAB
	;

equal_token:
	EQUAL {
        anjuta_token_set_type ($$, MK_TOKEN_EQUAL);
    }
	| IMMEDIATE_EQUAL {
        anjuta_token_set_type ($$, MK_TOKEN_IMMEDIATE_EQUAL);
    }
	| CONDITIONAL_EQUAL {
        anjuta_token_set_type ($$, MK_TOKEN_CONDITIONAL_EQUAL);
    }
	| APPEND {
        anjuta_token_set_type ($$, MK_TOKEN_APPEND);
    }
	;
		
%%

static void
mkp_yyerror (YYLTYPE *loc, MkpScanner *scanner, char const *s)
{
    gchar *filename;

	g_message ("scanner %p", scanner);
    filename = mkp_scanner_get_filename ((MkpScanner *)scanner);
    if (filename == NULL) filename = "?";
    g_message ("%s (%d:%d-%d:%d) %s\n", filename, loc->first_line, loc->first_column, loc->last_line, loc->last_column, s);
}
     
/* Public functions
 *---------------------------------------------------------------------------*/

