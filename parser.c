#include "chibicc.h"

// All local variable instances created during parsing are
// accumulated to this list
Var *locals;

// Find a local variable by name.
static Var *find_var(Token *tok) {
    for(Var *var = locals; var; var=var->next) {
        if(strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len)) {
            return var;
        }
    }
    return NULL;
}

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

static Token *consume_ident() {
    if(token->kind != TK_IDENT) {
        return NULL;
    }
    Token *t = token;
    token = token->next;
    return t;
}

// 次のトークンが期待している記号の時には, トークンを1つ読み進める
// それ以外の場合にはエラーを返す.
static void expect(char *op) {
	if(token->kind != TK_RESERVED ||
		strlen(op) != token->len ||
		memcmp(token->str, op, token->len)) {
		error_at(token->str, "not '%s'.", op);
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

static bool at_eof() {
    return token->kind == TK_EOF;
}

// generate new template node
static Node *new_node(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = new_node(kind);
	node->lhs = lhs;
	node->rhs = rhs;
	return node;
}

static Node *new_unary(NodeKind kind, Node *expr) {
    Node *node = new_node(kind);
    node->lhs = expr;
    return node;
}

static Node *new_num(int val) {
    Node *node = new_node(ND_NUM);
	node->val = val;
	return node;
}

static Node *new_var_node(Var *var) {
    Node *node = new_node(ND_VAR);
    node->var = var;
    return node;
}

// assign new variable
static Var *new_lvar(char *name) {
    Var *var = calloc(1, sizeof(Var));
    var->next = locals;
    var->name = name;
    locals = var;
    return var;
}

static Node *stmt();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *primary();
static Node *unary();

// program = stmt*
Function *program() {
    locals = NULL;

    Node head = {};
    Node *cur = &head;

    while(!at_eof()) {
        cur->next = stmt();
        cur = cur->next;
    }

    Function *prog = calloc(1, sizeof(Function));
    prog->node = head.next;
    prog->locals = locals;
    return prog;
}

static Node *read_expr_stmt() {
    return new_unary(ND_EXPR_STMT, expr());
}

// stmt = "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | expr ";"
static Node *stmt() {
    if(consume("return")) {
        Node *node = new_unary(ND_RETURN, expr());
        expect(";");
        return node;
    }

    if(consume("while")) {
        Node *node = new_node(ND_WHILE);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if(consume("if")) {
        Node *node = new_node(ND_IF);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if(consume("else")) {
            node->els = stmt();
        }
        return node;
    }
    Node *node = read_expr_stmt();
    expect(";");
    return node;
}

// expr = assign
static Node *expr() {
    return assign();
}

// assign = equality ("=" assign)?
static Node *assign() {
    Node *node = equality();
    if(consume("=")) {
        node = new_binary(ND_ASSIGN, node, assign());
    }
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality() {
	Node *node = relational();

	while(true) {
		if (consume("==")) {
			node = new_binary(ND_EQ, node, relational());
		} else if(consume("!=")) {
			node = new_binary(ND_NE, node, relational());
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
			node = new_binary(ND_LT, node, add());
		} else if(consume("<=")) {
			node = new_binary(ND_LE, node, add());
		} else if(consume(">")) {
			node = new_binary(ND_LT, add(), node);
		} else if(consume(">=")) {
			node = new_binary(ND_LE, add(), node);
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
			node = new_binary(ND_ADD, node, mul());
		} else if(consume("-")) {
			node = new_binary(ND_SUB, node, mul());
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
			node = new_binary(ND_MUL, node, unary());
		} else if(consume("/")) {
			node = new_binary(ND_DIV, node, unary());
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
		return new_binary(ND_SUB, new_num(0), unary());
	}
	return primary();
}

// primary = "(" expr ")" | ident | num
static Node *primary() {
	// 次のトークンが"("なら, "(" expr ")"のはず
	if(consume("(")) {
		Node *node = expr();
		expect(")");
		return node;
	}

    Token *tok = consume_ident();

    if(tok) {
        Var *var = find_var(tok);
        if(!var) {
            // strndup(str, len): assign copy string of str.slice(0 until len)
            var = new_lvar(strndup(tok->str, tok->len));
        }
        return new_var_node(var);
    }

	return new_num(expect_number());
}
