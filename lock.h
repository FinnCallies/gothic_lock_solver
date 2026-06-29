#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct lock {
	uint8_t layers;
	int8_t **layer_moves;
	uint8_t *layer_ptr;
} Lock;

typedef struct move {
	uint8_t layer;
	bool inverted;
} Move;

#define PIN 3
#define HOLES 7
#define MAX_DEPTH 8

void correct(Lock *l);
