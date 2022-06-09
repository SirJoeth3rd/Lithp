#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "mpc.h"
#include <editline/readline.h>

//Our atom , in lithp errors are just possible values
typedef struct atom {
    int type;
    long num;
    //symbol and err strings
    char *err;
    char *sym;
    //the list in lisp
    int count;
    struct atom** cell;
} atom;

//Possible values for types
enum { ATOM_NUM, ATOM_ERR, ATOM_SYM, ATOM_SEXPR};

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
atom* atom_eval(atom*);
atom* builtin_op(atom*,char*);
atom* atom_pop(atom*, int);
atom* atom_take(atom*,int);
atom* atom_num(long);
atom* atom_err(char*);
atom* atom_sym(char*);
atom* atom_sexpr();
void atom_del(atom*);
atom* atom_read(mpc_ast_t*);
atom* atom_add(atom*,atom*);
void atom_expr_print(atom*,char,char);
void atom_println(atom*);
void atom_print(atom*);

//HERE WE GO
int main(int argc, char** argv) {

    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lithp  = mpc_new("lithp");

    mpca_lang(MPCA_LANG_DEFAULT,
    "                                          \
        number : /-?[0-9]+/ ;                    \
        symbol : '+' | '-' | '*' | '/' ;         \
        sexpr  : '(' <expr>* ')' ;               \
        expr   : <number> | <symbol> | <sexpr> ; \
        lispy  : /^/ <expr>* /$/ ;               \
    ",
    Number, Symbol, Sexpr, Expr, Lithp);

    puts("Lithp version 1.1");
    puts("Press Ctrl-C to exit");

    bool cont_loop = true;

    while (cont_loop) {

        //Get the user input
        char* input = readline("lithp>");
        add_history(input);

        //Parse the input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lithp, &r)) {
            /* Evaluate the AST */
            atom* x = atom_eval(atom_read(r.output));
            atom_println(x);
            atom_del(x);
        } else {
            /* Print the error tree */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    //terminating the parser
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lithp);
    return 0;
}

atom* atom_eval_sexpr(atom* v) {
    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = atom_eval(v->cell[i]);
    }

    /* Error Checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == ATOM_ERR) { return atom_take(v, i); }
    }

    /* Empty Expression */
    if (v->count == 0) {
        return v;
    }

    /* Single Expression */
    if (v->count == 1) {
        return atom_take(v, 0);
    }

    /* Ensure First Element is Symbol */
    atom* f = atom_pop(v, 0);
    if (f->type != ATOM_SYM) {
        atom_del(f); atom_del(v);
        return atom_err("S-expression Does not start with symbol!");
    }

    /* Call builtin with operator */
    atom* result = builtin_op(v, f->sym);
    atom_del(f);
    return result;
}

atom* builtin_op(atom* a, char* op) {

  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != ATOM_NUM) {
      atom_del(a);
      return atom_err("Cannot operate on non-number!");
    }
  }

  /* Pop the first element */
  atom* x = atom_pop(a, 0);

  /* If no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  /* While there are still elements remaining */
  while (a->count > 0) {

    /* Pop the next element */
    atom* y = atom_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        atom_del(x); atom_del(y);
        x = atom_err("Division By Zero!"); break;
      }
      x->num /= y->num;
    }

    atom_del(y);
  }

  atom_del(a); return x;
}

atom* atom_eval(atom* v) {
    if (v->type == ATOM_SEXPR) {
        return atom_eval_sexpr(v);
    }
    /* All other atom types remain the same */
    return v;
}

atom* atom_pop(atom* v, int i) {
  /* Find the item at "i" */
  atom* x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i+1],
    sizeof(atom*) * (v->count-i-1));

  /* Decrease the count of items in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(atom*) * v->count);
  return x;
}

atom* atom_take(atom* v, int i) {
  atom* x = atom_pop(v, i);
  atom_del(v);
  return x;
}

//creation of atoms
atom* atom_num(long x){
    atom *v = malloc(sizeof(atom));
    v->type = ATOM_NUM;
    v->num  = x;
    return v;
}

atom* atom_err(char* m){
    atom *v = malloc(sizeof(atom));
    v->type = ATOM_ERR;
    v->err  = malloc(strlen(m)+1);
    strcpy(v->err,m);
    return v;
}

atom* atom_sym(char* s){
    atom* v = malloc(sizeof(atom));
    v->type = ATOM_SYM;
    v->sym  = malloc(strlen(s)+1);
    strcpy(v->sym,s);
    return v;
}

atom* atom_sexpr(){
    atom* v  = malloc(sizeof(atom));
    v->type  = ATOM_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void atom_del(atom* v) {
  switch (v->type) {
    case ATOM_NUM:
        break;
    case ATOM_ERR:
        free(v->err);
        break;
    case ATOM_SYM:
        free(v->sym);
        break;

    case ATOM_SEXPR:
      for (int i = 0; i < v->count; i++) {
        atom_del(v->cell[i]);
      }
      free(v->cell);
    break;
  }
  free(v);
}

atom* atom_read_num(mpc_ast_t* t){
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        atom_num(x) : atom_err("invalid number");

}

atom* atom_read(mpc_ast_t* t){
    if (strstr(t->tag, "number")) {
        return atom_read_num(t);
    }
    if (strstr(t->tag, "symbol")) {
        return atom_sym(t->contents);
    }

    atom* x = NULL;
    if (strcmp(t->tag,">")==0)    {x = atom_sexpr();}
    if (strcmp(t->tag,"sexpr")==0){x = atom_sexpr();}

    for (int i = 0; i < t->children_num; i++){
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = atom_add(x, atom_read(t->children[i]));
    }

    return x;
}

atom* atom_add(atom* v, atom* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(atom*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

void atom_expr_print(atom* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {

    /* Print Value contained within */
    atom_print(v->cell[i]);

    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void atom_print(atom* v) {
  switch (v->type) {
    case ATOM_NUM:   printf("%li", v->num); break;
    case ATOM_ERR:   printf("Error: %s", v->err); break;
    case ATOM_SYM:   printf("%s", v->sym); break;
    case ATOM_SEXPR: atom_expr_print(v, '(', ')'); break;
  }
}

void atom_println(atom* v) { atom_print(v); putchar('\n'); }
