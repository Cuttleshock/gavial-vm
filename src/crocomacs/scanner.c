#include <stdbool.h>
#include <string.h>

#define CROCOMACS_IMPL
#include "common.h"
#include "scanner.h"

// TODO: Watch those NULLs
static const char *source = NULL;
static const char *end = NULL;
static const char *token_start = NULL;
static const char *tapehead = NULL;
// These could be calculated on the fly but the hassle and inefficiency of doing
// so outweighs the downsides of making them global
static int token_start_line = 0;
static int token_start_character = 0;
static int line = 0;
static int character = 0;

static Token token_at(TokenType type, const char *chars, int length)
{
	Token token;
	token.type = type;
	token.chars = chars;
	token.length = length;
	token.line = token_start_line;
	token.character = token_start_character;
	return token;
}

static Token make_token(TokenType type)
{
	return token_at(type, token_start, tapehead - token_start);
}

static Token error_token(const char *message)
{
	return token_at(TOKEN_ERROR, message, strlen(message));
}

// TODO: Potential danger if tapehead overruns end, but weakening to '>='
// encourages advance()ing without is_at_end()ing, which you shouldn't anyway.
static bool is_at_end()
{
	return tapehead == end;
}

static char peek()
{
	return *tapehead;
}

static char advance()
{
	switch (*tapehead) {
		case '\n':
			++line;
			character = 1;
			break;
		case '\r':
			break;
		default:
			++character;
			break;
	}
	return *(++tapehead);
}

static void start_token()
{
	token_start = tapehead;
	token_start_line = line;
	token_start_character = character;
}

// TODO: More exhaustive?
// wsp: [\s;]
static bool is_whitespace(char c)
{
	return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ';';
}

// [\d]
static bool is_digit(char c)
{
	return '0' <= c && c <= '9';
}

// [\-\d]
static bool starts_number(char c)
{
	return is_digit(c) || '-' == c;
}

// Characters that can come after a Number, String, Macro or Symbol
// end: [(wsp)\(\),]
static bool ends_token(char c)
{
	return is_whitespace(c) || c == '(' || c == ')' || c == ',';
}

// Characters that cannot be part of a Macro or Symbol
// res: [(end)"`:?]
static bool is_special(char c)
{
	return ends_token(c) || c == '"' || c == '#' || c == '`' || c == ':' || c == '?';
}

// .*
static void consume_comment()
{
	while (!is_at_end() && peek() != '\n') {
		advance();
	}
}

// (\s|;.*)*
static void consume_whitespace()
{
	while (!is_at_end()) {
		if (peek() == ';') {
			consume_comment();
		} else if (is_whitespace(peek())) {
			advance();
		} else {
			return;
		}
	}
}

// number: \d+(\.\d*)?[(end)$]
// TODO: It would make life easier if we just strtod'd right now (adding a
// field to Token). Regardless, should we allow leading zeroes?
static Token number()
{
	char c = advance();
	while (!is_at_end() && is_digit(c)) {
		c = advance();
	}
	// v   v
	// 1234?

	if (!is_at_end() && '.' == c) {
		c = advance();
		// v    v
		// 1234.?
		while (!is_at_end() && is_digit(c)) {
			c = advance();
		}
		// v      v
		// 1234.56?
	}

	if (is_at_end() || ends_token(c)) {
		// v      v
		// 1234.56,
		return make_token(TOKEN_NUMBER);
	} else {
		// v      v
		// 1234.56a
		return error_token("Malformed number");
	}
}

// -(number)
static Token negative_number()
{
	char c = advance();
	if (is_at_end() || !is_digit(c)) {
		return error_token("Malformed number");
	}
	// vv
	// -2
	return number();
}

