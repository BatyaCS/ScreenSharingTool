#ifndef COMPILER_ATTRIBUTES_H_
#define COMPILER_ATTRIBUTES_H_

#define ALIAS(func) __attribute__((alias(func)))
#define OPTIMIZE(level) __attribute__((optimize(level)))
#define SECTION(name) __attribute__((__section__(name)))
#define PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#define PACKED __attribute__((packed))
#define USED __attribute__((used))
#define NO_INLINE __attribute__((noinline))
#define WEAK __attribute__((weak))
#define ALIGNED(...) __attribute__((aligned(__VA_ARGS__)))
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define NAKED __attribute__((naked))
#define CONST_RESULT __attribute__ ((const))

#if defined(__cplusplus) && __cplusplus >= 201103L 
#define NO_RETURN [[noreturn]]
#else
#if defined(_MSC_VER)
#define NO_RETURN __declspec(noreturn)
#else
#define NO_RETURN __attribute__((noreturn))
#endif
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L
#define FALLTHROUGH [[fallthrough]]
#elif (defined(__GNUC__) && __GNUC__ >= 7)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif

#if defined(__arm__)
#define INTERRUPT_FN __attribute__((interrupt))
#else
#define INTERRUPT_FN  /* Required for ARM */
#endif
#endif /* COMPILER_ATTRIBUTES_H_ */