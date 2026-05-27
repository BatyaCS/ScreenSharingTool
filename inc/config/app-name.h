#ifndef APP_NAME_H_
#define APP_NAME_H_

#include <version/version.h>
#include <macro/str.h>
#include <macro/glue.h>

#define APP_NAME "Batya Streamer"
#define APP_VERSION_STRING STR(VERSION_MAJOR) "." STR(VERSION_MINOR) "." STR(VERSION_REVISION)
#define APP_FULL_NAME GLUE3(APP_NAME, " ", APP_VERSION_STRING)

#endif /* APP_NAME_H_ */