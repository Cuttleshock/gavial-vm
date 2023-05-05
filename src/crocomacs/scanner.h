// TODO: Since it's really just an internal module of crocomacs/parser, does
// crocomacs/scanner really need to exist?

#ifndef CROCOMACS_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef CROCOMACS_SCANNER_H
#define CROCOMACS_SCANNER_H

typedef enum {
	// Literals
	TOKEN_STRING,
	TOKEN_NUMBER,
	// Names
	TOKEN_MACRO,
	TOKEN_PRIMITIVE,
	TOKEN_SYMBOL,
	TOKEN_DECLARATION,
	TOKEN_PARAMETER,
	// Punctuation
	TOKEN_LEFT_PAREN,
	TOKEN_RIGHT_PAREN,
	TOKEN_COMMA,
	// Special
	TOKEN_ERROR,
	TOKEN_EOF,
} TokenType;

typedef struct {
	TokenType type;
	const char *chars;
	int length;
	int line;
	int character;
} Token;

void set_source(const char *chars, int length, int initial_line);
void print_token(Token t);
Token scan_token();

#endif // CROCOMACS_SCANNER_H
