#include "chibicc.h"

// Scope for local variables, global variables or typedefs
typedef struct VarScope VarScope;
struct VarScope {
    VarScope *next;
    char *name;
    Var *var;
    Type *type_def;
};

// Scope for struct tags
typedef struct TagScope TagScope;
struct TagScope {
    TagScope *next;
    char *name;
    Type *ty;
};

typedef struct {
    VarScope *var_scope;
    TagScope *tag_scope;
} Scope;

// All local variable instances created during parsing are
// accumulated to this list
static VarList *locals;

// Likewise, global variables are accumulated to this list.
static VarList *globals;

// C has two block scopes; one is for variables/typedefs and 
// the other is for struct tags.
// static VarScope *var_scope; 
static VarScope *var_scope;
static TagScope *tag_scope;

// Begin a block scope
static Scope *enter_scope() {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->var_scope = var_scope;
    sc->tag_scope = tag_scope;
    return sc;
}

// End a block scope
static void leave_scope(Scope *sc) {
    var_scope = sc->var_scope;
    tag_scope = sc->tag_scope;
}

// Find a variable or a typedef by name.
static VarScope *find_var(Token *tok) {
    for(VarScope *sc=var_scope; sc; sc=sc->next) {
        if(strlen(sc->name) == tok->len && !strncmp(tok->str, sc->name, tok->len)) {
            return sc;
        }
    }
    return NULL;
}

