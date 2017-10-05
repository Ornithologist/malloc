#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#ifndef __COMMON_H__
#define __COMMON_H__

#define BASE_BLOCK 8
#define MIN_NUM_ARENA 2
#define SIZE_TO_ORDER(size) ((int) ceil( (log(size) / log(BASE_BLOCK) )) )

#endif