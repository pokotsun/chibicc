#include <assert.h>
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
void error_tok(Token *tok, char *fmt, ...);
static Token *new_token(TokenKind kind, Token *cur, char *str, int len); 
static bool startswitch(char *p, char *q);
Token *tokenize();


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
    ND_RETURN, // "return"
    ND_EXPR_STMT, // Expression statement
	ND_NUM, // Integer
} NodeKind;

// AST node type 
typedef struct Node Node;
struct Node {
	NodeKind kind; // Node kind
    Node *next; // next Node
	Node *lhs; // Left-hand side
	Node *rhs; // Right-hand side
	int val; // kindがND_NUMの場合のみ使う
};

extern Token *token;
extern char *user_input;

//
// parser.c
//

Node *program();
static Node *stmt();
static Node *expr();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *primary();
static Node *unary();

//
// codegen.c
//
void code_gen(Node *node);
