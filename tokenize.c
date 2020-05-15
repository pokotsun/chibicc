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
	return is_alpha(c) || ('0' <= c && c<= '9');
}

static char *starts_with_reserved(char *p) {
    // Keyword
    static char *kw[] = {"return", "if", "else", "while", "for"};   

    for(int i=0; i<sizeof(kw) / sizeof(*kw); i++) {
        int len = strlen(kw[i]);
        if(startswitch(p, kw[i]) && !is_alnum(p[len])) {
            return kw[i];
        }
    }

    // Multi letter punctuator
    static char *ops[] = {"==", "!=", "<=", ">="};

    for(int i=0; i<sizeof(ops) / sizeof(*ops); i++) {
        if(startswitch(p, ops[i])) {
            return ops[i];
        }
    }
    return NULL;
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

        // Keywords or multi-letter punctuators
        char *kw = starts_with_reserved(p);
        if(kw) {
            int len = strlen(kw);
            cur = new_token(TK_RESERVED, cur, p, len);
            p += len;
            continue;
        }

		// Identifier
		if(is_alpha(*p)) {
			char *q = p++;
			while(is_alnum(*p)) {
				p++;
			}
			cur = new_token(TK_IDENT, cur, q, p - q);
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
