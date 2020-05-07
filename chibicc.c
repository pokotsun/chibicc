#include "chibicc.h"


int main(int argc, char**argv) {
	if(argc != 2) {
		fprintf(stderr, "invalid arguments\n");
		return 1;
	}

	// tokenizer
	Token *token = tokenize(argv[1]);
	Node *node = expr(token);

	code_gen(node);

	return 0;
}
