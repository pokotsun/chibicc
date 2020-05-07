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


// 抽象構文木のノードの種類
typedef enum {
	ND_ADD, // +
	ND_SUB, // -
	ND_MUL, // *
	ND_DIV, // /
	ND_EQ,  // ==
	ND_NE,  // !=
	ND_LT,  // <
	ND_LE,  // <=
	ND_NUM, // number 
} NodeKind;

// AST node type 
typedef struct Node Node;
struct Node {
	NodeKind kind; // ノードの型
	Node *lhs; // 左辺
	Node *rhs; // 右辺
	int val; // kindがND_NUMの場合のみ使う
};

//
// parser.c
//
Node *expr(Token *token);
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *primary();
Node *unary();


//
// codegen.c
//
void code_gen(Node *node);
