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
    Function *prog = program();

    // prog->stack_size = offset;
    for(Function *fn=prog; fn; fn=fn->next) {
        // Assign offsets to function variables.
        int offset = 0;
        for(Var *var=prog->locals; var; var=var->next) {
            offset += 8;
            var->offset = offset;
        }
        fn->stack_size = offset;
    }

    // Traverse the AST to emit assembly.
    codegen(prog);

	return 0;
}
