#include "value.h"
#include "../gc.h"

Value *make_function(GC *gc, Callable *definition);

Value *make_derived_function(GC *gc, Derived derived);
Value *make_derived_function_with_purity(GC *gc, Derived derived, bool is_pure);

Value *make_intrinsic_function(GC *gc, Intrinsic intrinsic);
Value *make_intrinsic_function_with_purity(GC *gc, Intrinsic intrinsic, bool is_pure);
