#include<ctype.h>
#include<stdarg.h>
#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

// input
char *user_input;

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	int pos = loc - user_input;
	fprintf(stderr, "%s\n", user_input);
	fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
	fprintf(stderr, "^ ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	exit(1);
} 

// kinds of token
typedef enum {
	TK_RESERVED, // symbol
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
};

// current focused token 
Token *token;

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	exit(1);
}

// 次のトークンが期待している記号の時には, トークンを1つ読み進めて
// 真を返す. それ以外の場合には偽を返す.
bool consume(char op) {
	if(token->kind != TK_RESERVED || token->str[0] != op) {
		return false;
	}
	token = token->next;
	return true;
}

// 次のトークンが期待している記号の時には, トークンを1つ読み進める
// それ以外の場合にはエラーを返す.
void expect(char op) {
	if(token->kind != TK_RESERVED || token->str[0] != op) {
		error_at(token->str, "not '%c'.", op);
	}
	token = token->next;
}

// 次のトークンが数値の場合, トークンを1つ読み進めてその数値を返す.
// それ以外の場合にはエラーを報告する.
int expect_number() {
	if(token->kind != TK_NUM) {
		error_at(token->str, "Not Number.");
	}
	int val = token->val;
	token = token->next;
	return val;
}

bool at_eof() {
	return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str) {
	Token *tok = calloc(1, sizeof(Token));
	tok->kind = kind;
	tok->str = str;
	cur->next = tok;
	return tok;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p) {
	Token head;
	head.next = NULL;
	Token *cur = &head;

	while(*p) {
		if(isspace(*p)) {
			p++;
			continue;
		}

		if(*p == '+' || *p == '-') {
			cur = new_token(TK_RESERVED, cur, p++);
			continue;
		}

		if(isdigit(*p)) {
			cur = new_token(TK_NUM, cur, p);
			cur->val = strtol(p, &p, 10);
			continue;
		}

		error_at(cur->str, "can not tokenize.");
	}

	new_token(TK_EOF, cur, p);
	return head.next;
}

int main(int argc, char**argv) {
	if(argc != 2) {
		fprintf(stderr, "invalid arguments\n");
		return 1;
	}

	user_input = argv[1];
	// tokenizer
	token = tokenize(user_input);

	// アセンブリの前半部分を出力
	printf(".intel_syntax noprefix\n");
	printf(".global main\n");
	printf("main:\n");

	// 式の最初は数でなければならないので, それをチェックして
	// 最初のmov命令を出力
	printf("  mov rax, %d\n", expect_number());

	// `+ <num>`or `- <num>`というトークンの並びを消費しつつ
	// アセンブリを出力
	while (!at_eof()) {
		if(consume('+')) {
			printf("  add rax, %d\n", expect_number());
			continue;
		}

		expect('-');
		printf("  sub rax, %d\n", expect_number());
	}

	printf("  ret\n");
	return 0;
}
