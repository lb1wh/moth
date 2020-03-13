#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>

struct mval;
struct menv;
typedef struct mval mval;
typedef struct menv menv;

struct menv {
    int count;
    char **syms;
    mothval **vals;
};

/* Possible Moth value types */
enum { MOTHVAL_NUM, MOTHVAL_ERR, MOTHVAL_SYM, MOTHVAL_SEXPR,
       MOTHVAL_QEXPR, MOTHVAL_FUN };

typedef mval* (*mbuiltin)(menv*, mval*);

struct mothval {
    int type;
    long num;
    /* Error and symbol types have string data */
    char *err;
    char *sym;
    mbuiltin fun;

    /* Count and pointer to a list of "mothval*" */
    int count;
    struct mothval **cell;
};

typedef struct mothval mothval;

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline */
char *readline(char *prompt)
{
    fputs(prompt, stout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Create a new number type mothval */
mothval *mothval_num(long x) {
    mothval *v = malloc(sizeof(mothval));
    v->type = MOTHVAL_NUM;
    v->num = x;
    return v;
}

/* Create a new error type motherr */
mothval *mothval_err(char *m) {
    mothval *v = malloc(sizeof(mothval));
    v->type = MOTHVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

mothval *mothval_sym(char *s)
{
    mothval *v = malloc(sizeof(mothval));
    v->type = MOTHVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

mothval *mothval_sexpr(void)
{
    mothval *v = malloc(sizeof(mothval));
    v->type = MOTHVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

mothval *mothval_qexpr(void)
{
    mothval *v = malloc(sizeof(mothval));
    v->type = MOTHVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

mothval *mothval_fun(lbuiltin func)
{
    mothval *v = malloc(sizeof(mval));
    v->type = MOTHVAL_FUN;
    v->fun = func;
    return v;
}

menv *menv_new(void)
{
    menv *e = malloc(sizeof(menv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void menv_del(menv *e)
{
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        mval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

mval *menv_get(menv *e, mval *k)
{
    /* Iterate over all the values in the environment */
    for (int i = 0; i < e->count; i++) {
        /* If the stored string matches the symbol string,
           return a copy of its value */
        if (strcmp(e->syms[i], k->sym) == 0) {
            return mval_copy(e->vals[i]);
        }
    }
    /* No symbol found */
    return mval_err("unbound symbol!");
}

void menv_put(menv* e, mval *k, mval *v)
{
    /* Iterate over all the elements in the environment
       to check if the variable already exists */
    for (int i = 0; i < e->count; i++) {
        /* If the variable is found, delete the item at
           that position and replace it with the variable
           supplies by the user */
        if (strcmp(e->syms[i], k->sym) == 0) {
            mval_del(e->vals[i]);
            e->vals[i] = mval_copy(v);
            return;
        }
    }

    /* If there's no existing entry, allocate space for the new one */
    e->count++;
    e->vals = realloc(e->vals, sizeof(mval *) * e->count);
    e->syms = realloc(e->syms, sizeof(char *) * e->count);

    /* Copy the contents of mval and symbol string into new location */
    e->vals[e->count - 1] = mval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

mval *mval_eval(menv *e, mval* v)
{
    if (v->type == MVAL_SYM) {
        mval *x = menv_get(e, v);
        mval_del(v);
        return x;
    }

    if (v->type == MVAL_SEXPR) { return mval_eval_sexpr(e, v); }
    return v;
}

mval *mval_eval_sexpr(menv *e, mval *e)
{
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = mval_eval(e, v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == MVAL_ERR) { return mval_take(v, i); }
    }

    if (v->count == 0) { return v; }
    if (v->count == 1) { return mval_take(v, 0); }

    /* Ensure that the first element is a function, after eval */
    mval *f = mval_pop(v, 0);
    if (f->type != MVAL_FUN) {
        mval_del(v); mval_del(f);
        return mval_err("The first element is not a function");
    }

    /* Call function to get result */
    mval *result = f->fun(e, v);
    mval_del(f);
    return result;
}

void mothval_del(mothval *v)
{
    switch (v->type) {
    case MOTHVAL_NUM: break;

    /* Free string data */
    case MOTHVAL_ERR: free(v->err); break;
    case MOTHVAL_SYM: free(v->sym); break;

    case MOTHVAL_FUN: break;

    /* If Qexpr or Sexpr, delete all elements inside */
    case MOTHVAL_QEXPR:
    case MOTHVAL_SEXPR:
        for (int i = 0; i < v->count; i++) {
            mothval_del(v->cell[i]);
        }
        /* Free the memory allocated to contain the pointers */
        free(v->cell);
        break;
    }

    free(v);
}

mothval *mothval_read_num(mpc_ast_t *t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? mothval_num(x) : mothval_err("Invalid number");
}

mothval *mothval_add(mothval *v, mothval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(mothval *) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

mothval *mothval_read(mpc_ast_t *t)
{
    /* If we read a symbol or number, return the conversion to that type */
    if (strstr(t->tag, "number")) { return mothval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return mothval_sym(t->contents); }

    /* If root or sexpr, then create an empty list */
    mothval *x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = mothval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = mothval_sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = mothval_qexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = mothval_add(x, mothval_read(t->children[i]));
    }

    return x;
}

/* Forward declare */
void mothval_print(mothval *v);

void mothval_expr_print(mothval *v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        mothval_print(v->cell[i]);

        /* Don't print trailing space if it's the last element */
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

mothval *mothval_pop(mothval *v, int i)
{
    /* Find the item at "i" */
    mothval *x = v->cell[i];

    /* Shift memory after the item at "i" over the top */
    memmove(&v->cell[i], &v->cell[i + 1],
            sizeof(mothval *) * (v->count - i - 1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(mothval *) * v->count);
    return x;
}

mothval *mothval_take(mothval *v, int i)
{
    mothval *x = mothval_pop(v, i);
    mothval_del(v);
    return x;
}

void mothval_print(mothval *v)
{
    switch (v->type) {
    case MOTHVAL_NUM:   printf("%li", v->num); break;
    case MOTHVAL_ERR:   printf("Error: %s", v->err); break;
    case MOTHVAL_SYM:   printf("%s", v->sym); break;
    case MOTHVAL_FUN:   printf("<function>"); break;
    case MOTHVAL_SEXPR: mothval_expr_print(v, '(', ')'); break;
    case MOTHVAL_QEXPR: mothval_expr_print(v, '{', '}'); break;
    }
}

mothval *mothval_copy(mothval *v)
{
    mothval *x = malloc(sizeof(mval));
    x->type = v->type;

    switch (v->type) {
    /* Copy functions and numbers directly */
    case MOTHVAL_FUN: x->fun = v->fun; break;
    case MOTHVAL_NUM: x->num = v->num; break;

    /* Copy strings */
    case MOTHVAL_ERR:
        x->err = malloc(strlen(v->err) + 1);
        strcpy(x->err, v->err); break;

    case MOTHVAL_SYM:
        x->sym = malloc(strlen(v->sym) + 1);
        strcpy(x->sym, v->sym); break;

    /* Copy lists by copying each sub-expression */
    case MOTHVAL_SEXPR:
    case MOTHVAL_QEXPR:
        x->count = v->count;
        x->cell = malloc(sizeof(mothval *) * x->count);
        for (int i = 0; i < x->count; i++) {
            x->cell[i] = mothval_copy(v->cell[i]);
        }
        break;
    }

    return x;
}

void mothval_println(mothval *v) { mothval_print(v); putchar('\n'); }

mothval *builtin_op(mothval *a, char *op)
{
    /* Ensure that all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != MOTHVAL_NUM) {
            mothval_del(a);
            return mothval_err("Can't operate on non-number!");
        }
    }

    /* Pop the first element */
    mothval *x = mothval_pop(a, 0);

    /* If there are no arguments and a subtraction, perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while (a->count > 0) {
        /* Pop the next element */
        mothval *y = mothval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                mothval_del(x); mothval_del(y);
                x = mothval_err("Division by zero!"); break;
            }
            x->num /= y->num;
        }
        mothval_del(y);
    }
    mothval_del(a);
    return x;
}

mothval *builtin_add(menv *e, mval *a)
{
    return builtin_op(e, a, "+");
}

mothval *builtin_sub(menv *e, mval *a)
{
    return builtin_op(e, a, "-");
}

mothval *builtin_mul(menv *e, mval *a)
{
    return builtin_op(e, a, "*");
}

mothval *builtin_div(menv *e, mval *a)
{
    return builtin_op(e, a, "/");
}

mothval *mothval_eval(mothval *v);
mothval *builtin(mothval *a, char *func);

mothval* mothval_eval_sexpr(mothval *v)
{
    /* Evaluate children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = mothval_eval(v->cell[i]);
    }

    /* Error checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == MOTHVAL_ERR) { return mothval_take(v, i); }
    }

    /* Empty expression */
    if (v->count == 0) { return v; }

    /* Single expression */
    if (v->count == 1) { return mothval_take(v, 0); }

    /* Ensure that the first element is a symbol */
    mothval *f = mothval_pop(v, 0);
    if (f->type != MOTHVAL_SYM) {
        mothval_del(f); mothval_del(v);
        return mothval_err("Symbolic expression does not start with a symbol!");
    }

    /* Call builtin with operator */
    mothval *result = builtin(v, f->sym);
    mothval_del(f);
    return result;
}

mothval *mothval_eval(mothval *v) {
    /* Evaluate symbolic expressions */
    if (v->type == MOTHVAL_SEXPR) { return mothval_eval_sexpr(v); }

    /* All other mothval types remain the same */
    return v;
}

#define LASSERT(args, cond, err) \
    if (!(cond)) { mothval_del(args); return mothval_err(err); }

mothval* builtin_head(mothval *a)
{
    LASSERT(a, a->count == 1,
            "The function 'head' passed too many arguments!");

    LASSERT(a, a->cell[0]->type == MOTHVAL_QEXPR, // NOT-EQUALS?
            "Function 'head' passed incorrect types!");

    LASSERT(a, a->cell[0]->count != 0,
            "Function 'head' passed {}!");

    /* Otherwise, take first argument */
    mothval *v = mothval_take(a, 0);

    while (v->count > 1) { mothval_del(mothval_pop(v, 1)); }
    return v;
}

mothval *builtin_tail(mothval *a)
{
    LASSERT(a, a->count == 1,
            "Function 'tail' passed too many arguments! ");

    LASSERT(a, a->cell[0]->type == MOTHVAL_QEXPR,
            "Function 'tail' passed incorrect type!");

    LASSERT(a, a->cell[0]->count != 0,
            "Function 'tail' passed {}!");

    /* Take first arguments */
    mothval *v = mothval_take(a, 0);

    /* Delete the first element and return */
    mothval_del(mothval_pop(v, 0));
    return v;
}

mothval *builtin_list(mothval *a)
{
    a->type = MOTHVAL_QEXPR;
    return a;
}

mothval *builtin_eval(mothval *a)
{
    LASSERT(a, a->count == 1,
            "Function 'eval' passed too many arguments!");

    LASSERT(a, a->cell[0]->type == MOTHVAL_QEXPR,
            "Function 'eval' passed incorrect type!");

    mothval *x = mothval_take(a, 0);
    x->type = MOTHVAL_SEXPR;
    return mothval_eval(x);
}

mothval *mothval_join(mothval *x, mothval *y)
{
    /* For each cell in 'y', add it to 'x' */
    while (y->count) {
        x = mothval_add(x, mothval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x' */
    mothval_del(y);
    return x;
}

mothval *builtin_join(mothval *a)
{
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == MOTHVAL_QEXPR,
                "Function 'join' passed incorrect type!");
    }

    mothval *x = mothval_pop(a, 0);

    while (a->count) {
        x = mothval_join(x, mothval_pop(a, 0));
    }

    mothval_del(a);
    return x;
}

mothval *builtin(mothval *a, char *func)
{
    if (strcmp("list", func) == 0) { return builtin_list(a); }
    if (strcmp("head", func) == 0) { return builtin_head(a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(a); }
    if (strcmp("join", func) == 0) { return builtin_join(a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(a); }
    if (strstr("+-/*", func)) { return builtin_op(a, func); }
    mothval_del(a);
    return mothval_err("Unknown function!");
}

int main(int argc, char *argv[])
{
    /* Create parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Qexpr = mpc_new("qexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Moth = mpc_new("moth");

    /* Define language using parsers */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                         \
               number   : /-?[0-9]+/ ;                                  \
               symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                                       \
               sexpr    : '(' <expr>* ')' ;                             \
               qexpr    : '{' <expr>* '}' ;                             \
               expr     : <number> | <symbol> | <sexpr> | <qexpr> ;     \
               moth     : /^/ <expr>* /$/ ;                             \
              ",
              Number, Symbol, Sexpr, Qexpr, Expr, Moth);

    puts("Moth v0.1\n");
    puts("Press Ctrl-C to exit\n");

    while (1) {
        char *input = readline("moth> ");
        add_history(input);

        /* Parse user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Moth, &r)) {
            mothval *x = mothval_eval(mothval_read(r.output));
            mothval_println(x);
            mothval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }

    /* Undefine and delete parsers */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Moth);

    return 0;
}
