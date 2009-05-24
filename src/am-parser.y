/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * am-parser.y
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

#include <am-scanner.h>

#include <stdlib.h>

#define YYDEBUG 1

%}

/* Defining an union allow to use 2 protocol blocks (enclosed by %{ %}) which
 * is useful when redefining YYSTYPE. */
%union {
	AnjutaTokenOld *token;
	AnjutaTokenOldRange range;
}

%token	<token> EOL	'\n'
%token	<token> SPACE
%token	<token> TAB '\t'
%token	<token> MACRO
%token	<token> VARIABLE
%token	<token> COLON ':'
%token	<token> DOUBLE_COLON "::"
%token	<token> ORDER '|'
%token	<token> SEMI_COLON ';'
%token	<token> EQUAL '='
%token	<token> IMMEDIATE_EQUAL ":="
%token	<token> CONDITIONAL_EQUAL "?="
%token	<token> APPEND "+="
%token	<token> CHARACTER
%token	<token> NAME
%token	<token> AM_VARIABLE

%type <token> head_token target_token value_token name_token space_token rule_token equal_token token automake_token prerequisite_token
%type <range> am_variable
%type <range> value head space prerequisite target depend rule variable commands head_with_space
%type <range> value_list prerequisite_list target_list token_list target_list2

%defines

%pure_parser

/* Necessary because autotools wrapper always looks for a file named "y.tab.c",
 * not "amp-scanner.c"
%output="y.tab.c"*/

%glr-parser

%parse-param {void* scanner}
%lex-param   {void* scanner}

%name-prefix="amp_am_yy"

%locations

%start file

%debug

%{

//amp_am_yydebug = 1;

static void amp_am_yyerror (YYLTYPE *loc, void *scanner, char const *s);

%}


%%

file:
	optional_space statement
	| file EOL optional_space statement
	;
        
statement:
	/* empty */
	| line
	| am_variable
	;

line:
	name_token
	| line token
	;
		
variable:
	head_with_space equal_token optional_space value_list optional_space
	| head equal_token optional_space value_list optional_space
	;

rule:
	depend
	| depend SEMI_COLON commands
	| depend EOL TAB commands
	;
		
depend:
	target_list rule_token optional_space prerequisite_list optional_space ORDER optional_space prerequisite_list 
	;

commands:
	token_list
	| commands EOL TAB token_list
	;
		
am_variable:
	AM_VARIABLE space_list_value {
		anjuta_token_old_set_flags ($1, ANJUTA_TOKEN_OLD_SIGNIFICANT);
	}
	| AM_VARIABLE optional_space equal_token optional_space
	;
				
space_list_value: optional_space  equal_token   optional_space  value_list  optional_space {
        anjuta_token_old_set_flags ($2, ANJUTA_TOKEN_OLD_IRRELEVANT);
	anjuta_token_old_insert_after ($2, anjuta_token_old_new_static (ANJUTA_TOKEN_OLD_OPEN, NULL));
	anjuta_token_old_insert_after ($4.last, anjuta_token_old_new_static (ANJUTA_TOKEN_OLD_CLOSE, NULL));
	}
	;
		
value_list:
	value
	| value_list  space  value {
		$$.first = $1.first;
		$$.last = $3.last;
		anjuta_token_old_set_flags ($2.first, ANJUTA_TOKEN_OLD_NEXT);
	}
	;

target_list:
	head
	| head_with_space
	| head_with_space target_list2 optional_space
	;

target_list2:
	target
	| target_list2 space target {
		$$.first = $1.first;
		$$.last = $3.last;
		anjuta_token_old_set_flags ($2.first, ANJUTA_TOKEN_OLD_NEXT);
	}
	;
		
token_list:
	token {
		$$.first = $1;
		$$.last = $1;
	}
	| token_list token
	;
		
prerequisite_list:
	prerequisite
	| prerequisite_list space prerequisite {
		$$.first = $1.first;
		$$.last = $3.last;
		anjuta_token_old_set_flags ($2.first, ANJUTA_TOKEN_OLD_NEXT);
	}
	;

		

optional_space:
	/* empty */
	| space
	;


head_with_space:
	head space
	;
		
head:
	head_token {
		$$.first = $1;
		$$.last = $1;
	}
	| head name_token {
		$$.first = $1.first;
		$$.last = $2;
	}
	;

target:
	head_token {
		$$.first = $1;
		$$.last = $1;
	}
	| target target_token {
		$$.first = $1.first;
		$$.last = $2;
	}
	;
		
value:
	value_token {
		$$.first = $1;
		$$.last = $1;
	}
	| value value_token {
		$$.first = $1.first;
		$$.last = $2;
	}
	;

prerequisite:
	prerequisite_token {
		$$.first = $1;
		$$.last = $1;
	}
	| prerequisite prerequisite_token {
		$$.first = $1.first;
		$$.last = $2;
	}
	;
		
space:
	space_token {
		$$.first = $1;
		$$.last = $1;
		anjuta_token_old_set_flags ($1, ANJUTA_TOKEN_OLD_IRRELEVANT);
	}
	| space space_token	{
		$$.first = $1.first;
		$$.last = $2;
		anjuta_token_old_set_flags ($2, ANJUTA_TOKEN_OLD_IRRELEVANT);
	}
	;

		
token:
	space_token
	| value_token
	;            
	
value_token:
	equal_token
	| rule_token
	| target_token
	;

prerequisite_token:
	equal_token
	| rule_token
	| name_token
	| automake_token
	| ORDER
	| SEMI_COLON
	;

target_token:
	head_token
	| automake_token
	;
		
		
space_token:
	SPACE
	| TAB
	;

equal_token:
	EQUAL
	| IMMEDIATE_EQUAL
	| CONDITIONAL_EQUAL
	| APPEND
	;

rule_token:
	COLON
	| DOUBLE_COLON
	;
		
head_token:
	MACRO
	| VARIABLE
	| NAME
	| CHARACTER
	| ORDER
	| SEMI_COLON
	;

name_token:
	MACRO
	| VARIABLE
	| NAME
	| CHARACTER
	;
		
automake_token:
	AM_VARIABLE
	;
		
%%
     
static void
amp_am_yyerror (YYLTYPE *loc, void *scanner, char const *s)
{
        g_message ("(%d:%d-%d:%d) %s\n", loc->first_line, loc->first_column, loc->last_line, loc->last_column, s);
}

/* Public functions
 *---------------------------------------------------------------------------*/

