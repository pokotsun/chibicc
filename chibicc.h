#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 
// tokenize.c
// 

// kinds of token
typedef enum {
	TK_RESERVED, // 予約語(演算子とか)
	TK_NUM, // number token
	TK_EOF, // end of input
} TokenKind;

typedef struct Token Token;

// token type
struct Token {
	TokenKind kind; // token type
	Token *next; // next input token
	int val; // kindがTK_NUMの場合, その数値
	char *str; // トークン文字列
	int len; // トークンの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
Token *new_token(TokenKind kind, Token *cur, char *str, int len); 
bool startswitch(char *p, char *q);
Token *tokenize(char *p);

