#include "lock.h"
#include <time.h>

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

void lock_print_moves(Lock lock)
{
	for (size_t i = 0; i < lock.layers; i++) {
		printf("%2d:    ", i);
		for (size_t j = 0; j < lock.layers; j++) {
			printf("%2d ", lock.layer_moves[i][j]);
		}
		printf("\n");
	}
}

void lock_print(Lock lock)
{
	printf("      v\n");
	for (size_t i = 0; i < lock.layers; i++) {
		for (size_t s = 0; s < HOLES - (lock.layer_ptr[i] + 1); s++)
			printf(" ");
		printf("oooxooo\n");
	}
}

void lock_set_layer(uint8_t layer, uint8_t set, Lock *lock)
{
	if (layer >= lock->layers || set >= HOLES)
		return;

	lock->layer_ptr[layer] = set;
}

bool is_solved(Lock lock)
{
	for (size_t i = 0; i < lock.layers; i++)
		if (lock.layer_ptr[i] != PIN)
			return false;

	return true;
}

bool lock_cmp(Lock a, Lock b)
{
	if (a.layers != b.layers)
		return false;

	for (size_t i = 0; i < a.layers; i++)
		if (a.layer_ptr[i] != b.layer_ptr[i])
			return false;

	return true;
}

Lock lock_cp(Lock lock)
{
	Lock ret;

	ret = lock_new(lock.layers);

	for (size_t i = 0; i < lock.layers; i++) {
		ret.layer_ptr[i] = lock.layer_ptr[i];
		for (size_t j = 0; j < lock.layers; j++) {
			ret.layer_moves[i][j] = lock.layer_moves[i][j];
		}
	}

	return ret;
}

bool lock_apply(Lock from, Lock *to)
{
	if (from.layers != to->layers)
		return false;

	for (size_t i = 0; i < from.layers; i++) {
		to->layer_ptr[i] = from.layer_ptr[i];
	}

	return true;
}

bool layer_is_indepedent(Lock lock, uint8_t layer)
{
	for (size_t i = 0; i < lock.layers; i++) {
		if (i == layer)
			continue;
		if (lock.layer_moves[layer][i] != 0)
			return false;
	}

	return true;
}

bool move(Move move, Lock *lock)
{
	if (move.layer >= lock->layers)
		return false;
	int8_t mod = move.inverted ? -1 : 1;
	Lock c = lock_cp(*lock);

	for (size_t i = 0; i < lock->layers; i++) {
		if (lock->layer_moves[move.layer][i] == 0)
			continue;

		int8_t m = lock->layer_moves[move.layer][i] * mod;
		if (lock->layer_ptr[i] + m < 0 || lock->layer_ptr[i] + m >= HOLES) {
			lock_free(c);
			return false;
		}
		c.layer_ptr[i] += m;
	}

	lock_apply(c, lock);

	lock_free(c);
	return true;
}

void correct(Lock *l)
{
	for (size_t j = 0; j < l->layers; j++) {
		if (!layer_is_indepedent(*l, j))
			continue;
		if (l->layer_ptr[j] == PIN)
			continue;
		Move m = { .layer = j, .inverted = l->layer_ptr[j] < PIN ? false : true };
		while (l->layer_ptr[j] != PIN)
			move(m, l);
	}
}

uint8_t evaluate(Lock lock, uint8_t depth)
{
	uint8_t value = 0;

	if (is_solved(lock))
		return UINT8_MAX - depth;

	for (size_t i = 0; i < lock.layers; i++) {
		if (layer_is_indepedent(lock, i))
			continue;
		if (lock.layer_ptr[i] == PIN) {
			value += (UINT8_MAX / lock.layers);
			for (size_t j = 0; j < lock.layers; j++)
				if (i != j)
					value += lock.layer_moves[i][j];
		} else if (lock.layer_ptr[i] < PIN) {
			value += lock.layer_ptr[i];
		} else {
			value += PIN - (lock.layer_ptr[i] - PIN);
		}
	}

	if (value >= depth)
		value -= depth;

	return value;
}

bool is_inverted(Move a, Move b)
{
	if (a.layer == b.layer && a.inverted != b.inverted)
		return true;
	return false;
}

uint8_t back_research(Lock lock, uint8_t depth, Move last)
{
	uint8_t min = UINT8_MAX;

	if (depth == MAX_DEPTH)
		return evaluate(lock, 0);
	for (size_t i = 0; i < lock.layers; i++) {
		for (size_t j = 0; j < 2; j++) {
			Lock copy = lock_cp(lock);
			Move m = { .layer = i, .inverted = j ? false : true };
			if (layer_is_indepedent(lock, i))
				goto skip;
			if (is_inverted(last, m))
				goto skip;
			if (!move(m, &copy))
				goto skip;
			correct(&copy);
			uint8_t ret = back_research(copy, depth + 1, m);
			if (ret > 0 && ret < min)
				min = ret;
skip:
			lock_free(copy);
		}
	}

	return min;
}

uint8_t research(Lock lock, uint8_t depth, Move last, Lock back_search)
{
	uint8_t max = 0;

	if (depth == MAX_DEPTH || is_solved(lock) || lock_cmp(lock, back_search))
		return evaluate(lock, depth);
	for (size_t i = 0; i < lock.layers; i++) {
		for (size_t j = 0; j < 2; j++) {
			Lock copy = lock_cp(lock);
			Move m = { .layer = i, .inverted = j ? false : true };
			if (layer_is_indepedent(lock, i))
				goto skip;
			if (is_inverted(last, m))
				goto skip;
			if (!move(m, &copy))
				goto skip;
			correct(&copy);
			uint8_t ret = research(copy, depth + 1, m, back_search);
			if (ret > max)
				max = ret;
skip:
			lock_free(copy);
		}
	}

	return max;
}

