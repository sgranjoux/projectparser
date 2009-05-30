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

#define YYDEBUG 1

#include "libanjuta/anjuta-debug.h"

//static void amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s);

//amp_ac_yydebug = 1;

%}

%union {
	AnjutaToken *token;
	AnjutaTokenRange range;
}

%token  EOL '\n'

%token  <token> SPACE ' '

%token  HASH               '#'
%token  LEFT_PAREN     '('
%token  <token> RIGHT_PAREN    ')'
%token  LEFT_CURLY		'{'
%token  RIGHT_CURLY    '}'
%token  LEFT_BRACE     '['
%token  RIGHT_BRACE    ']'
%token  EQUAL             '='
%token  <token> COMMA             ','
%token  LOWER              '<'
%token  GREATER           '>'
%token  LOWER_OR_EQUAL
%token  GREATER_OR_EQUAL	
    
%token  COMMENT
%token  <token> NAME
%token  KEYWORD
%token  TOPNAME
%token  IDENTIFIER
%token  NUMBER
%token  <token> MACRO
%token  <token> OPERATOR

%token	<token> PKG_CHECK_MODULES
%token	<token> OBSOLETE_AC_OUTPUT
%token	<token> AC_OUTPUT
%token	<token> AC_CONFIG_FILES
%token	<token> AC_SUBST
%token	<token> AC_INIT

%type <token> pkg_check_modules obsolete_ac_output ac_config_files 
%type <token> optional_space name space_list comma_separator
%type <token> list_empty_optional list_arg_optional list_optional_optional

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
	statement
	| file EOL statement
	;
   
statement:
	line_or_empty
	| pkg_check_modules
	| obsolete_ac_output
	| ac_output
	| ac_config_files
	;

line_or_empty:
	/* empty */
	| line
	;

line:
	token
	| line token
	;

pkg_check_modules:
	PKG_CHECK_MODULES  optional_space  name  comma_separator  space_list   list_empty_optional  {
		anjuta_token_set_flags ($1, ANJUTA_TOKEN_SIGNIFICANT);
		if ($2 == NULL) anjuta_token_insert_after ($1, anjuta_token_new_static (ANJUTA_TOKEN_SPACE, NULL));
		anjuta_token_merge ($1, $6);
	}
	| PKG_CHECK_MODULES  optional_space name  comma_separator  space_list  list_arg_optional
	;

obsolete_ac_output:
	OBSOLETE_AC_OUTPUT  optional_space  space_list  list_optional_optional {
		anjuta_token_set_flags ($1, ANJUTA_TOKEN_SIGNIFICANT); 
		if ($2 == NULL) anjuta_token_insert_after ($1, anjuta_token_new_static (ANJUTA_TOKEN_SPACE, NULL));
		anjuta_token_merge ($1, $4);
	}
	;

ac_output:
	AC_OUTPUT
	;
	
ac_config_files:
	AC_CONFIG_FILES  optional_space space_list  list_optional_optional {
		if ($2 == NULL) anjuta_token_insert_after ($1, anjuta_token_new_static (ANJUTA_TOKEN_SPACE, NULL));
		anjuta_token_merge ($1, $4);
	}
	;
	
list_empty_optional:
	optional_space  RIGHT_PAREN {
		$$ = $1 == NULL ? $2 : anjuta_token_merge ($1, $2);
	}
	| optional_space  COMMA  optional_space  RIGHT_PAREN {
		$$ = anjuta_token_merge ($1 != NULL ? $1 :  $2, $4);
	}
	| optional_space  COMMA  optional_space  COMMA  arg_string_or_empty  RIGHT_PAREN {
		$$ = anjuta_token_merge ($1 != NULL ? $1 :  $2, $6);
	}
	;

list_arg_optional:
	optional_space  COMMA  arg_string  RIGHT_PAREN {
		$$ = anjuta_token_merge ($1 != NULL ? $1 : $2, $4);
	}
	| optional_space  COMMA  arg_string  COMMA  arg_string_or_empty  RIGHT_PAREN {
		$$ = anjuta_token_merge ($1 != NULL ? $1 :  $2, $6);
	}
	;

list_optional_optional:
	optional_space RIGHT_PAREN {
		$$ = $1 == NULL ? $2 : anjuta_token_merge ($1, $2);
	}
	| optional_space COMMA arg_string_or_empty RIGHT_PAREN {
		$$ =anjuta_token_merge ($1 != NULL ? $1 : $2, $4);
	}
	| optional_space COMMA arg_string_or_empty COMMA arg_string_or_empty RIGHT_PAREN {
		$$ = anjuta_token_merge ($1 != NULL ? $1 :  $2, $6);
	}
	;

arg_string_or_empty:
	/* empty */
	| SPACE
	| arg_string
	;

arg_string:
	arg
	| SPACE arg 
	| arg_string arg
	| arg_string SPACE
	;

comma_separator:
	optional_space COMMA optional_space {
		$$ = anjuta_token_merge (
			anjuta_token_insert_before ($1 != NULL ? $1 : $2,
					anjuta_token_new_static (ANJUTA_TOKEN_NEXT, NULL)),
			$3 != NULL ? $3 : $2);
	}
	;

name: 
	NAME
	| MACRO
	| OPERATOR
	| name MACRO {
		$$ = anjuta_token_merge ($1, $2);
	}
	| name OPERATOR {
		$$ = anjuta_token_merge ($1, $2);
	}
	| name NAME {
		$$ = anjuta_token_merge ($1, $2);
	}
	;


space_list:
	name {
		$$ = anjuta_token_merge (
			anjuta_token_insert_before ($1,
					anjuta_token_new_static (ANJUTA_TOKEN_LIST, NULL)),
			$1);
	}
	| space_list SPACE name {
		anjuta_token_merge ($1, $3);
	}
	;

optional_space:
	/* empty */ {
		$$ = NULL;
	}
	| SPACE {
		anjuta_token_set_flags ($1, ANJUTA_TOKEN_IRRELEVANT);
	}
	;

arg:
	IDENTIFIER
	| OPERATOR
	| NUMBER
	| NAME
	| MACRO            
	;

token:
	SPACE
	| IDENTIFIER
	| OPERATOR
	| NUMBER
	| NAME
	| MACRO
	| COMMA
	| RIGHT_PAREN
	;            

%%
    
static void
amp_ac_yyerror (YYLTYPE *loc, void *scanner, char const *s)
{
    g_message ("(%d:%d-%d:%d) %s\n", loc->first_line, loc->first_column, loc->last_line, loc->last_column, s);
}

/* Public functions
 *---------------------------------------------------------------------------*/

