#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct lock {
	uint8_t layers;
	int8_t **layer_moves;
	uint8_t *layer_ptr;
} Lock;
