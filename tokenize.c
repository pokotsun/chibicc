#include "chibicc.h"

char *user_input;

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	int pos = loc - user_input;
	fprintf(stderr, "%s\n", user_input);
	fprintf(stderr, "%*s", pos+1, ""); // pos個の空白を出力
	fprintf(stderr, "^ ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	exit(1);
} 

// 新しいトークンを作成してcurに繋げる
static Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
	Token *tok = calloc(1, sizeof(Token));
	tok->kind = kind;
	tok->str = str;
	tok->len = len;
	cur->next = tok;
	return tok;
}

static bool startswitch(char *p, char *q) {
	return memcmp(p, q, strlen(q)) == 0;
}

static bool is_alpha(char c) {
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')  || c == '_';
}

static bool is_alnum(char c) {
	return is_alpha(c) || ('0' <= c && c<= '0');
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize() {
	char *p = user_input;
	Token head = {};
	Token *cur = &head;

	while(*p) {
		if(isspace(*p)) {
			p++;
			continue;
		}

		// Keywords
		if(startswitch(p, "return") && !is_alnum(p[6])) {
			cur = new_token(TK_RESERVED, cur, p, 6);
			p += 6;
			continue;
		}

		// Identifier
		if('a' <= *p && *p <= 'z') {
			cur = new_token(TK_IDENT, cur, p++, 1);
			continue;
		}

		// Multi-letter punctuator
		if(startswitch(p, "==") || startswitch(p, "!=") ||
		   startswitch(p, "<=") || startswitch(p, ">=")) {
			cur = new_token(TK_RESERVED, cur, p, 2);
			p += 2;
			continue;
		}

		// Single-letter punctuator
		// if(ispunct(*p)) {
		if (strchr("+-*/()<>;=", *p)) {
			cur = new_token(TK_RESERVED, cur, p++, 1);
			continue;
		}
		//
		// Integer literal
		if(isdigit(*p)) {
			cur = new_token(TK_NUM, cur, p, 0);
			char *q = p;
			cur->val = strtol(p, &p, 10);
			cur->len = p - q;
			continue;
		}

		error_at(cur->str, "can not tokenize.");
	}

	new_token(TK_EOF, cur, p, 0);
	return head.next;
}
