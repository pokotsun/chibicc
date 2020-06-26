#include "chibicc.h"

// Scope for local variables, global variables, typedefs
// or enum constants
typedef struct VarScope VarScope;
struct VarScope {
    VarScope *next;
    char *name;
    int depth;

    Var *var;
    Type *type_def;
    Type *enum_ty;
    int enum_val;
};

// Scope for struct or enum tags
typedef struct TagScope TagScope;
struct TagScope {
    TagScope *next;
    char *name;
    int depth;
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
// the other is for struct/union/enum tags.
// static VarScope *var_scope; 
static VarScope *var_scope;
static TagScope *tag_scope;
static int scope_depth;

// Points to a node representing a switch if we are parsing
// a switch statement. Otherwise, NULL.
static Node *current_switch;

// Begin a block scope
static Scope *enter_scope() {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->var_scope = var_scope;
    sc->tag_scope = tag_scope;
    scope_depth++;
    return sc;
}

// End a block scope
static void leave_scope(Scope *sc) {
    var_scope = sc->var_scope;
    tag_scope = sc->tag_scope;
    scope_depth--;
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
    sc->depth = scope_depth;
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

typedef enum {
    TYPEDEF = 1 << 0,
    STATIC = 1 << 1,
} StorageClass;

static Function *function();
static Type *basetype(StorageClass *sclass);
static Type *declarator(Type *ty, char **name);
static Type *abstract_declarator(Type *ty);
static Type *type_suffix(Type *ty);
static Type *type_name();
static Type *struct_decl();
static Type *enum_specifier();
static Member *struct_member();
static void global_var();
static Node *declaration();
static bool is_typename();
static Node *stmt();
static Node *stmt2();
static Node *expr();
static long const_expr();
static Node *assign();
static Node *conditional();
static Node *logor();
static Node *logand();
static Node *bitand();
static Node *bitor();
static Node *bitxor();
static Node *equality();
static Node *relational();
static Node *shift();
static Node *add();
static Node *mul();
static Node *cast();
static Node *primary();
static Node *unary();
static Node *postfix();

// Determine whether the next top-level item is a function
// or a global variable by looking ahead input tokens.
static bool is_function() {
    Token *tok = token;

    StorageClass sclass;
    Type *ty = basetype(&sclass);
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

// basetype = builtin-type | struct-decl | typedef-name | enum-specifier
// builtin-type = "void" | "bool" | "char" | "short" | "int" 
//                       | "long" | "long long"
//
// Note that "typedef" and "static" can appear anywhere in basetype.
// "int" can appear anywhere if type is short, long or long long.
static Type *basetype(StorageClass *sclass) {
    if(!is_typename()) {
        error_tok(token, "typename expected");
    }

    enum {
        VOID = 1 << 0,
        BOOL = 1 << 2,
        CHAR = 1 << 4,
        SHORT = 1 << 6,
        INT = 1 << 8,
        LONG = 1 << 10,
        OTHER = 1 << 12,
    };

    Type *ty = int_type;
    int counter = 0;

    if(sclass) {
        *sclass = 0;
    }

    while(is_typename()) {
        Token *tok = token;

        // Handle storage class specifiers.
        if(peek("typedef") || peek("static")) {
            if(!sclass) {
                error_tok(tok, "storage class specifier is not allowed");
            }

            if(consume("typedef")) {
                *sclass |= TYPEDEF;
            } else if(consume("static")) {
                *sclass |= STATIC;
            }

            if(*sclass & (*sclass - 1)) {
                error_tok(tok, "typedef and static may not be used together");
            }
            continue;
        }

        // Handle user-defined types.
        if(!peek("void") && !peek("_Bool") && !peek("char") &&
           !peek("short") && !peek("int") && !peek("long")) {
            if(counter) break;

            if(peek("struct")) {
                ty = struct_decl();
            } else if(peek("enum")) {
                ty = enum_specifier();
            } else {
                ty = find_typedef(token);
                assert(ty);
                token = token->next;
            }

            counter |= OTHER;
            continue;
        }

        // Handle built-in types.
        if(consume("void")) {
            counter += VOID;
        }
        if(consume("_Bool")) {
            counter += BOOL;
        }
        if(consume("char")) {
            counter += CHAR;
        }
        if(consume("short")) {
            counter += SHORT;
        }
        if(consume("int")) {
            counter += INT;
        }
        if(consume("long")) {
            counter += LONG;
        }

        switch(counter) {
            case VOID:
                ty = void_type;
                break;
            case BOOL:
                ty = bool_type;
                break;
            case CHAR:
                ty = char_type;
                break;
            case SHORT:
            case SHORT + INT:
                ty = short_type;
                break;
            case INT:
                ty = int_type;
                break;
            case LONG:
            case LONG + INT:
            case LONG + LONG:
            case LONG + LONG + INT:
                ty = long_type;
                break;
            default:
                error_tok(tok, "invalid type");
        }
    }

    return ty;
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

// abstract-declarator = "*"* ("(" abstract-declarator ")")? type-suffix
static Type *abstract_declarator(Type *ty) {
    while(consume("*")) {
        ty = pointer_to(ty);
    }

    if(consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = abstract_declarator(placeholder);
        expect(")");
        memcpy(placeholder, type_suffix(ty), sizeof(Type));
        return new_ty;
    }
    return type_suffix(ty);
}

// 型を返す
// type-suffix = ("[" const-expr? "]" type-suffix)?
static Type *type_suffix(Type *ty) {
    if(!consume("[")) {
        return ty;
    }
    int sz = 0;
    bool is_incomplete = true;
    if(!consume("]")) {
        sz = const_expr();
        is_incomplete = false;
        expect("]");
    }

    Token *tok = token;
    ty = type_suffix(ty);
    if(ty->is_incomplete) {
        error_tok(tok, "incomplete element type");
    }

    ty = array_of(ty, sz);
    ty->is_incomplete = is_incomplete;
    return ty;
}

// type-name = basetype abstract-declarator type-suffix
static Type *type_name() {
    Type *ty = basetype(NULL);
    ty = abstract_declarator(ty);
    return type_suffix(ty);
}

static void push_tag_scope(Token *tok, Type *ty) {
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->next = tag_scope;
    sc->name = strndup(tok->str, tok->len);
    sc->depth = scope_depth;
    sc->ty = ty;
    tag_scope = sc;
}

// struct-decl = "struct" ident? ("{" struct-member "}")?
static Type *struct_decl() {
    // Read a struct tag.
    expect("struct");
    Token *tag = consume_ident();
    // 定義済みのstructに対してアクセスする場合
    if(tag && !peek("{")) {
        TagScope *sc = find_tag(tag);
        if(!sc) {
            Type *ty = struct_type();
            push_tag_scope(tag, ty);
            return ty;
        }

        if(sc->ty->kind != TY_STRUCT) {
            error_tok(tag, "not a struct tag");
        }
        return sc->ty;
    }
    
    // Although it looks weird, "struct *foo" is legal C that defines
    // foo as a pointer to an unnamed incomplete struct type.
    if(!consume("{")) {
        return struct_type();
    }

    Type *ty;

    TagScope *sc = NULL;
    if(tag) {
        sc = find_tag(tag);
    }

    // 同じscope内に定義済みのstructが見つかった場合
    if(sc && sc->depth == scope_depth) {
        // If there's an existing struct type having the same tag name in
        // the same block scope, this is a redefinition.
        if(sc->ty->kind != TY_STRUCT) {
            error_tok(tag, "not a struct tag");
        }
        ty = sc->ty;
    } else {
        // Register a struct type as an incomplete type early, so that you
        // can write recursive structs such as
        // "struct T { struct T *next; }"
        ty = struct_type();
        // register the struct type if a name was given.
        if(tag) {
            push_tag_scope(tag, ty);
        }
    }

    Member head = {};
    Member *cur = &head;

    while(!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    ty->members = head.next;

    // Assign offsets within the struct to member
    int offset = 0;
    for(Member *mem=ty->members; mem; mem = mem->next) {
        if(mem->ty->is_incomplete) {
            error_tok(mem->tok, "incomplete struct member");
        }
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += mem->ty->size;

        if(ty->align < mem->ty->align) {
            ty->align = mem->ty->align;
        }
    }
    ty->size = align_to(offset, ty->align);

    ty->is_incomplete = false;

    return ty;
}

// Some types of list can end with an optional "," followed by "}"
// to allow a trailing comma. This function returns true if it looks
// like we are at the end of such list
static bool consume_end() {
    Token *tok = token;
    if(consume("}") || consume(",") && consume("}")) {
        return true;
    }
    token = tok;
    return false;
}

// enum-specifier = "enum" ident
//                | "enum" ident? "{" enum-list? "}"
//
// enum-list = enum-elem ("," enum-elem)* ","?
// enum-elem = ident("=" const-expr)?
static Type *enum_specifier() {
    expect("enum");
    Type *ty = enum_type();

    // Read an enum tag.
    Token *tag = consume_ident();

    // declare enum variable
    if(tag && !peek("{")) {
        TagScope *sc = find_tag(tag);
        if(!sc) {
            error_tok(tag, "unknown enum type");
        }
        if(sc->ty->kind != TY_ENUM) {
            error_tok(tag, "not an enum tag");
        }
        return sc->ty;
    }

    expect("{");

    // Read enum-list
    int cnt = 0;
    while(true) {
        char *name = expect_ident();
        if(consume("=")) {
            cnt = const_expr();
        }

        VarScope *sc = push_scope(name);
        sc->enum_ty = ty;
        sc->enum_val = cnt++;

        if(consume_end()) break;
        expect(",");
    }

    if(tag) {
        push_tag_scope(tag, ty);
    }
    return ty;
}

// struct-member = basetype declarator type-suffix ";"
static Member *struct_member() {
    Type *ty = basetype(NULL);
    Token *tok = token;
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Member *mem = calloc(1, sizeof(Member));
    mem->name = name;
    mem->ty = ty;
    mem->tok = tok;
    return mem;
}

static VarList *read_func_param() {
    Type *ty = basetype(NULL);
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

    // "array of T" is converted to "pointer to T" only in the parameter
    // context. For example, *argv[] is converted to **argv by this.
    if(ty->kind == TY_ARRAY) {
        ty = pointer_to(ty->base);
    }

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

    StorageClass sclass;
    Type *ty = basetype(&sclass);
    char *name = NULL;
    ty = declarator(ty, &name);

    // Add a function type to the scope
    new_gvar(name, func_type(ty), false);

    // Construct a function object
    Function *fn = calloc(1, sizeof(Function));
    fn->name = name;
    fn->is_static = (sclass == STATIC);
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
    StorageClass sclass;
    Type *ty = basetype(&sclass);
    char *name = NULL;
    Token *tok = token;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    if(sclass == TYPEDEF) {
        push_scope(name)->type_def = ty;
    } else {
        if(ty->is_incomplete) {
            error_tok(tok, "incomplete type");
        }
        new_gvar(name, ty, true);
    }
}

// declaration = basetype declarator type-suffix ("=" expr ) ";"
//             | basetype ";"
static Node *declaration() {
    Token *tok = token;
    StorageClass sclass;
    Type *ty = basetype(&sclass);
    if(tok = consume(";")) {
        return new_node(ND_NULL, tok);
    }
    tok = token;

    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

    if(sclass == TYPEDEF) {
        expect(";");
        push_scope(name)->type_def = ty;
        return new_node(ND_NULL, tok);
    }

    if(ty->kind == TY_VOID) {
        // In case void *x, its type is TY_PTR, so does not enter this scope
        error_tok(tok, "variable declared void");
    }
    Var *var = new_lvar(name, ty);
    if(ty->is_incomplete) {
        error_tok(tok, "incomplete type");
    }
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
           peek("int") || peek("long") || peek("enum") || peek("struct") ||
           peek("typedef") || peek("static") || find_typedef(token);
}

static Node *stmt() {
    Node *node = stmt2();
    add_type(node);
    return node;
}

// stmt2 = "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "switch" "(" expr ")" stmt
//      | "case" const-expr ":" stmt
//      | "default" ":" stmt
//      | "while" "(" expr ")" stmt
//      | "for" "(" (expr? ";" | declaration) expr? ";" expr? ")" stmt
//      | "{"  stmt "}"
//      | "break" ";"
//      | "continue" ";"
//      | "goto" ident ";"
//      | ident ":" stmt
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

    if(tok = consume("switch")) {
        Node *node = new_node(ND_SWITCH, tok);
        expect("(");
        node->cond = expr();
        expect(")");

        Node *sw = current_switch;
        current_switch = node;
        node->then = stmt(); // { case ... }
        current_switch = sw;
        return node;
    }
    if(tok = consume("case")) {
        if(!current_switch) {
            error_tok(tok, "stray case");
        }
        int val = const_expr();
        expect(":");

        Node *node = new_unary(ND_CASE, stmt(), tok);
        node->val = val;
        // current_switchの case_nextにcase nodeを数珠つなぎしていく
        node->case_next = current_switch->case_next;
        current_switch->case_next = node;
        return node;
    }

    if(tok = consume("default")) {
        if(!current_switch) {
            error_tok(tok, "stray default");
        }
        expect(":");

        Node *node = new_unary(ND_CASE, stmt(), tok);
        current_switch->default_case = node;
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
        Scope *sc = enter_scope();

        if(!consume(";")) {
            if(is_typename()) {
                node->init = declaration();
            } else {
                node->init = read_expr_stmt();
                expect(";");
            }
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

        leave_scope(sc);
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

    if(tok = consume("break")) {
        expect(";");
        return new_node(ND_BREAK, tok);
    }

    if(tok = consume("continue")) {
        expect(";");
        return new_node(ND_CONTINUE, tok);
    }

    if(tok = consume("goto")) {
        Node *node = new_node(ND_GOTO, tok);
        node->label_name = expect_ident();
        expect(";");
        return node;
    }

    // label
    if(tok = consume_ident()) {
        if(consume(":")) {
            Node *node = new_unary(ND_LABEL, stmt(), tok);
            node->label_name = strndup(tok->str, tok->len);
            return node;
        }
        token = tok;
    }

    if(is_typename()) {
        return declaration();
    }

    Node *node = read_expr_stmt();
    expect(";");
    return node;
}

// expr = assign ("," assign)*
static Node *expr() {
    Node *node = assign();
    Token *tok;
    while(tok = consume(",")) {
        // ここでND_EXPR_STMTに入れておかないと必要以上にスタックに値が残ってしまう
        node = new_unary(ND_EXPR_STMT, node, node->tok); 
        node = new_binary(ND_COMMA, node, assign(), tok);
    }
    return node;
}

// Evaluate a given node as a constant expression.
static long eval(Node *node) {
    switch(node->kind) {
        case ND_ADD:
            return eval(node->lhs) + eval(node->rhs);
        case ND_SUB:
            return eval(node->lhs) - eval(node->rhs);
        case ND_MUL:
            return eval(node->lhs) * eval(node->rhs);
        case ND_DIV:
            return eval(node->lhs) / eval(node->rhs);
        case ND_BITAND:
            return eval(node->lhs) & eval(node->rhs);
        case ND_BITOR:
            return eval(node->lhs) | eval(node->rhs);
        case ND_BITXOR:
            return eval(node->lhs) ^ eval(node->rhs);
        case ND_SHL:
            return eval(node->lhs) << eval(node->rhs);
        case ND_SHR:
            return eval(node->lhs) >> eval(node->rhs);
        case ND_EQ:
            return eval(node->lhs) == eval(node->rhs);
        case ND_NE:
            return eval(node->lhs) != eval(node->rhs);
        case ND_LT:
            return eval(node->lhs) < eval(node->rhs);
        case ND_LE:
            return eval(node->lhs) <= eval(node->rhs);
        case ND_TERNARY:
            return eval(node->cond) ? eval(node->then) : eval(node->els);
        case ND_COMMA:
            return eval(node->rhs);
        case ND_NOT:
            return !eval(node->lhs);
        case ND_BITNOT:
            return ~eval(node->lhs);
        case ND_LOGAND:
            return eval(node->lhs) && eval(node->rhs);
        case ND_LOGOR:
            return eval(node->lhs) || eval(node->rhs);
        case ND_NUM:
            return node->val;
    }

    error_tok(node->tok, "not a constant expression");
}

static long const_expr() {
    return eval(conditional());
}

// assign = conditional (assign-op assign)?
// assign-op = "=" | "+=" | "-=" | "*=" | "/=" | "<<=" | ">>="
static Node *assign() {
    Node *node = conditional();
    Token *tok;

    if(tok=consume("=")) {
        return new_binary(ND_ASSIGN, node, assign(), tok);
    }
    if(tok = consume("*=")) {
        return new_binary(ND_MUL_EQ, node, assign(), tok);
    }
    if(tok = consume("/=")) {
        return new_binary(ND_DIV_EQ, node, assign(), tok);
    }
    if(tok = consume("<<=")) {
        return new_binary(ND_SHL_EQ, node, assign(), tok);
    }
    if(tok = consume(">>=")) {
        return new_binary(ND_SHR_EQ, node, assign(), tok);
    }
    if(tok = consume("+=")) {
        add_type(node);
        if(node->ty->base) {
            return new_binary(ND_PTR_ADD_EQ, node, assign(), tok);
        } else {
            return new_binary(ND_ADD_EQ, node, assign(), tok);
        }
    }

    if(tok = consume("-=")) {
        add_type(node);
        if(node->ty->base) {
            return new_binary(ND_PTR_SUB_EQ, node, assign(), tok);
        } else {
            return new_binary(ND_SUB_EQ, node, assign(), tok);
        }
    }
    return node;
}

// conditional = logor ("?" expr ":" conditional)?
static Node *conditional() {
    Node *node = logor();
    Token *tok = consume("?");
    if(!tok) return node;

    Node *ternary = new_node(ND_TERNARY, tok);
    ternary->cond = node;
    ternary->then = expr();
    expect(":");
    ternary->els = conditional();
    return ternary;
}

// logor = logand ("||" logand)*
static Node *logor() {
    Node *node = logand();
    Token *tok;
    while(tok = consume("||")) {
        node = new_binary(ND_LOGOR, node, logand(), tok);
    }
    return node;
}

// logand = bitor ("&&" bitor)*
static Node *logand() {
    Node *node = bitor();
    Token *tok;
    while(tok = consume("&&")) {
        node = new_binary(ND_LOGAND, node, bitor(), tok);
    }
    return node;
}

// bitor = bitxor("|" bitxor)*
static Node *bitor() {
    Node *node = bitxor();
    Token *tok;
    while(tok = consume("|")) {
        node = new_binary(ND_BITOR, node, bitxor(), tok);
    }
    return node;
}

// bit xor = bitand("^ bitand")*
static Node *bitxor() {
    Node *node = bitand();
    Token *tok;
    while(tok = consume("^")) {
        node = new_binary(ND_BITXOR, node, bitand(), tok);
    }
    return node;
}

// bitand = equality ("&" equality)*
static Node *bitand() {
    Node *node = equality();
    Token *tok;
    while(tok = consume("&")) {
        node = new_binary(ND_BITAND, node, equality(), tok);
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

// relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
static Node *relational() {
	Node *node = shift();

    Token *tok;
	while(true) {
		if(tok = consume("<")) {
			node = new_binary(ND_LT, node, shift(), tok);
		} else if(tok = consume("<=")) {
			node = new_binary(ND_LE, node, shift(), tok);
		} else if(tok = consume(">")) {
			node = new_binary(ND_LT, shift(), node, tok);
		} else if(tok = consume(">=")) {
			node = new_binary(ND_LE, shift(), node, tok);
		} else {
			return node;
		}
	}
}

// shift = add("<<" add | ">>" add)
static Node *shift() {
    Node *node = add();
    Token *tok;
    
    while(true) {
        if(tok = consume("<<")) {
            node = new_binary(ND_SHL, node, add(), tok);
        } else if(tok = consume(">>")) {
            node = new_binary(ND_SHR, node, add(), tok);
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

// mul = cast ("*" cast | "/" cast)*
static Node *mul() {
	Node *node = cast();
    Token *tok;

	while(true) {
		if(tok = consume("*")) {
			node = new_binary(ND_MUL, node, cast(), tok);
		} else if(tok = consume("/")) {
			node = new_binary(ND_DIV, node, cast(), tok);
		} else {
			return node;
		}
	}
}

// cast = "(" type-name ")" cast | unary
static Node *cast() {
    Token *tok = token;

    if(consume("(")) {
        if(is_typename()) {
            Type *ty = type_name();
            expect(")");
            Node *node = new_unary(ND_CAST, cast(), tok);
            add_type(node->lhs);
            node->ty = ty;
            return node;
        }
        token = tok;
    }

    return unary();
}

// unary = ("+" | "-" | "*" | "&" | "!" | "~")? cast
//          | ("++" | "--") unary
//          | postfix
static Node *unary() {
    Token * tok;
	if(tok = consume("+")) {
		return cast();
	} 
    if(tok = consume("-")) {
		return new_binary(ND_SUB, new_num(0, tok), cast(), tok);
	} 
    if(tok = consume("&")) {
        return new_unary(ND_ADDR, cast(), tok);
    } 
    if(tok = consume("*")) {
        return new_unary(ND_DEREF, cast(), tok);
    } 
    if(tok = consume("!")) {
        return new_unary(ND_NOT, cast(), tok);
    }
    if(tok = consume("~")) {
        return new_unary(ND_BITNOT, cast(), tok);
    }
    if(tok = consume("++")) {
        return new_unary(ND_PRE_INC, unary(), tok);
    } 
    if(tok = consume("--")) {
        return new_unary(ND_PRE_DEC, unary(), tok);
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

// postfix = primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
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

        if(tok = consume("++")) {
            node = new_unary(ND_POST_INC, node, tok);
            continue;
        }

        if(tok = consume("--")) {
            node = new_unary(ND_POST_DEC, node, tok);
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
//          | "sizeof" "(" type-name ")"
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
        if(consume("(")) {
            if(is_typename()) {
                Type *ty = type_name();
                if(ty->is_incomplete) {
                    error_tok(tok, "incomplete type");
                }
                expect(")");
                return new_num(ty->size, tok);
            }
            token = tok->next;
        }

        Node *node = unary();
        add_type(node);
        if(node->ty->is_incomplete) {
            error_tok(node->tok, "incomplete type");
        }
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

        // Variable or enum constant
        VarScope *sc = find_var(tok);
        if(sc) {
            if(sc->var) {
                return new_var_node(sc->var, tok);
            }
            if(sc->enum_ty) {
                return new_num(sc->enum_val, tok);
            }
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
