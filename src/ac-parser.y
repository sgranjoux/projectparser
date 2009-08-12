/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ac-parser.y
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
#include "ac-scanner.h"
#include "ac-parser.h"

#define YYDEBUG 1

#include "libanjuta/anjuta-debug.h"

//static void amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s);

amp_ac_yydebug = 1;

%}

/*%union {
	AnjutaToken *token;
}*/

%token  EOL '\n'

%token  SPACE ' '

%token  HASH '#'
%token  LEFT_PAREN     '('
%token  RIGHT_PAREN    ')'
%token  LEFT_CURLY		'{'
%token  RIGHT_CURLY    '}'
%token  LEFT_BRACE     '['
%token  RIGHT_BRACE    ']'
%token  EQUAL             '='
%token  COMMA             ','
    
%token  NAME
%token  VARIABLE
%token  MACRO
%token  OPERATOR
%token  WORD

/* M4 macros */

%token  DNL


/* Autoconf macros */

%token	AC_MACRO_WITH_ARG
%token	AC_MACRO_WITHOUT_ARG

%token	PKG_CHECK_MODULES
%token	OBSOLETE_AC_OUTPUT
%token	AC_OUTPUT
%token	AC_CONFIG_FILES
%token	AC_SUBST

/*%type pkg_check_modules obsolete_ac_output ac_output ac_config_files
%type dnl
%type ac_macro_with_arg ac_macro_without_arg
%type spaces
%type separator
%type arg_string arg arg_list arg_list_body shell_string_body raw_string_body

%type expression comment macro
%type arg_string_body arg_body expression_body

%type any_space*/

%defines

%pure_parser

%parse-param {void* scanner}
%lex-param   {void* scanner}

%name-prefix="amp_ac_yy"

%locations

%start file

%debug


%{
static void amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s);

%}


%%

file:
    /* empty */
	| file statement
	;
   
statement:
    line
    | macro
    ;

line:
    any_space
    | comment
    | shell_string
    | args_token
    | EQUAL
    | NAME
    | VARIABLE
    | WORD
    ;

macro:
    dnl
	| ac_macro_with_arg
	| ac_macro_without_arg
	| pkg_check_modules 
	| obsolete_ac_output
	| ac_output
	| ac_config_files
	;

/* Macros
 *----------------------------------------------------------------------------*/

dnl:
    DNL not_eol_list EOL
    ;
    

pkg_check_modules:
    PKG_CHECK_MODULES arg_list
/*	PKG_CHECK_MODULES  name  COMMA  space_list   list_empty_optional  {
		anjuta_token_merge ($1, $5);
	}
	| PKG_CHECK_MODULES  name  COMMA  space_list  list_arg_optional {
		anjuta_token_set_type ($$, ANJUTA_TOKEN_KEYWORD);
    }*/
	;

ac_macro_without_arg:
	AC_MACRO_WITHOUT_ARG
	;

ac_macro_with_arg:
	AC_MACRO_WITH_ARG arg_list {
		anjuta_token_merge ($1, $2);
	}
	;

obsolete_ac_output:
    OBSOLETE_AC_OUTPUT  arg_list
/*	OBSOLETE_AC_OUTPUT  optional_space_list  list_optional_optional {
		anjuta_token_merge ($1, $3);
	}*/
	;

ac_output:
	AC_OUTPUT
	;
	
ac_config_files:
    AC_CONFIG_FILES  arg_list
/*	AC_CONFIG_FILES  space_list  list_optional_optional {
		anjuta_token_merge ($1, $3);
	}*/
	;

/* Lists
 *----------------------------------------------------------------------------*/

arg_list:
    arg_list_body RIGHT_PAREN
    | spaces arg_list_body RIGHT_PAREN
    ;

arg_list_body:
    arg
    | arg_list_body separator arg
    ;
    
comment:
    HASH not_eol_list EOL
    ;

not_eol_list:
    /* empty */
    | not_eol_list not_eol
    ;


shell_string:
    LEFT_BRACE shell_string_body RIGHT_BRACE
    ;

shell_string_body:
    /* empty */ {
        $$ = NULL;
    }
    | shell_string_body not_brace
    | shell_string_body shell_string
    ;

raw_string:
    LEFT_BRACE raw_string_body RIGHT_BRACE
    ;

raw_string_body:
    /* empty */ {
        $$ = NULL;
    }
    | raw_string_body not_brace
    | raw_string_body raw_string
    ;

arg_string:
    LEFT_BRACE arg_string_body RIGHT_BRACE
    ;

arg_string_body:
    /* empty */ {
        $$ = NULL;
    }
    | arg_string_body any_space
    | arg_string_body HASH
    | arg_string_body LEFT_PAREN
    | arg_string_body RIGHT_PAREN
    | arg_string_body COMMA
    | arg_string_body EQUAL
    | arg_string_body NAME
    | arg_string_body VARIABLE
    | arg_string_body WORD
    | arg_string_body macro
    | arg_string_body raw_string
    ;

arg:
    /* empty */ {
        $$ = NULL;
    }
    | arg_string arg_body
    | expression arg_body
    | comment arg_body
    | macro arg_body
    | EQUAL arg_body
    | NAME arg_body
    | VARIABLE arg_body
    | WORD arg_body
    ;

arg_body:
    /* empty */ {
        $$ = NULL;
    }
    | arg_body any_space
    | arg_body arg_string
    | arg_body expression
    | arg_body comment
    | arg_body macro
    | arg_body EQUAL
    | arg_body NAME
    | arg_body VARIABLE
    | arg_body WORD
    ;

separator:
    COMMA
    | COMMA spaces
    ;

expression:
    LEFT_PAREN expression_body RIGHT_PAREN
    ;

expression_body:
    /* empty */ {
        $$ = NULL;
    }
    | expression_body any_space
    | expression_body comment
    | expression_body COMMA
    | expression_body EQUAL
    | expression_body NAME
    | expression_body VARIABLE
    | expression_body WORD
    | expression_body macro
    | expression_body expression
    ;


/* Items
 *----------------------------------------------------------------------------*/
    

spaces:
	any_space
	| spaces any_space {
		anjuta_token_merge ($1, $2);
	}
	;


/* Tokens
 *----------------------------------------------------------------------------*/

not_eol:
    SPACE
    | any_word    
    ;

not_brace:
    any_space
    | args_token
    | HASH
    | EQUAL
    | NAME
    | VARIABLE
    | WORD
    | any_macro
    ;

any_space:
    SPACE
    | EOL
    ;

args_token:
    LEFT_PAREN
    | RIGHT_PAREN
    | COMMA
    ;

any_word:
    HASH
    | LEFT_BRACE
    | RIGHT_BRACE
    | LEFT_PAREN
    | RIGHT_PAREN
    | COMMA
    | EQUAL
    | NAME
    | VARIABLE
    | WORD
    | any_macro
    ;

any_macro:
    AC_CONFIG_FILES
	| AC_MACRO_WITH_ARG
	| AC_MACRO_WITHOUT_ARG
    | AC_OUTPUT
    | DNL
    | OBSOLETE_AC_OUTPUT
    | PKG_CHECK_MODULES
    ;

%%
    
static void
amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s)
{
	g_message ("scanner %p", scanner);
    g_message ("%s (%d:%d-%d:%d) %s\n", amp_ac_scanner_get_filename ((AmpAcScanner *)scanner), loc->first_line, loc->first_column, loc->last_line, loc->last_column, s);
}

/* Public functions
 *---------------------------------------------------------------------------*/

