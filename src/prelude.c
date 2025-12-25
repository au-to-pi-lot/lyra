#include "prelude.h"
#include "closure.h"
#include "intrinsics.h"
#include "types/boolean.h"
#include "types/list.h"
#include "types/macro.h"
#include "types/function.h"
#include "eval.h"

void define_intrinsic_value(GC *gc, Closure *closure, char *key, Value *value) {
    UT_string *ut_key = gc_alloc_string(gc);
    utstring_printf(ut_key, "%s", key);

    set_var(gc, closure, ut_key, value);
}

void define_intrinsic_macro(GC *gc, Closure *closure, char *key, Intrinsic intrinsic_macro) {
    Value *value = make_intrinsic_macro(gc, intrinsic_macro);
    define_intrinsic_value(gc, closure, key, value);
}

void define_intrinsic_function(GC *gc, Closure *closure, char *key, Intrinsic intrinsic_function) {
    Value *value = make_intrinsic_function(gc, intrinsic_function);
    define_intrinsic_value(gc, closure, key, value);
}

void define_intrinsic_function_pure(GC *gc, Closure *closure, char *key, Intrinsic intrinsic_function, bool is_pure) {
    Value *value = make_intrinsic_function_with_purity(gc, intrinsic_function, is_pure);
    define_intrinsic_value(gc, closure, key, value);
}

