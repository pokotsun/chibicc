#include "chibicc.h"


int main(int argc, char**argv) {
	if(argc != 2) {
		fprintf(stderr, "invalid arguments\n");
		return 1;
	}

	// tokenizer
	Token *token = tokenize(argv[1]);
	Node *node = expr(token);

	// アセンブリの前半部分を出力
	printf(".intel_syntax noprefix\n");
	printf(".global main\n");
	printf("main:\n");

	gen(node);

	// スタックトップに式全体の値が残っているはずなので
	// それをRAXにpopして関数からの返り値とする
	printf("  pop rax\n");

	printf("  ret\n");
	return 0;
}
