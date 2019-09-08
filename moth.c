#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>

/* Possible Moth value types */
enum { MOTHVAL_NUM, MOTHVAL_ERR, MOTHVAL_SYM, MOTHVAL_SEXPR };

struct mothval {
    int type;
    long num;
    /* Error and symbol types have string data */
    char *err;
    char *sym;
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

void mothval_del(mothval *v)
{
    switch (v->type) {
    case MOTHVAL_NUM: break;

    /* Free string data */
    case MOTHVAL_ERR: free(v->err); break;
    case MOTHVAL_SYM: free(v->sym); break;

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
    if (strstr(t->tag, "sexpr")) {x = mothval_sexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
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
    case MOTHVAL_SEXPR: mothval_expr_print(v, '(', ')'); break;
    }
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

//mothval eval_op(mothval x, char *op, mothval y) {
//    if (x.type == MOTHVAL_ERR) { return x; }
//    if (y.type == MOTHVAL_ERR) { return y; }
//
//    if (strcmp(op, "+") == 0) { return mothval_num(x.num + y.num); }
//    if (strcmp(op, "-") == 0) { return mothval_num(x.num - y.num); }
//    if (strcmp(op, "*") == 0) { return mothval_num(x.num * y.num); }
//    if (strcmp(op, "/") == 0) {
//        return y.num == 0
//            ? mothval_err(MOTHERR_DIV_ZERO)
//            : mothval_num(x.num / y.num);
//    }
//    return mothval_err(MOTHERR_BAD_OP);
//}

//mothval eval(mpc_ast_t *t)
//{
//    if (strstr(t->tag, "number")) {
//        /* Check if the conversion fails*/
//        errno = 0;
//        long x = strtol(t->contents, NULL, 10);
//        return errno != ERANGE ? mothval_num(x) : mothval_err(MOTHERR_BAD_NUM);
//    }
//
//    /* When an expression is not a number, the operator is always
//       the second child. The first child is always '(', as defined
//       in our grammar for expressions:
//       expr     : <number> | '(' <operator> <expr>+ ')'; */
//    char *op = t->children[1]->contents;
//
//    /* The third child */
//    mothval x = eval(t->children[2]);
//
//    int i = 3;
//    while (strstr(t->children[i]->tag, "expr")) {
//        x = eval_op(x, op, eval(t->children[i]));
//        i++;
//    }
//
//    return x;
//}


mothval *mothval_eval(mothval *v);

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
    mothval *result = builtin_op(v, f->sym);
    mothval_del(f);
    return result;
}

mothval *mothval_eval(mothval *v) {
    /* Evaluate symbolic expressions */
    if (v->type == MOTHVAL_SEXPR) { return mothval_eval_sexpr(v); }

    /* All other mothval types remain the same */
    return v;
}

int main(int argc, char *argv[])
{
    /* Create parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Moth = mpc_new("moth");

    /* Define language using parsers */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                    \
               number   : /-?[0-9]+/ ;                             \
               symbol   : '+' | '-' | '*' | '/' ;                  \
               sexpr    : '(' <expr>* ')' ;                        \
               expr     : <number> | <symbol> | <sexpr> ;          \
               moth     : /^/ <expr>* /$/ ;                        \
              ",
              Number, Symbol, Sexpr, Expr, Moth);

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
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Moth);

    return 0;
}
