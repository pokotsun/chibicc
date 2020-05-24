#include "chibicc.h"

static int labelseq = 1; // ラベルのindex
static char *funcname;

// 汎用レジスタの下1bitだけ
static char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static void gen(Node *node);

// pushes the given node's address to the stack.
static void gen_addr(Node *node) {
    switch(node->kind) {
        case ND_VAR: {
            Var *var = node->var;
            if(var->is_local) {
                // [rbp-%d] アドレスの値をraxに入れる
                printf("  lea rax, [rbp-%d]\n", var->offset);
                printf("  push rax\n");
            } else { // global
                printf("  push offset %s\n", var->name);
            }
            return;
        }
        case ND_DEREF:
            gen(node->lhs);
            return;
    }

    error_tok(node->tok, "not an lvalue");
}

static void gen_lval(Node *node) {
    if(node->ty->kind == TY_ARRAY) {
        error_tok(node->tok, "not an lvalue");
    }
    gen_addr(node);
}

static void load(Type *ty) {
    printf("  pop rax\n");
    if(ty->size == 1) {
        printf("  movsx rax, byte ptr [rax]\n");
    } else {
        printf("  mov rax, [rax]\n");
    }
    printf("  push rax\n");
}

// store data to variable
static void store(Type *ty) {
    printf("  pop rdi\n");
    printf("  pop rax\n");
    if(ty->size == 1) { // charのとき
        printf("  mov [rax], dil\n");
    } else {
        printf("  mov [rax], rdi\n");
    }
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
        case ND_NULL:
            return;
        case ND_VAR:
            gen_addr(node);
            if(node->ty->kind != TY_ARRAY) {
                load(node->ty);
            }
            return;
        case ND_ASSIGN:
            gen_lval(node->lhs);
            gen(node->rhs);
            store(node->ty);
            return;
        case ND_ADDR:
            gen_addr(node->lhs);
            return;
        case ND_DEREF:
            gen(node->lhs);
            if(node->ty->kind != TY_ARRAY) {
                load(node->ty);
            }
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
        case ND_STMT_EXPR:
			for(Node *n = node->body; n; n=n->next) {
				gen(n);
			}
            return;
        case ND_FUNCALL: {
            int nargs = 0;
            for(Node *arg = node->args; arg; arg=arg->next) {
                gen(arg);
                nargs++;
            }
            for(int i=nargs-1; i>=0; i--) {
                printf("  pop %s\n", argreg8[i]);
            }

            // We need to align RSP to a 16 byte boundary because it is ABI!
            // RAX is set to 0 for variadict function.
            int seq = labelseq++;
            printf("  mov rax, rsp\n");
            // 0でない場合jump
            // 16で割り切れない <- 15とANDをとって0にならない
            printf("  and rax, 15\n");
            printf("  jnz .L.call.%d\n", seq);
            printf("  mov rax, 0\n"); // 0にしておかないと次の関数を呼ぶときのノイズになる
            printf("  call %s\n", node->funcname);
            printf("  jmp .L.end.%d\n", seq);
            printf(".L.call.%d:\n", seq);
            printf("  sub rsp, 8\n"); // padding to align 16 byte boundary
            printf("  mov rax, 0\n");
            printf("  call %s\n", node->funcname);
            printf("  add rsp, 8\n"); // remove padding
            printf(".L.end.%d:\n", seq);
            printf("  push rax\n");
            return;
        }
        case ND_RETURN:
            gen(node->lhs);
            // raxにpopしてから呼び出し元に戻る
            printf("  pop rax\n");
            printf("  jmp .L.return.%s\n", funcname);
            return;
    }

	gen(node->lhs);
	gen(node->rhs);

	printf("  pop rdi\n");
	printf("  pop rax\n");

    // gen expression stack上に値を1つ残す
	switch (node->kind) {
        case ND_NUM:
            printf("  push %d\n", node->val);
            return;
		case ND_ADD:
			printf("  add rax, rdi\n");
			break;
        case ND_PTR_ADD:
            printf("  imul rdi, %d\n", node->ty->base->size); // 型のサイズ分をかける
            printf("  add rax, rdi\n");
            break;
		case ND_SUB:
			printf("  sub rax, rdi\n");
			break;
        case ND_PTR_SUB:
            printf("  imul rdi, %d\n", node->ty->base->size); // 型のサイズ分をかける
            printf("  sub rax, rdi\n");
            break;
        case ND_PTR_DIFF:
            printf("  sub rax, rdi\n");
            printf("  cqo\n"); // raxを符号拡張して rdx:raxに設定
            printf("  idiv %d\n", node->lhs->ty->base->size); // rdx:raxから型のサイズ分除算
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

// set global data
static void emit_data(Program *prog) {
    printf(".data\n");

    for(VarList *vl=prog->globals; vl; vl=vl->next) {
        Var *var = vl->var;
        printf("%s:\n", var->name);

        if(!var->contents) {
            printf("  .zero %d\n", var->ty->size);
            continue;
        } 

        for(int i=0; i<var->cont_len; i++) {
            printf("  .byte %d\n", var->contents[i]);
        }
    }
}

static void load_arg(Var *var, int idx) {
    int sz = var->ty->size;
    if(sz == 1) {
        printf("  mov [rbp-%d], %s\n", var->offset, argreg1[idx]);
    } else {
        assert(sz==8);
        printf("  mov [rbp-%d], %s\n", var->offset, argreg8[idx]);
    }
}

// set program code
static void emit_text(Program *prog) {
    printf(".text\n");

    for(Function *fn = prog->fns; fn; fn=fn->next) {
        printf(".global %s\n", fn->name);
        printf("%s:\n", fn->name);
        funcname = fn->name;
        
        // Prologue
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", fn->stack_size); // main関数内のlocal変数の領域を確保

        // Push arguments to the stack.
        int i = 0;
        for(VarList *vl = fn->params; vl; vl = vl->next) {
            // 関数を指しているrbpの次から引数を積む(6個まで)
            load_arg(vl->var, i++);
        }
        
        // Emit Code
        // gen for each statement;
        for(Node *node=fn->node; node; node=node->next) {
            gen(node);
        }
        //
        // Epilogue
        printf(".L.return.%s:\n", funcname);
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
    }
}

void codegen(Program *prog) {
	// アセンブリの最初1行を出力
	printf(".intel_syntax noprefix\n");
    emit_data(prog);
    emit_text(prog);
}
