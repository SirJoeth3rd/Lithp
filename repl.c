#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "mpc.h"

//Now for a lesson on portability in C
//Will probably remove this later.
#ifdef _WIN32
#include <string.h>
/* Our own readline */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* You dont get history for using windows */
void add_history(char* unused) {}
#else
// Use this on all other systems not windows
#include <editline/readline.h>
#endif

//Our atom , in lithp errors are just possible values
typedef struct {
    int type;
    long num;
    int err;
} atom;

//Possible values for types
enum { ATOM_NUM, ATOM_ERR };

//Possible errors
enum {ERR_DIV_ZERO, ERR_BAD_OP, ERR_BAD_NUM};
const char* ERR_MSGS[] = {
    "Error: Division by zero",
    "Error: Invalid operator",
    "Error: Invalid Number",
};

//functions declaraions
atom eval_op(atom, char*, atom);
atom eval(mpc_ast_t*);
atom atom_num(long);
atom atom_err(int);
void atom_println(atom);
void atom_print(atom);

//HERE WE GO
int main(int argc, char** argv) {

    /* Parser objects created with mpc library*/
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lispy    = mpc_new("lispy");

    /* Our language definition */
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
        number   : /-?[0-9]+/ ;                             \
        operator : '+' | '-' | '*' | '/' ;                  \
        expr     : <number> | '(' <operator> <expr>+ ')' ;  \
        lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Operator, Expr, Lispy);


    puts("Lithp version 1.0");
    puts("Press Ctrl-C to exit");

    bool cont_loop = true;

    while (cont_loop) {

        //Get the user input
        char* input = readline("lithp>");
        add_history(input);

        //Parse the input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* Evaluate the AST */
            atom result = eval(r.output);
            atom_println(result);
            mpc_ast_delete(r.output);
        } else {
            /* Print the error tree */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    //terminating the parser
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}

//the evaluation function takes the tree and recursively evaluates it
atom eval(mpc_ast_t* t) {
    //base case
    if (strstr(t->tag,"number")) {
        errno = 0;
        long x = strtol(t->contents,NULL,10);
        return errno != ERANGE ? atom_num(x) : atom_err(ERR_BAD_NUM);
    }

    char *op = t->children[1]->contents;
    atom x = eval(t->children[2]);

    int i = 3;
    while(strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

//evaluates two atoms and a operator
atom eval_op(atom x, char* op, atom y) {
    if (x.type == ATOM_ERR) {return x;}
    if (y.type == ATOM_ERR) {return y;}

    if (strcmp(op, "+") == 0) { return atom_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return atom_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return atom_num(x.num * y.num); }
    if (strcmp(op, "/") == 0) {
        return (y.num == 0) ? atom_err(ERR_DIV_ZERO) : atom_num(x.num/y.num);
    }

    return atom_err(ERR_BAD_OP);
}

//creation of atoms
atom atom_num(long x){
    atom v;
    v.type = ATOM_NUM;
    v.num = x;
    return v;
}

atom atom_err(int x){
    atom v;
    v.type = ATOM_ERR;
    v.err = x;
    return v;
}

//printing a atom
void atom_print(atom v) {
    switch (v.type) {
        case ATOM_NUM:
            printf("%li",v.num);
            break;
        case ATOM_ERR:
            printf("%s",ERR_MSGS[v.err]);
            break;
    }
}

void atom_println(atom v) {
    atom_print(v);
    putchar('\n');
}
