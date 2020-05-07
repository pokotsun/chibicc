#include "chibicc.h"

int main(int argc, char**argv) {
	if(argc != 2) {
        error("%s: invalid number of arguments", argv[0]);
		return 1;
	}

	// tokenizer
	Token *tok = tokenize(argv[1]);
	Node *node = expr(tok);
	code_gen(node);

	return 0;
}
