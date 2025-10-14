#include "value.h"
#include "../gc.h"
#include <utstring.h>

Value *make_string(GC *gc, UT_string *value);