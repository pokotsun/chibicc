#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;
typedef struct Member Member;

// 
// tokenize.c
// 

// kinds of token
typedef enum {
	TK_RESERVED, // 予約語(演算子とか)
    TK_IDENT, // Identifiers
    TK_STR, // String literals
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

    char *contents; // String literal contents including terminating '\0'
    char cont_len; // string literal length
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
Token *tokenize();

// variable
typedef struct Var Var;
struct Var {
    char *name; // Variable name
	Type *ty; // Type
	bool is_local; // local or global 

	// Local variable
    int offset; // Offset from RBP based current func

    // Global variable
    char *contents;
    int cont_len;
};

typedef struct VarList VarList;
struct VarList {
	VarList *next;
	Var *var;
};

// 抽象構文木のノードの種類
typedef enum {
	ND_ADD, // num + num 
	ND_PTR_ADD, // ptr + num or num + ptr
	ND_SUB, // num - num 
	ND_PTR_SUB, // ptr - num
	ND_PTR_DIFF, // ptr - ptr
	ND_MUL, // *
	ND_DIV, // /
	ND_EQ,  // ==
	ND_NE,  // !=
	ND_LT,  // <
	ND_LE,  // <=
    ND_ASSIGN, // =
    ND_MEMBER, // . (struct member access)
	ND_ADDR, // unary &
	ND_DEREF, // unary *
    ND_RETURN, // "return"
    ND_IF, // "if"
    ND_WHILE, // "while"
    ND_FOR, // "for"
    ND_BLOCK, // { ... }
	ND_FUNCALL, // Function call
    ND_EXPR_STMT, // Expression statement(式が評価されるが何もスタックに残さないようにする)
    ND_STMT_EXPR, // Statement expression 複数文が存在するが最後の文の結果だけstackに残す
    ND_VAR, // Variable
	ND_NUM, // Integer
	ND_NULL, // Empty statement
} NodeKind;

// AST node type 
typedef struct Node Node;
struct Node {
	NodeKind kind; // Node kind
    Node *next; // next Node
	Type *ty; // Type, e.g. int or pointer to int
	Token *tok; // Representatie token

	Node *lhs; // Left-hand side
	Node *rhs; // Right-hand side

    // "if", "while" or "for" statement
    Node *cond; // 条件式
    Node *then;
    Node *els;
    Node *init; // only use "for"
    Node *inc; // only use "for"

    // Block or statement expression
    Node *body;

    // Struct member access
    Member *member;

	// Function call
	char *funcname;
	Node *args;

    Var *var; // 変数の時だけ使う
	int val; // kindがND_NUMの場合のみ使う
};

extern Token *token;
extern char *filename;
extern char *user_input;

//
// parser.c
//

typedef struct Function Function;
struct Function {
	Function *next;
	char *name;
	VarList *params;
    Node *node;
	VarList *locals;
    int stack_size;
};

typedef struct {
	VarList *globals;
	Function *fns;
} Program;

Program *program();

// 
// typing.c
//
typedef enum { 
    TY_CHAR, 
    TY_INT, 
    TY_PTR, 
    TY_ARRAY,
    TY_STRUCT,
} TypeKind;

struct Type {
	TypeKind kind;
	int size; // sizeof() value
    int align; // alignment
	Type *base; // pointer or array
	int array_len; // array
    Member *members; // struct
};

// Struct member
struct Member {
    Member *next;
    Type *ty;
    char *name;
    int offset;
};

extern Type *char_type;
extern Type *int_type;

bool is_integer(Type *ty);
int align_to(int n, int align);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
void add_type(Node *node);

//
// codegen.c
//
void codegen(Program *prog);

