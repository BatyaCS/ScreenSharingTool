#ifndef COMPILER_EXTERN_C_H_
#define COMPILER_EXTERN_C_H_

#ifdef __cplusplus
#define EXTERN_C extern "C"
#define EXTERN_C_BLOCK extern "C" {
#define EXTERN_C_END };
#else
#define EXTERN_C
#define EXTERN_C_BLOCK
#define EXTERN_C_END
#endif

#endif /* COMPILER_EXTERN_C_H_ */