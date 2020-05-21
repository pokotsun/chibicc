#include "chibicc.h"

// All local variable instances created during parsing are
// accumulated to this list
static VarList *locals;

// Find a local variable by name.
static Var *find_var(Token *tok) {
    for(VarList *vl = locals; vl; vl = vl->next) {
        Var *var = vl->var;
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
static Token *consume(char *op) {
	if(token->kind != TK_RESERVED ||
		strlen(op) != token->len ||
		memcmp(token->str, op, token->len)) {
		return NULL;
	}
	Token *t = token;
    token = token->next;
	return t;
}

// Return token if the current token matches a given string.
static Token *peek(char *s) {
    if(token->kind != TK_RESERVED || 
        strlen(s) != token->len || 
        strncmp(token->str, s, token->len)) {
        return NULL;
    }
    return token;
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
static void expect(char *s) {
    if(!peek(s)) {
        error_tok(token, "expected \"%s\"", s);
	}
	token = token->next;
}

// 次のトークンが数値の場合, トークンを1つ読み進めてその数値を返す.
// それ以外の場合にはエラーを報告する.
static int expect_number() {
	if(token->kind != TK_NUM) {
        error_tok(token, "expected a number");
	}
	int val = token->val;
	token = token->next;
	return val;
}

// Ensure that the current token is TK_IDENT.
// and return TK_IDENT name.
static char *expect_ident() {
    if(token->kind != TK_IDENT) {
        error_tok(token, "expected an identifier");
    }
    char *s = strndup(token->str, token->len);
    token = token->next;
    return s;
}

static bool at_eof() {
    return token->kind == TK_EOF;
}

// generate new template node
static Node *new_node(NodeKind kind, Token *tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
    Node *node = new_node(kind, tok);
	node->lhs = lhs;
	node->rhs = rhs;
	return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

static Node *new_num(int val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
	node->val = val;
	return node;
}

static Node *new_var_node(Var *var, Token * tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

// assign new variable
static Var *new_lvar(char *name, Type *ty) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
    return var;
}

static Function *function();
static Node *declaration();
static Node *stmt();
static Node *stmt2();
static Node *expr();
static Node *assign();
static Node *equality();
static Node *relational();
static Node *add();
static Node *mul();
static Node *primary();
static Node *unary();
static Node *postfix();

// program = function*
Function *program() {
    Function head = {};
    Function *cur = &head;

    while(!at_eof()) {
        cur->next = function();
        cur = cur->next;
    }
    return head.next;
}

// basetype = "int" "*"*
static Type *basetype() {
    expect("int");
    Type *ty = int_type;
    while(consume("*")) {
        ty = pointer_to(ty);
    }
    return ty;
}

// 型を返す
static Type *read_type_suffix(Type *base) {
    if(!consume("[")) {
        return base;
    }
    int sz = expect_number();
    expect("]");
    base = read_type_suffix(base);
    return array_of(base, sz);
}

static VarList *read_func_param() {
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = new_lvar(name, ty);
    return vl;
}

static VarList *read_func_params() {
    if(consume(")")) {
        return NULL;
    }

    VarList *head = read_func_param();
    VarList *cur = head;

    while(!consume(")")) {
        expect(",");
        cur->next = read_func_param();
        cur = cur->next;
    }

    return head;
}

// function = basetype ident "(" params? ")" "{" stmt* "}"
// params = param ("," param)*
// param = basetype ident
static Function *function() {
    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    basetype();
    fn->name = expect_ident();
    expect("(");
    fn->params = read_func_params();
    expect("{");

    Node head = {};
    Node *cur = &head;
    while(!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }

    fn->node = head.next;
    fn->locals = locals;
    return fn;
}

// declaration = basetype ident ("[" num "]")* ("=" expr )";"
static Node *declaration() {
    Token *tok = token;
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    Var *var = new_lvar(name, ty);

    if(consume(";")) {
        return new_node(ND_NULL, tok);
    }

    expect("=");
    Node *lhs = new_var_node(var, tok);
    Node *rhs = expr();
    expect(";");
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
    return new_unary(ND_EXPR_STMT, node, tok);
}

/*
 expr(式): 必ず1つの値をstackに残す(ex. +, variable)
 stmt(文): stackに何も残さない(rspの位置を変えない) (ex. while, for, if) 
 exprをstmtとして扱うためにexpr_stmtにexprを入れておき, expr_stmtを実行した後には
 stackの中身を一つ取り出すようにしておく.
*/
static Node *read_expr_stmt() {
    Token *tok = token;
    return new_unary(ND_EXPR_STMT, expr(), tok);
}

static Node *stmt() {
    Node *node = stmt2();
    add_type(node);
    return node;
}

// stmt2 = "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | "{"  expr "}"
//      | declaration
//      | expr ";"
static Node *stmt2() {
    Token *tok;
    if(tok = consume("return")) {
        Node *node = new_unary(ND_RETURN, expr(), tok);
        expect(";");
        return node;
    }

    if(tok = consume("if")) {
        Node *node = new_node(ND_IF, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if(consume("else")) {
            node->els = stmt();
        }
        return node;
    }

    if(tok = consume("while")) {
        Node *node = new_node(ND_WHILE, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if(tok = consume("for")) {
        Node *node = new_node(ND_FOR, tok);
        expect("(");
        if(!consume(";")) {
            node->init = read_expr_stmt();
            expect(";");
        }
        if(!consume(";")) {
            node->cond = expr();
            expect(";");
        }
        if(!consume(")")) {
            node->inc = read_expr_stmt();
            expect(")");
        }
        node->then = stmt();
        return node;
    }

	if(tok=consume("{")) {
		Node head = {};
		Node *cur = &head;

		while(!consume("}")) {
			cur->next = stmt();
			cur = cur->next;
		}
		Node *node = new_node(ND_BLOCK, tok);
		node->body = head.next;
		return node;
	}

    if(tok = peek("int")) {
        return declaration();
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
    Token *tok;
    if(tok=consume("=")) {
        node = new_binary(ND_ASSIGN, node, assign(), tok);
    }
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality() {
	Node *node = relational();

    Token *tok;
	while(true) {
		if(tok=consume("==")) {
			node = new_binary(ND_EQ, node, relational(), tok);
		} else if(consume("!=")) {
			node = new_binary(ND_NE, node, relational(), tok);
		} else {
			return node;
		}
	}
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational() {
	Node *node = add();

    Token *tok;
	while(true) {
		if(tok = consume("<")) {
			node = new_binary(ND_LT, node, add(), tok);
		} else if(tok = consume("<=")) {
			node = new_binary(ND_LE, node, add(), tok);
		} else if(tok = consume(">")) {
			node = new_binary(ND_LT, add(), node, tok);
		} else if(tok = consume(">=")) {
			node = new_binary(ND_LE, add(), node, tok);
		} else {
			return node;
		}
	}
}

static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
    add_type(lhs);
    add_type(rhs);

    if(is_integer(lhs->ty) && is_integer(rhs->ty)) {
        return new_binary(ND_ADD, lhs, rhs, tok);
    } 
    if(lhs->ty->base && is_integer(rhs->ty)) {
        return new_binary(ND_PTR_ADD, lhs, rhs, tok);
    } 
    if(is_integer(lhs->ty) && rhs->ty->base) {
        return new_binary(ND_PTR_ADD, rhs, lhs, tok);
    }
    error_tok(tok, "invalid operands");
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
    add_type(lhs);
    add_type(rhs);

    if(is_integer(lhs->ty) && is_integer(rhs->ty)) {
        return new_binary(ND_SUB, lhs, rhs, tok); 
    }
    if(lhs->ty->base && is_integer(rhs->ty)) {
        return new_binary(ND_PTR_SUB, lhs, rhs, tok);
    }
    if(lhs->ty->base && rhs->ty->base) {
        return new_binary(ND_PTR_DIFF, lhs, rhs, tok);
    }
    error_tok(tok, "invalid operands");
}

// add = mul ("+" mul | "-" mul)*
static Node *add() {
	Node *node = mul();
    Token *tok;

	while(true) {
		if(tok = consume("+")) {
            node = new_add(node, mul(), tok);
		} else if(tok = consume("-")) {
            node = new_sub(node, mul(), tok);
		} else {
			return node;
		}
	}
}

// mul = unary("*" unary | "/" unary)*
static Node *mul() {
	Node *node = unary();
    Token *tok;

	while(true) {
		if(tok = consume("*")) {
			node = new_binary(ND_MUL, node, unary(), tok);
		} else if(tok = consume("/")) {
			node = new_binary(ND_DIV, node, unary(), tok);
		} else {
			return node;
		}
	}
}

// unary = ("+" | "-" | "*" | "&")? unary
//          | postfix
static Node *unary() {
    Token * tok;
	if(tok = consume("+")) {
		return unary();
	} else if(tok = consume("-")) {
		return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
	} else if(tok = consume("&")) {
        return new_unary(ND_ADDR, unary(), tok);
    } else if(tok = consume("*")) {
        return new_unary(ND_DEREF, unary(), tok);
    }
	return postfix();
}

// postfix = primary ("[" expr "]")*
static Node *postfix() {
    Node *node = primary();
    Token *tok;

    while(tok=consume("[")) {
        // x[y] is syntax sugar for *(x+y)
        Node *exp = new_add(node, expr(), tok);
        expect("]");
        node = new_unary(ND_DEREF, exp, tok);
    }
    return node;
}

// func-args = "(" (assign (",", assign)*)? ")" 
static Node *func_args() {
    if(consume(")")) {
        return NULL;
    }

    Node *head = assign();
    Node *cur = head;
    while(consume(",")) {
        cur->next = assign();
        cur = cur->next;
    }
    expect(")");
    return head;
}

// primary = "(" expr ")" | ident func-args? | num
static Node *primary() {
	// 次のトークンが"("なら, "(" expr ")"のはず
	if(consume("(")) {
		Node *node = expr();
		expect(")");
		return node;
	}

    Token *tok;
    if(tok=consume_ident()) {
        // Function call
        if(consume("(")) {
            Node *node = new_node(ND_FUNCALL, tok);
            // strndup(str, len): assign copy string of str.slice(0 until len)
            node->funcname = strndup(tok->str, tok->len);
            node->args = func_args();
            return node;
        }

        // Variable
        Var *var = find_var(tok);
        if(!var) {
            error_tok(tok, "undefined variable");
        }
        return new_var_node(var, tok);
    }

    tok = token;
    if(tok->kind != TK_NUM) {
        error_tok(tok, "expected expression");
    }
	return new_num(expect_number(), tok);
}
