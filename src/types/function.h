#include "value.h"
#include "../gc.h"

Value *make_function(GC *gc, Callable *definition);

Value *make_derived_function(GC *gc, Derived derived);
Value *make_intrinsic_function(GC *gc, Intrinsic intrinsic);