static TagScope *find_tag(Token *tok) {
    for(TagScope *sc = tag_scope; sc; sc=sc->next) {
        if(strlen(sc->name) == tok->len && !strncmp(tok->str, sc->name, tok->len)) {
            return sc;
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

static VarScope *push_scope(char *name) {
    VarScope *sc = calloc(1, sizeof(VarScope));
    sc->name = name;
    sc->next = var_scope;
    var_scope = sc;
    return sc;
}

// assign new variable
static Var *new_var(char *name, Type *ty, bool is_local) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;

    return var;
}

static Var *new_lvar(char *name, Type *ty) {
    Var *var = new_var(name, ty, true);
    push_scope(name)->var = var;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
    return var;
}

static Var *new_gvar(char *name, Type *ty, bool emit) {
    Var *var = new_var(name, ty, false);
    push_scope(name)->var = var;

    if(emit) {
        VarList *vl = calloc(1, sizeof(VarList));
        vl->var = var;
        vl->next = globals;
        globals = vl;
    }
    return var;
}

static Type *find_typedef(Token *tok) {
    if(tok->kind == TK_IDENT) {
        VarScope *sc = find_var(tok);
        if(sc) {
            return sc->type_def;
        }
    }
    return NULL;
}

static char *new_label() {
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return strndup(buf, 20);
}

static Function *function();
static Type *basetype();
static Type *declarator(Type *ty, char **name);
static Type *type_suffix(Type *ty);
static Type *struct_decl();
static Member *struct_member();
static void global_var();
static Node *declaration();
static bool is_typename();
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

// Determine whether the next top-level item is a function
// or a global variable by looking ahead input tokens.
static bool is_function() {
    Token *tok = token;

    Type *ty = basetype();
    char *name = NULL;
    declarator(ty, &name);
    bool isfunc = name && consume("(");

    // 読み進めてしまった分を元に戻す
    token = tok;
    return isfunc;
}

// program = (global-var | function)*
Program *program() {
    Function head = {};
    Function *cur = &head;
    globals = NULL;

    while(!at_eof()) {
        if(is_function()) {
            Function *fn = function();
            if(!fn) continue;
            cur->next = fn;
            cur = cur->next;
        } else {
            global_var();
        }
    }
    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns = head.next;
    return prog;
}

// basetype = builtin-type | struct-decl | typedef-name
// builtin-type = "void" | "bool" | "char" | "short" | "int" | "long" 
static Type *basetype() {
    if(!is_typename(token)) {
        error_tok(token, "typename expected");
    }

    if(consume("void")) {
        return void_type;
    }
    if(consume("_Bool")) {
        return bool_type;
    }
    if(consume("char")) {
        return char_type;
    }
    if(consume("short")) {
        return short_type;
    }
    if(consume("int")) {
        return int_type;
    }
    if(consume("long")) {
        consume("long");
        return long_type;
    }
    if(consume("struct")) {
        return struct_decl();
    }
    return find_var(consume_ident())->type_def;
}

// declarator = "*"* ("(" declarator ")" | ident) type-suffix
static Type *declarator(Type *ty, char **name) {
    while(consume("*")) {
        ty = pointer_to(ty);
    }

    if(consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = declarator(placeholder, name);
        expect(")");
        memcpy(placeholder, type_suffix(ty), sizeof(Type));
        return new_ty;
    }

    *name = expect_ident();
    return type_suffix(ty);
}

// 型を返す
// type-suffix = ("[" num "]" type-suffix)?
static Type *type_suffix(Type *ty) {
    if(!consume("[")) {
        return ty;
    }
    int sz = expect_number();
    expect("]");
    ty = type_suffix(ty);
    return array_of(ty, sz);
}

static void push_tag_scope(Token *tok, Type *ty) {
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->next = tag_scope;
    sc->name = strndup(tok->str, tok->len);
    sc->ty = ty;
    tag_scope = sc;
}

// struct-decl = "struct"
//             | "struct" "{" struct-member "}"
static Type *struct_decl() {

    // Read a struct tag.
    Token *tag = consume_ident();
    if(tag && !peek("{")) {
        TagScope *sc = find_tag(tag);
        if(!sc) {
            error_tok(tag, "unknown struct type");
        }
        return sc->ty;
    }
    
    // Read struct members.
    expect("{");

    Member head = {};
    Member *cur = &head;

    while(!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_STRUCT;
    ty->members = head.next;

    // Assign offsets within the struct to member
    int offset = 0;
    for(Member *mem=ty->members; mem; mem = mem->next) {
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += mem->ty->size;

        if(ty->align < mem->ty->align) {
            ty->align = mem->ty->align;
        }
    }
    ty->size = align_to(offset, ty->align);

    // register the struct type if a name was given.
    if(tag) {
        push_tag_scope(tag, ty);
    }

    return ty;
}

// struct-member = basetype declarator type-suffix ";"
static Member *struct_member() {
    Type *ty = basetype();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Member *mem = calloc(1, sizeof(Member));
    mem->name = name;
    mem->ty = ty;
    return mem;
}

static VarList *read_func_param() {
    Type *ty = basetype();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

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

// function = basetype declarator "(" params? ")" ("{" stmt* "}" | ";")
// params = param ("," param)*
// param = basetype declarator type-suffix
static Function *function() {
    locals = NULL;

    Type *ty = basetype();
    char *name = NULL;
    ty = declarator(ty, &name);

    // Add a function type to the scope
    new_gvar(name, func_type(ty), false);

    // Construct a function object
    Function *fn = calloc(1, sizeof(Function));
    fn->name = name;
    expect("(");

    Scope *sc = enter_scope();
    fn->params = read_func_params();

    if(consume(";")) {
        leave_scope(sc);
        return NULL;
    }

    // Read function body
    Node head = {};
    Node *cur = &head;
    expect("{");
    while(!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    leave_scope(sc);

    fn->node = head.next;
    fn->locals = locals;
    return fn;
}

// global-var = basetype declarator type-suffix";"
static void global_var() {
    Type *ty = basetype();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");
    new_gvar(name, ty, true);
}

// declaration = basetype declarator type-suffix ("=" expr ) ";"
//             | basetype ";"
static Node *declaration() {
    Token *tok = token;
    Type *ty = basetype();
    if(consume(";")) {
        return new_node(ND_NULL, tok);
    }

    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

    if(ty->kind == TY_VOID) {
        // In case void *x, its type is TY_PTR, so does not enter this scope
        error_tok(tok, "variable declared void");
    }
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

// Returns true if the next token represents a type.
static bool is_typename() {
    return peek("void") || peek("_Bool") || peek("char") || peek("short") || 
           peek("int") || peek("long") || peek("struct") || find_typedef(token);
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
//      | "{"  stmt "}"
//      | "typedef" basetype declarator type-suffix ";"
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

        Scope *sc = enter_scope();
		while(!consume("}")) {
			cur->next = stmt();
			cur = cur->next;
		}
        leave_scope(sc);

		Node *node = new_node(ND_BLOCK, tok);
		node->body = head.next;
		return node;
	}

    if(tok=consume("typedef")) {
        Type *ty = basetype();
        char *name = NULL;
        ty = declarator(ty, &name);
        ty = type_suffix(ty);
        expect(";");

        push_scope(name)->type_def = ty;
        return new_node(ND_NULL, tok);
    }

    if(is_typename()) {
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

static Member *find_member(Type *ty, char *name) {
    for(Member *mem=ty->members; mem; mem=mem->next) {
        if(!strcmp(mem->name, name)) {
            return mem;
        }
    }
    return NULL;
}

static Node *struct_ref(Node *lhs) {
    add_type(lhs);
    if(lhs->ty->kind != TY_STRUCT) {
        error_tok(lhs->tok, "not a struct");
    }

    Token *tok = token;
    Member *mem = find_member(lhs->ty, expect_ident());
    if(!mem) {
        error_tok(tok, "no such member");
    }

    Node *node = new_unary(ND_MEMBER, lhs, tok);
    node->member = mem;
    return node;
}

// postfix = primary ("[" expr "]" | "." ident | "->" ident)*
static Node *postfix() {
    Node *node = primary();
    Token *tok;

    while(true) {
        if(tok = consume("[")) {
            // x[y] is syntax sugar for *(x+y)
            Node *exp = new_add(node, expr(), tok);
            expect("]");
            node = new_unary(ND_DEREF, exp, tok);
            continue; 
        }

        if(tok = consume(".")) {
            node = struct_ref(node);
            continue;
        }

        if(tok = consume("->")) {
            // x->y is syntax sugar for (*x).y
            node = new_unary(ND_DEREF, node, tok);
            node = struct_ref(node);
            continue;
        }
        return node;
    }
}

// stmt-expr = "(" "{" stmt+ "}" ")"
// Statement expression is a GNU c extension.
static Node *stmt_expr(Token *tok) {
    Scope *sc = enter_scope();
    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = stmt();
    Node *cur = node->body;

    while(!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    expect(")");

    leave_scope(sc);

    if(cur->kind != ND_EXPR_STMT) { // EXPR_STMTは何もstackに値を残さない -> voidだからダメ
        error_tok(cur->tok, "stmt expr returning void is not supported");
    }
    // 最終的に評価されるだろう式の結果をtopノードにして返す
    memcpy(cur, cur->lhs, sizeof(Node));
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

// primary = "(" "{" stmt-expr-tail
//          | "(" expr ")" 
//          | "sizeof" unary 
//          | ident func-args? 
//          | str 
//          | num
static Node *primary() {
    Token *tok;

    if(tok = consume("(")) {
        if(consume("{")) {
            return stmt_expr(tok);
        }
		Node *node = expr();
		expect(")");
		return node;
	}

    if(tok=consume("sizeof")) {
        Node *node = unary();
        add_type(node);
        return new_num(node->ty->size, tok);
    }

    if(tok=consume_ident()) {
        // Function call
        if(consume("(")) {
            Node *node = new_node(ND_FUNCALL, tok);
            // strndup(str, len): assign copy string of str.slice(0 until len)
            node->funcname = strndup(tok->str, tok->len);
            node->args = func_args();
            add_type(node);

            VarScope *sc = find_var(tok);
            if(sc) {
                if(!sc->var || sc->var->ty->kind != TY_FUNC) {
                    error_tok(tok, "not a function");
                }
                node->ty = sc->var->ty->return_ty;
            } else { // まだ宣言されてない関数の宣言は返り値をintと仮定
                warn_tok(node->tok, "implicit declaration of a function");
                node->ty = int_type;
            }

            return node;
        }

        // Variable
        VarScope *sc = find_var(tok);
        if(sc && sc->var) {
            return new_var_node(sc->var, tok);
        }
        error_tok(tok, "undefined variable");
    }

    tok = token;
    if(tok->kind == TK_STR) {
        token = token->next;

        Type *ty = array_of(char_type, tok->cont_len);
        Var *var = new_gvar(new_label(), ty, true);
        var->contents = tok->contents;
        var->cont_len = tok->cont_len;
        return new_var_node(var, tok);
    }

    if(tok->kind != TK_NUM) {
        error_tok(tok, "expected expression");
    }
	return new_num(expect_number(), tok);
}
