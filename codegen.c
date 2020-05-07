#include "chibicc.h"

static char *reg(int idx) {
    static char *r[] = {"r10", "r11", "r12", "r13", "r14", "r15"};
    if(idx < 0 || sizeof(r) / sizeof(*r) <= idx) {
        error("Register out of range: %d", idx);
    }
    return r[idx];
}

static void gen_expr(Node *node) {
	if(node->kind == ND_NUM) {
		printf("  push %d\n", node->val);
		return;
	}

	gen_expr(node->lhs);
	gen_expr(node->rhs);

	printf("  pop rdi\n");
	printf("  pop rax\n");

	switch (node->kind) {
		case ND_ADD:
			printf("  add rax, rdi\n");
			break;
		case ND_SUB:
			printf("  sub rax, rdi\n");
			break;
		case ND_MUL:
			printf("  imul rax, rdi\n");
			break;
		case ND_DIV:
			printf("  cqo\n");
			printf("  idiv rdi\n");
			break;
		case ND_EQ:
			printf("  cmp rax, rdi\n");
			printf("  sete al\n");
			printf("  movzb rax, al\n");
			break;
		case ND_NE:
			printf("  cmp rax, rdi\n");
			printf("  setne al\n");
			printf("  movzb rax, al\n");
			break;
		case ND_LT:
			printf("  cmp rax, rdi\n");
			printf("  setl al\n");
			printf("  movzb rax, al\n");
			break;
		case ND_LE:
			printf("  cmp rax, rdi\n");
			printf("  setle al\n");
			printf("  movzb rax, al\n");
			break;
	}

	printf("  push rax\n");
}

void code_gen(Node *node) {
	// アセンブリの前半部分を出力
	printf(".intel_syntax noprefix\n");
	printf(".global main\n");
	printf("main:\n");

	gen_expr(node);

	// スタックトップに式全体の値が残っているはずなので
	// それをRAXにpopして関数からの返り値とする
	printf("  pop rax\n");

	printf("  ret\n");
}
