#ifndef COMMON_H_
#define COMMON_H_

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cinttypes>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdbool.h>
#endif

#include <macro/static-assert.h>
#include <macro/str.h>

#include <compiler/attributes.h>
#include <compiler/extern-c.h>

#include <type/schar.h>
#include <type/uchar.h>
#include <type/ushort.h>
#include <type/uint.h>
#include <type/ulong.h>
#include <type/ulonglong.h>

#include <config.h>

#include <log/log.h>
#include <debug/trace.h>
#include <timer/timer.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#endif /* COMMON_H_ */