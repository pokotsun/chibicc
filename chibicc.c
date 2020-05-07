#include "chibicc.h"

// current focused token 
Token *token;

// 次のトークンが期待している記号の時には, トークンを1つ読み進めて
// 真を返す. それ以外の場合には偽を返す.
bool consume(char *op) {
	if(token->kind != TK_RESERVED ||
		strlen(op) != token->len ||
		memcmp(token->str, op, token->len)) {
		return false;
	}
	token = token->next;
	return true;
}

// 次のトークンが期待している記号の時には, トークンを1つ読み進める
// それ以外の場合にはエラーを返す.
void expect(char *op) {
	if(token->kind != TK_RESERVED ||
		strlen(op) != token->len ||
		memcmp(token->str, op, token->len)) {
		error_at(token->str, "not '%c'.", op);
	}
	token = token->next;
}

// 次のトークンが数値の場合, トークンを1つ読み進めてその数値を返す.
// それ以外の場合にはエラーを報告する.
int expect_number() {
	if(token->kind != TK_NUM) {
		error_at(token->str, "Not Number.");
	}
	int val = token->val;
	token = token->next;
	return val;
}

bool at_eof() {
	return token->kind == TK_EOF;
}


Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
	Node *node = calloc(1, sizeof(Node));
	node->kind = kind;
	node->lhs = lhs;
	node->rhs = rhs;
	return node;
}

Node *new_node_num(int val) {
	Node *node = calloc(1, sizeof(Node));
	node->kind = ND_NUM;
	node->val = val;
	return node;
}

Node *expr();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *primary();
Node *unary();

// expr = equality
Node *expr() {
	return equality();
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
	Node *node = relational();

	while(true) {
		if (consume("==")) {
			node = new_node(ND_EQ, node, relational());
		} else if(consume("!=")) {
			node = new_node(ND_NE, node, relational());
		} else {
			return node;
		}
	}
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
	Node *node = add();

	while(true) {
		if(consume("<")) {
			node = new_node(ND_LT, node, add());
		} else if(consume("<=")) {
			node = new_node(ND_LE, node, add());
		} else if(consume(">")) {
			node = new_node(ND_LT, add(), node);
		} else if(consume(">=")) {
			node = new_node(ND_LE, add(), node);
		} else {
			return node;
		}
	}
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
	Node *node = mul();

	while(true) {
		if(consume("+")) {
			node = new_node(ND_ADD, node, mul());
		} else if(consume("-")) {
			node = new_node(ND_SUB, node, mul());
		} else {
			return node;
		}
	}
}

// mul = unary("*" unary | "/" unary)*
Node *mul() {
	Node *node = unary();

	while(true) {
		if(consume("*")) {
			node = new_node(ND_MUL, node, unary());
		} else if(consume("/")) {
			node = new_node(ND_DIV, node, unary());
		} else {
			return node;
		}
	}
}

// unary = ("+" | "-")? unary
//			| primary
Node *unary() {
	if(consume("+")) {
		return unary();
	}
	else if(consume("-")) {
		return new_node(ND_SUB, new_node_num(0), unary());
	}
	return primary();
}

// primary = "(" expr ")" | num
Node *primary() {
	// 次のトークンが"("なら, "(" expr ")"のはず
	if(consume("(")) {
		Node *node = expr();
		expect(")");
		return node;
	}

	return new_node_num(expect_number());
}


int main(int argc, char**argv) {
	if(argc != 2) {
		fprintf(stderr, "invalid arguments\n");
		return 1;
	}

	// tokenizer
	token = tokenize(argv[1]);
	Node *node = expr();

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
