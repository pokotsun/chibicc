#include "chibicc.h"
Token *token;

int main(int argc, char**argv) {
	if(argc != 2) {
        error("%s: invalid number of arguments", argv[0]);
		return 1;
	}

	// tokenizer
    user_input = argv[1];
	token = tokenize();
	Node *node = program();
	code_gen(node);

	return 0;
}