// "([^"\\]|\\.)*"[(end)$]
static Token string()
{
	char c = advance();
	// vv
	// "?
	while (!is_at_end() && '"' != c) {
		if ('\\' == c) {
			// v    v
			// "ab 2\?
			advance();
			if (is_at_end()) {
				// v     v
				// "ab 2\$
				return error_token("Unterminated string");
			}
		}
		c = advance();
		// v      v
		// "ab 2\??
	}

	if (is_at_end()) {
		// v      v
		// "ab 2\"$
		return error_token("Unterminated string");
	}

	c = advance();
	if (is_at_end() || ends_token(c)) {
		// v       v
		// "ab 2\"";
		return make_token(TOKEN_STRING);
	} else {
		// v       v
		// "ab 2\""4
		return error_token("Malformed string");
	}
}

// str_ident: [^(res)\d\-][^(res)]*[(end)$]
// num_ident: [^(res)]+[(end)$]
// ident: ((str_ident)|(num_ident))
static Token identifier(TokenType type, const char *empty_error, const char *malformed_error, bool allow_numeric)
{
	char c = peek();
	if (is_at_end() || is_special(c) || (!allow_numeric && starts_number(c))) {
		// vv    vv
		// `2 or ?"
		return error_token(empty_error);
	}

	while (!is_at_end() && !is_special(c)) {
		c = advance();
		// v      v
		// `asd%5s?
	}

	if (is_at_end() || ends_token(c)) {
		// v      v
		// `asd%5s(
		return make_token(type);
	} else {
		// v      v
		// `asd%5s"
		return error_token(malformed_error);
	}
}

// #(str_ident)
static Token primitive()
{
	advance(); // skip #
	return identifier(TOKEN_PRIMITIVE, "Empty primitive name", "Malformed primitive", false);
}

// `(str_ident)
static Token symbol()
{
	advance(); // skip `
	return identifier(TOKEN_SYMBOL, "Empty symbol name", "Malformed symbol", false);
}

// :(str_ident)
static Token declaration()
{
	advance(); // skip :
	return identifier(TOKEN_DECLARATION, "Empty macro name", "Malformed macro declaration", false);
}

// ?(num_ident)
// Unlike macros, primitives and symbols, parameters may start with a number
static Token parameter()
{
	advance(); // skip ?
	return identifier(TOKEN_PARAMETER, "Empty parameter name", "Malformed parameter", true);
}

// (str_ident)
static Token macro()
{
	//                             (impossible)
	return identifier(TOKEN_MACRO, "Empty macro name", "Malformed macro", false);
}

void set_source(const char *chars, int length, int initial_line)
{
	source = chars;
	line = initial_line;
	character = 1;
	end = chars + length;
	token_start = chars;
	tapehead = chars;
}

static Token scan_token_impl()
{
	consume_whitespace();
	start_token();

	if (is_at_end()) {
		return make_token(TOKEN_EOF);
	}

	char c = peek();

	if (is_digit(c)) {
		return number();
	}

	switch (c) {
		case '-':
			return negative_number();
		case '"':
			return string();
		case '#':
			return primitive();
		case '`':
			return symbol();
		case ':':
			return declaration();
		case '?':
			return parameter();
		case '(':
			advance();
			return make_token(TOKEN_LEFT_PAREN);
		case ')':
			advance();
			return make_token(TOKEN_RIGHT_PAREN);
		case ',':
			advance();
			return make_token(TOKEN_COMMA);
		default:
			return macro();
	}
}

void print_token(Token t)
{
#define CASE(type) case TOKEN_ ## type : ccm_log("%-11s %.*s\n", #type, t.length, t.chars); break

	switch (t.type) {
		CASE(STRING);
		CASE(NUMBER);
		CASE(MACRO);
		CASE(PRIMITIVE);
		CASE(SYMBOL);
		CASE(DECLARATION);
		CASE(PARAMETER);
		CASE(LEFT_PAREN);
		CASE(RIGHT_PAREN);
		CASE(COMMA);
		CASE(ERROR);
		CASE(EOF);
	}

#undef CASE
}

Token scan_token()
{

	Token t = scan_token_impl();
#ifdef CCM_DEBUG_PRINT_TOKENS
	print_token(t);
#endif
	return t;
}
