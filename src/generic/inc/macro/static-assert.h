#ifndef STATIC_ASSERT
#define STATIC_ASSERT(x) static_assert((x), "Assertation '" STR(x) "' failed")
#endif /* STATIC_ASSERT */