#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

typedef struct lock {
	uint8_t disks;
	int8_t **disk_deps;
	uint8_t *disk_ptr;
} Lock;

typedef struct shift {
	uint8_t disk;
	bool inverted;
} Shift;

#define PIN 3
#define HOLES 7
#define MAX_DEPTH 10
#define ITERATIONS 100

void correct(Lock *l, bool track);
