#ifndef _FLV_H_
#define _FLV_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif
/*
 * Mux h264 byte stream into flv container.
 * return: 0 for success, 1 for failure.
 */
void write_flv(char* data, int len, const char* filename);

#ifdef __cplusplus
}
#endif
#endif