void prelude(GC *gc, Closure *closure) {
    define_intrinsic_value(gc, closure, "true", TRUE);
    define_intrinsic_value(gc, closure, "false", FALSE);
    define_intrinsic_value(gc, closure, "nil", NIL);

    define_intrinsic_macro(gc, closure, "lambda", &intrinsic_lambda);
    define_intrinsic_macro(gc, closure, "if", &intrinsic_if);
    define_intrinsic_macro(gc, closure, "define", &intrinsic_define);
    define_intrinsic_macro(gc, closure, "define-macro", &intrinsic_define_macro);
    define_intrinsic_macro(gc, closure, "begin", &intrinsic_begin);
    define_intrinsic_macro(gc, closure, "module", &intrinsic_module);
    define_intrinsic_macro(gc, closure, "match", &intrinsic_match);

    // Boolean operators
    define_intrinsic_macro(gc, closure, "and", &intrinsic_and);
    define_intrinsic_macro(gc, closure, "or", &intrinsic_or);
    define_intrinsic_macro(gc, closure, "not", &intrinsic_not);

    // Pure arithmetic functions
    define_intrinsic_function_pure(gc, closure, "+", &intrinsic_add, true);
    define_intrinsic_function_pure(gc, closure, "-", &intrinsic_sub, true);
    define_intrinsic_function_pure(gc, closure, "*", &intrinsic_mul, true);
    define_intrinsic_function_pure(gc, closure, "/", &intrinsic_div, true);
    define_intrinsic_function_pure(gc, closure, "divmod", &intrinsic_divmod, true);

    // Comparison operators
    define_intrinsic_function_pure(gc, closure, "=", &intrinsic_eq, true);
    define_intrinsic_function_pure(gc, closure, "<", &intrinsic_lt, true);
    define_intrinsic_function_pure(gc, closure, "<=", &intrinsic_lt_eq, true);
    define_intrinsic_function_pure(gc, closure, ">", &intrinsic_gt, true);
    define_intrinsic_function_pure(gc, closure, ">=", &intrinsic_gt_eq, true);

    // Pure list functions
    define_intrinsic_function_pure(gc, closure, "car", &intrinsic_car, true);
    define_intrinsic_function_pure(gc, closure, "cdr", &intrinsic_cdr, true);
    define_intrinsic_function_pure(gc, closure, "cons", &intrinsic_cons, true);

    // Purity-polymorphic (depends on function argument) - mark as impure conservatively
    define_intrinsic_function_pure(gc, closure, "apply", &intrinsic_apply, false);

    // Impure functions (side effects)
    define_intrinsic_function_pure(gc, closure, "gensym", &intrinsic_gensym, false);  // Mutates GC counter
    define_intrinsic_function_pure(gc, closure, "load-file", &intrinsic_load_file, false);  // File I/O
    define_intrinsic_function_pure(gc, closure, "eval-string", &intrinsic_eval_string, false);  // Dynamic evaluation
    define_intrinsic_function_pure(gc, closure, "get-var", &intrinsic_get_var, true);  // Just reads
    define_intrinsic_function_pure(gc, closure, "is-pure", &intrinsic_pure_p, true);  // Purity query

    // Type predicates (all pure)
    define_intrinsic_function_pure(gc, closure, "is-cons", &intrinsic_is_cons, true);
    define_intrinsic_function_pure(gc, closure, "is-int", &intrinsic_is_int, true);
    define_intrinsic_function_pure(gc, closure, "is-float", &intrinsic_is_float, true);
    define_intrinsic_function_pure(gc, closure, "is-boolean", &intrinsic_is_boolean, true);
    define_intrinsic_function_pure(gc, closure, "is-string", &intrinsic_is_string, true);
    define_intrinsic_function_pure(gc, closure, "is-symbol", &intrinsic_is_symbol, true);
    define_intrinsic_function_pure(gc, closure, "is-function", &intrinsic_is_function, true);
    define_intrinsic_function_pure(gc, closure, "is-macro", &intrinsic_is_macro, true);
    define_intrinsic_function_pure(gc, closure, "is-nil", &intrinsic_is_nil, true);

    // Type constructors
    define_intrinsic_function_pure(gc, closure, "string", &intrinsic_string, true);
    define_intrinsic_function_pure(gc, closure, "symbol", &intrinsic_symbol, true);

    // Directory operations
    define_intrinsic_function_pure(gc, closure, "list-directory", &intrinsic_list_directory, false);

    // Initialize module registry
    eval(gc, closure, "(define *module-exports* nil)");

    eval(gc, closure,
        "(define Z (lambda (f) "
        "  ((lambda (x) (f (lambda (*args) (apply (x x) args)))) "
        "   (lambda (x) (f (lambda (*args) (apply (x x) args)))))))"
    );

    // Define fn macro: (fn name (params...) body) => (define name (Z (lambda (name) (lambda (params...) body))))
    // This allows the function to reference itself by name for recursion
    // NOTE: Hygienic - all symbols in template are user-provided (name, params, body)
    eval(gc, closure,
        "(define-macro fn (lambda (name params body) "
        "  `(define ,name (Z (lambda (,name) (lambda ,params ,body))))))"
    );

    eval(gc, closure,
        "(fn foldl (f acc lst) "
        "  (if (is-nil lst) "
        "    acc "
        "    (foldl f (f acc (car lst)) (cdr lst))))"
    );

    eval(gc, closure,
        "(fn foldr (f lst acc) "
        "  (if (is-nil lst) "
        "    acc "
        "    (f (car lst) (foldr f (cdr lst) acc))))"
    );

    eval(gc, closure,
        "(fn reverse (lst)"
        "  (foldl (lambda (acc item) (cons item acc)) nil lst))"
    );

    eval(gc, closure,
        "(fn map (f lst) "
        "  (reverse (foldl (lambda (acc item) (cons (f item) acc)) nil lst)))"
    );

    eval(gc, closure,
        "(fn filter (f lst)"
        "  (reverse (foldl (lambda (acc item) (if (f item) (cons item acc) acc)) nil lst)))"
    );

    // Define let macro: (let ((x 1) (y 2)) body...) => ((lambda (x y) body...) 1 2)
    // NOTE: Hygienic - vars/vals only used during expansion, not in generated code
    // Supports implicit progn: multiple body expressions
    eval(gc, closure,
        "(define-macro let (lambda (bindings *body) "
        "  (define vars (map car bindings)) "
        "  (define vals (map (lambda (b) (car (cdr b))) bindings)) "
        "  `((lambda ,vars ,@body) ,@vals)))"
    );

    // Helper: recursively load files from a list
    eval(gc, closure,
        "(fn load-files (module-str files-list) "
        "  (if (is-nil files-list) "
        "    nil "
        "    (begin "
        "      (define file (car files-list)) "
        "      (eval-string (load-file (string module-str \"/\" file))) "
        "      (load-files module-str (cdr files-list)))))"
    );

    // Helper: load all module files from directory
    eval(gc, closure,
        "(define load-module-dir "
        "  (lambda (module-str) "
        "    (define files (list-directory module-str)) "
        "    (load-files module-str files)))"
    );

    // Import macro: (import module-name (sym1 sym2 ...))
    // Supports multi-file modules (math/*.lyra) with (module ...) declarations
    // The macro expands to code that loads the module and imports requested symbols
    eval(gc, closure,
        "(define-macro import (lambda (module-name symbols) "
        "  (define module-str (string module-name)) "
        "  (define marker-name (symbol "
        "    (string \"*module-loaded-\" module-str \"*\"))) "
        "  `(begin "
        "     (if (is-nil (get-var ',marker-name)) "
        "       (begin "
        "         (load-module-dir ,module-str) "
        "         (define ,marker-name true)) "
        "       nil) "
        "     ,@(map (lambda (sym) "
        "              `(define ,sym "
        "                 (begin "
        "                   (define find-export "
        "                     (lambda (exports) "
        "                       (if (is-nil exports) "
        "                         nil "
        "                         (if (= (car (car exports)) ',sym) "
        "                           (car (cdr (car exports))) "
        "                           (find-export (cdr exports)))))) "
        "                   (define find-mod "
        "                     (lambda (registry) "
        "                       (if (is-nil registry) "
        "                         nil "
        "                         (if (= (car (car registry)) ',module-name) "
        "                           (find-export (car (cdr (car registry)))) "
        "                           (find-mod (cdr registry)))))) "
        "                   (find-mod *module-exports*)))) "
        "            symbols))))"
    );
}
