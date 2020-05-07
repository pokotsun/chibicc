#include "chibicc.h"

// current focused token 
Token *token;

// 次のトークンが期待している記号の時には, トークンを1つ読み進めて
// 真を返す. それ以外の場合には偽を返す.
static bool consume(char *op) {
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
static void expect(char *op) {
	if(token->kind != TK_RESERVED ||
		strlen(op) != token->len ||
		memcmp(token->str, op, token->len)) {
		error_at(token->str, "not '%c'.", op);
	}
	token = token->next;
}

// 次のトークンが数値の場合, トークンを1つ読み進めてその数値を返す.
// それ以外の場合にはエラーを報告する.
static int expect_number() {
	if(token->kind != TK_NUM) {
		error_at(token->str, "Not Number.");
	}
	int val = token->val;
	token = token->next;
	return val;
}

static Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
	Node *node = calloc(1, sizeof(Node));
	node->kind = kind;
	node->lhs = lhs;
	node->rhs = rhs;
	return node;
}

static Node *new_node_num(int val) {
	Node *node = calloc(1, sizeof(Node));
	node->kind = ND_NUM;
	node->val = val;
	return node;
}

// expr = equality
Node *expr(Token *tok) {
    token = tok;
	return equality();
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality() {
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
static Node *relational() {
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
static Node *add() {
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
static Node *mul() {
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
static Node *unary() {
	if(consume("+")) {
		return unary();
	}
	else if(consume("-")) {
		return new_node(ND_SUB, new_node_num(0), unary());
	}
	return primary();
}

// primary = "(" expr ")" | num
static Node *primary() {
	// 次のトークンが"("なら, "(" expr ")"のはず
	if(consume("(")) {
		Node *node = expr(token);
		expect(")");
		return node;
	}

	return new_node_num(expect_number());
}
