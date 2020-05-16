#include "chibicc.h"

static int labelseq = 1; // ラベルのindex

static void gen_addr(Node *node) {
    if(node->kind == ND_VAR) {
        // [rbp-%d] アドレスの値をraxに入れる
        printf("  lea rax, [rbp-%d]\n", node->var->offset);
        printf("  push rax\n");
        return;
    }

    error("not an lvalue");
}

static void load() {
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

// store data to variable
static void store() {
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n");
    printf("  push rdi\n");
}

// generate code for a given node
static void gen(Node *node) {
	if(node->kind == ND_NUM) {
		printf("  push %d\n", node->val);
		return;
	}

    // gen statement stack上に値を残さない
    switch(node->kind) {
        case ND_NUM:
            printf("  push %d\n", node->val);
            return;
        case ND_VAR:
            gen_addr(node);
            load();
            return;
        case ND_ASSIGN:
            gen_addr(node->lhs);
            gen(node->rhs);
            store();
            return;
        case ND_IF: {
            int seq = labelseq++;
            if(node->els) {
                gen(node->cond);
                printf("  pop rax\n");
                printf("  cmp rax, 0\n");
                printf("  je .L.else.%d\n", seq);
                gen(node->then);
                printf("  jmp .L.end.%d\n", seq);
                printf(".L.else.%d:\n", seq);
                gen(node->els);
                printf(".L.end.%d:\n", seq);
            } else {
				gen(node->cond);
				printf("  pop rax\n");
				printf("  cmp rax, 0\n");
				printf("  je  .L.end.%d\n", seq);
				gen(node->then);
				printf(".L.end.%d:\n", seq);
            }
            return;
        }
		case ND_WHILE: {
			int seq = labelseq++;
			printf(".L.begin.%d:\n", seq);
			gen(node->cond);
			printf("  pop rax\n");
			printf("  cmp rax, 0\n");
			printf("  je  .L.end.%d\n", seq);
			gen(node->then);
			printf("  jmp .L.begin.%d\n", seq);
			printf(".L.end.%d:\n", seq);
			return;
	    }
		case ND_FOR: {
			int seq = labelseq++;
			if(node->init) {
				gen(node->init);
			}
			printf(".L.begin.%d:\n", seq);
			if(node->cond) {
				gen(node->cond);
				printf("  pop rax\n");
				printf("  cmp rax, 0\n");
				printf("  je  .L.end.%d\n", seq);
			}
			gen(node->then);
			if(node->inc) {
				gen(node->inc);
			}
			printf("  jmp  .L.begin.%d\n", seq);
			printf(".L.end.%d:\n", seq);
			return;
		}
        case ND_EXPR_STMT:
            gen(node->lhs);
			// 式を評価した結果を捨てるためにstackを戻す
            printf("  add rsp, 8\n");
            return;
		case ND_BLOCK:
			for(Node *n = node->body; n; n=n->next) {
				gen(n);
			}
            return;
        case ND_FUNCALL:
            printf("  call %s\n", node->funcname);
            printf("  push rax\n");
            return;
        case ND_RETURN:
            gen(node->lhs);
            // raxにpopしてから呼び出し元に戻る
            printf("  pop rax\n");
            printf("  jmp .L.return\n");
            return;
    }

	gen(node->lhs);
	gen(node->rhs);

	printf("  pop rdi\n");
	printf("  pop rax\n");

    // gen expression stack上に値を1つ残す
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

void codegen(Function *prog) {
	// アセンブリの前半部分を出力
	printf(".intel_syntax noprefix\n");
	printf(".global main\n");
	printf("main:\n");

    // Prologue
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", prog->stack_size); // main関数内のlocal変数の領域を確保

    // Emit Code
    // gen for each statement;
    for(Node *node=prog->node; node; node=node->next) {
        gen(node);
    }

    // Epilogue
    printf(".L.return:\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");

	printf("  ret\n");
}
