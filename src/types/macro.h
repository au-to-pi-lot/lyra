#include "value.h"
#include "../gc.h"

Value *make_intrinsic_macro(GC *gc, Intrinsic intrinsic_macro);
Value *make_derived_macro(GC *gc, Derived derived);
