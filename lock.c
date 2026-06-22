#include "lock.h"

void lock_free(Lock lock)
{
	if (!lock.layer_moves)
		return;
	for (size_t i = 0; i < lock.layers; i++) {
		if (!lock.layer_moves[i])
			continue;
		free(lock.layer_moves[i]);
	}
	free(lock.layer_moves);
	if (lock.layer_ptr)
		free(lock.layer_ptr);
}

void lock_set_move(uint8_t layer, int8_t moves, bool inverted, Lock *lock)
{
	if (layer >= lock->layers || moves >= lock->layers)
		return;

	lock->layer_moves[layer][moves] = inverted ? -1 : 1;
}

Lock lock_new(uint8_t layers) 
{
	Lock ret;

	ret.layers = layers;
	ret.layer_moves = (int8_t **) calloc(ret.layers, sizeof(int8_t *));
	if (!ret.layer_moves) {
		ret.layers = 0;
		return ret;
	}
	ret.layer_ptr = (uint8_t *) calloc(ret.layers, sizeof(uint8_t));
	if (!ret.layer_ptr) {
		ret.layers = 0;
		return ret;
	}
	for (size_t i = 0; i < ret.layers; i++) {
		ret.layer_moves[i] = (int8_t *) calloc(ret.layers, sizeof(int8_t));
		if (!ret.layer_moves[i]) {
			lock_free(ret);
			return ret;
		}
		lock_set_move(i, i, false, &ret);
	}

	return ret;
}

void lock_print(Lock lock)
{
	for (size_t i = 0; i < lock.layers; i++) {
		printf("%2d:    ", i);
		for (size_t j = 0; j < lock.layers; j++) {
			printf("%2d ", lock.layer_moves[i][j]);
		}
		printf("\n");
	}
}

void lock_set_layer(uint8_t layer, uint8_t set, Lock *lock)
{
	if (layer >= lock->layers || set >= lock->layers)
		return;

	lock->layer_ptr[layer] = set;
}

bool is_solved(Lock lock)
{
	for (size_t i = 0; i < lock.layers; i++)
		if (lock.layer_ptr[i] != 3)
			return false;

	return true;
}

bool move(uint8_t layer2move, bool inverted, Lock *lock)
{
	if (layer2move >= lock->layers)
		return false;
	int8_t *moves = lock->layer_moves[layer2move];
	if (!moves)
		return false;

	int8_t mod = inverted ? -1 : 1;

	for (size_t i = 0; i < lock->layers; i++) {
		if (!moves[i])
			continue;

		int8_t move = moves[i] * mod;
		if (lock->layer_ptr[i] + move < 0 || lock->layer_ptr[i] + move >= lock->layers)
			return false;
		lock->layer_ptr += move;
	}

	return true;
}

int main(int argc, char *argv[])
{
	Lock l = lock_new(6);

	// Set moves
	lock_set_move(1, 2, true, &l);
	lock_set_move(1, 3, false, &l);
	lock_set_move(1, 4, false, &l);

	lock_set_move(2, 3, false, &l);

	lock_set_move(4, 1, false, &l);
	lock_set_move(4, 5, true, &l);

	lock_set_move(5, 0, true, &l);
	lock_set_move(5, 1, false, &l);

	// Set starting positions
	lock_set_layer(0, 4, &l);
	lock_set_layer(1, 5, &l);
	lock_set_layer(2, 2, &l);
	lock_set_layer(3, 2, &l);
	lock_set_layer(4, 0, &l);
	lock_set_layer(5, 0, &l);

	lock_print(l);

	lock_free(l);

	return 0;
}