Move back_solve(Lock lock, Move last)
{
	uint8_t min = UINT8_MAX;
	Move worst = { .layer = 0, .inverted = false };

	for (size_t i = 0; i < lock.layers; i++) {
		for (size_t j = 0; j < 2; j++) {
			Lock copy = lock_cp(lock);
			Move m = { .layer = i, .inverted = j ? false : true };
			if (layer_is_indepedent(lock, i))
				goto out;
			if (is_inverted(last, m))
				goto out;
			if (!move(m, &copy))
				goto out;
			correct(&copy);
			uint8_t val = back_research(copy, 1, m);
			if (val > 0 && val < min) {
				worst.layer = i;
				worst.inverted = j ? false : true;
				min = val;
			}
out:
			lock_free(copy);
		}
	}

	return worst;
}

Move solve(Lock lock, Move last, Lock back_search)
{
	uint8_t max = 0;
	Move best = { .layer = 0, .inverted = false };

	for (size_t i = 0; i < lock.layers; i++) {
		for (size_t j = 0; j < 2; j++) {
			Lock copy = lock_cp(lock);
			Move m = { .layer = i, .inverted = j ? false : true };
			if (layer_is_indepedent(lock, i))
				goto out;
			if (is_inverted(last, m))
				goto out;
			if (!move(m, &copy))
				goto out;
			correct(&copy);
			uint8_t val = research(copy, 1, m, back_search);
			if (val > max || (val == max && rand() % 2 == 0)) {
				best.layer = i;
				best.inverted = j ? false : true;
				max = val;
			}
out:
			lock_free(copy);
		}
	}

	printf("\n");

	return best;
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	Lock l = lock_new(7);
	Move next = { .layer = UINT8_MAX, .inverted = false };
	size_t iterations = 100;
	Move *solution = calloc(2 * iterations, sizeof(Move));
	size_t idx = 0;

	// Set moves
	lock_set_move(0, 5, true, &l);
	lock_set_move(1, 0, false, &l);
	lock_set_move(2, 6, false, &l);
	lock_set_move(3, 0, false, &l);
	lock_set_move(3, 6, true, &l);
	lock_set_move(4, 3, true, &l);
	lock_set_move(4, 6, false, &l);
	lock_set_move(5, 0, false, &l);
	lock_set_move(6, 0, false, &l);
	lock_set_move(6, 5, false, &l);

	// Set starting positions
	lock_set_layer(0, 6, &l);
	lock_set_layer(1, 4, &l);
	lock_set_layer(2, 0, &l);
	lock_set_layer(3, 6, &l);
	lock_set_layer(4, 5, &l);
	lock_set_layer(5, 4, &l);
	lock_set_layer(6, 5, &l);

	lock_print_moves(l);
	lock_print(l);

	Lock copy = lock_cp(l);

	// solve copy
//	for (size_t j = 0; j < copy.layers; j++) {
//		copy.layer_ptr[j] = PIN;
//	}
//	for (size_t i = 0; i < MAX_DEPTH; i++) {
//
//		Move next = back_solve(copy, next);
//		move(next, &copy);
//
//		for (size_t j = 0; j < copy.layers; j++) {
//			if (!layer_is_indepedent(copy, j))
//				continue;
//			if (copy.layer_ptr[j] == PIN)
//				continue;
//			Move m = { .layer = j, .inverted = copy.layer_ptr[j] < PIN ? false : true };
//			while (copy.layer_ptr[j] != PIN) {
//				move(m, &copy);
//			}
//		}
//	}
//	printf("back search:\n");
//	lock_print(copy);

	// solve independent
	for (size_t j = 0; j < l.layers; j++) {
		if (!layer_is_indepedent(l, j))
			continue;
		if (l.layer_ptr[j] == PIN)
			continue;
		Move m = { .layer = j, .inverted = l.layer_ptr[j] < PIN ? false : true };
		while (l.layer_ptr[j] != PIN) {
			solution[idx] = m;
			idx++;
			printf("%d, %s\n", m.layer, m.inverted ? "d" : "a");
			move(m, &l);
		}
	}
	lock_print(l);

	// solve
	for (size_t i = 0; i < iterations && !is_solved(l); i++) {
		Move next = solve(l, next, copy);
		printf("%d, %s\n", next.layer, next.inverted ? "d" : "a");
		solution[idx] = next;
		idx++;
		move(next, &l);

		for (size_t j = 0; j < l.layers; j++) {
			if (!layer_is_indepedent(l, j))
				continue;
			if (l.layer_ptr[j] == PIN)
				continue;
			Move m = { .layer = j, .inverted = l.layer_ptr[j] < PIN ? false : true };
			while (l.layer_ptr[j] != PIN) {
				solution[idx] = m;
				idx++;
				printf("%d, %s\n", m.layer, m.inverted ? "d" : "a");
				move(m, &l);
			}
		}

		lock_print(l);
	}

	for (size_t i = 0; i < idx; i++)
		printf("%d, %s\n", solution[i].layer, solution[i].inverted ? "d" : "a");

	lock_free(l);

	return 0;
}
