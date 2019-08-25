#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>

/* Possible Moth value types */
enum { MOTHVAL_NUM, MOTHVAL_ERR };

/* Possible Moth errors */
enum { MOTHERR_DIV_ZERO, MOTHERR_BAD_OP, MOTHERR_BAD_NUM };

typedef struct {
    int type;
    long num;
    int err;
} mothval;

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
mothval mothval_num(long x) {
    mothval v;
    v.type = MOTHVAL_NUM;
    v.num = x;
    return v;
}

/* Create a new error type motherr */
mothval mothval_err(int x) {
    mothval v;
    v.type = MOTHVAL_ERR;
    v.err = x;
    return v;
}

void mothval_print(mothval v) {
    switch (v.type) {
    case MOTHVAL_NUM: printf("%li", v.num); break;
    case MOTHVAL_ERR:
        if (v.err == MOTHERR_DIV_ZERO) {
            printf("Error: Divsion by zero!");
        } else if (v.err == MOTHERR_BAD_OP) {
            printf("Error: Invalid operator!");
        } else if (v.err == MOTHERR_BAD_NUM) {
            printf("Error: Invalid number!");
        }
        break;
    }
}

void mothval_println(mothval v) { mothval_print(v); putchar('\n'); }

mothval eval_op(mothval x, char *op, mothval y) {
    if (x.type == MOTHVAL_ERR) { return x; }
    if (y.type == MOTHVAL_ERR) { return y; }

    if (strcmp(op, "+") == 0) { return mothval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return mothval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return mothval_num(x.num * y.num); }
    if (strcmp(op, "/") == 0) {
        return y.num == 0
            ? mothval_err(MOTHERR_DIV_ZERO)
            : mothval_num(x.num / y.num);
    }
    return mothval_err(MOTHERR_BAD_OP);
}

mothval eval(mpc_ast_t *t)
{
    if (strstr(t->tag, "number")) {
        /* Check if the conversion fails*/
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? mothval_num(x) : mothval_err(MOTHERR_BAD_NUM);
    }

    /* When an expression is not a number, the operator is always
       the second child. The first child is always '(', as defined
       in our grammar for expressions:
       expr     : <number> | '(' <operator> <expr>+ ')'; */
    char *op = t->children[1]->contents;

    /* The third child */
    mothval x = eval(t->children[2]);

    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char *argv[])
{
    /* Create parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Moth = mpc_new("moth");

    /* Define language using parsers */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                    \
               number   : /-?[0-9]+/ ;                             \
               operator : '+' | '-' | '*' | '/' ;                  \
               expr     : <number> | '(' <operator> <expr>+ ')' ;  \
               moth     : /^/ <operator> <expr>+ /$/ ;             \
              ",
              Number, Operator, Expr, Moth);

    puts("Moth v0.1\n");
    puts("Press Ctrl-C to exit\n");

    while (1) {
        char *input = readline("moth> ");
        add_history(input);

        /* Parse user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Moth, &r)) {
            // mpc_ast_print(r.output);
            mothval result = eval(r.output);
            mothval_println(result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }

    /* Undefine and delete parsers */
    mpc_cleanup(4, Number, Operator, Expr, Moth);

    return 0;
}
