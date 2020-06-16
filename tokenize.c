#include "chibicc.h"

char *filename;
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

// Reports an error message in the following format and exit.
//
// foo.c:10: x = y + 1;
//               ^ <error message here>
static void verror_at(char *loc, char *fmt, va_list ap) {
    // Find a line containing `loc`.
    char *line = loc;
    while(user_input < line && line [-1] != '\n') {
        line--;
    }

    char *end = loc;
    while(*end != '\n') {
        end++;
    }

    // Get a line number.
    int line_num = 1;
    for(char *p = user_input; p < line; p++) {
        if(*p == '\n') {
            line_num++;
        }
    }

    // Print out the line.
    int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // Show the error message.
    int pos = loc - line + indent;
	fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
	fprintf(stderr, "^ ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
    verror_at(loc, fmt, ap);
	exit(1);
} 

// Reports an error location and exit.
void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->str, fmt, ap);
    exit(1);
}

void warn_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->str, fmt, ap);
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
    static char *kw[] = {"return", "if", "else", "while", "for", "int", 
                         "char", "sizeof", "struct", "typedef", "short", 
                         "long", "void"};

    for(int i=0; i<sizeof(kw) / sizeof(*kw); i++) {
        int len = strlen(kw[i]);
        if(startswitch(p, kw[i]) && !is_alnum(p[len])) {
            return kw[i];
        }
    }

    // Multi letter punctuator
    static char *ops[] = {"==", "!=", "<=", ">=", "->"};

    for(int i=0; i<sizeof(ops) / sizeof(*ops); i++) {
        if(startswitch(p, ops[i])) {
            return ops[i];
        }
    }
    return NULL;
}

static char get_escape_char(char c) {
    switch(c) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        case 'e': return 27;
        case '0': return 0;
        default: return c;
    }
}

static Token *read_string_literal(Token *cur, char *start) {
    char *p = start + 1;
    char buf[1024];
    int len = 0;
    
    while(true) {
        if(len == sizeof(buf)) {
            error_at(start, "string literal too large");
        }
        if(*p == '\0') {
            error_at(start, "unclosed string literal");
        }
        if(*p == '"') {
            break;
        }

        if(*p == '\\') {
            p++;
            buf[len++] = get_escape_char(*p++);
        } else {
            buf[len++] = *p++;
        }
    }
    Token *tok = new_token(TK_STR, cur, start, p - start + 1);
    tok->contents = malloc(len + 1);
    memcpy(tok->contents, buf, len);
    tok->contents[len] = '\0';
    tok->cont_len = len + 1;
    return tok;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize() {
	char *p = user_input;
	Token head = {};
	Token *cur = &head;
    cur->str = p;

	while(*p) {
        // Skip whitespace characters.
		if(isspace(*p)) {
			p++;
			continue;
		}

        // Skip line comments.
        if(startswitch(p, "//")) {
            p += 2;
            while(*p != '\n') {
                p++;
            }
            continue;
        }

        // Skip block comments.
        if(startswitch(p, "/*")) {
            char *q = strstr(p+2, "*/");
            if(!q) {
                error_at(p, "unclosed block comment");
            }
            p = q + 2;
            continue;
        }

        // String literal
        if(*p == '"') {
            cur = read_string_literal(cur, p);
            p += cur->len;
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
		if (strchr("+-*/()<>;={}[],*&.", *p)) {
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
