#include "mpc.h"

#include <stdio.h>
#include <stdlib.h>

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
            mpc_ast_print(r.output);
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
