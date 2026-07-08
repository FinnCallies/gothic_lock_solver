#include "lock.h"
#include <time.h>

// The shifts needed to solve the lock
Shift *solution;
size_t idx;

void lock_free(Lock lock)
{
	if (!lock.disk_deps)
		return;
	for (size_t i = 0; i < lock.disks; i++) {
		if (!lock.disk_deps[i])
			continue;
		free(lock.disk_deps[i]);
	}
	free(lock.disk_deps);
	if (lock.disk_ptr)
		free(lock.disk_ptr);
}

void lock_set_dependency(uint8_t disk, int8_t shifts, bool inverted, Lock *lock)
{
	if (disk >= lock->disks || shifts >= lock->disks)
		return;

	lock->disk_deps[disk][shifts] = inverted ? -1 : 1;
}

Lock lock_new(uint8_t disks) 
{
	Lock ret;

	ret.disks = disks;
	ret.disk_deps = (int8_t **) calloc(ret.disks, sizeof(int8_t *));
	if (!ret.disk_deps) {
		ret.disks = 0;
		return ret;
	}
	ret.disk_ptr = (uint8_t *) calloc(ret.disks, sizeof(uint8_t));
	if (!ret.disk_ptr) {
		ret.disks = 0;
		return ret;
	}
	for (size_t i = 0; i < ret.disks; i++) {
		ret.disk_deps[i] = (int8_t *) calloc(ret.disks, sizeof(int8_t));
		if (!ret.disk_deps[i]) {
			lock_free(ret);
			return ret;
		}
		lock_set_dependency(i, i, false, &ret);
	}

	return ret;
}

void lock_print_shifts(Lock lock)
{
	for (size_t i = 0; i < lock.disks; i++) {
		printf("%2ld:    ", i);
		for (size_t j = 0; j < lock.disks; j++) {
			printf("%2d ", lock.disk_deps[i][j]);
		}
		printf("\n");
	}
}

void lock_print(Lock lock)
{
	printf("      v\n");
	for (size_t i = 0; i < lock.disks; i++) {
		for (size_t s = 0; s < HOLES - (lock.disk_ptr[i] + 1); s++)
			printf(" ");
		printf("oooxooo\n");
	}
}

void lock_set_disk_ptr(uint8_t disk, uint8_t set, Lock *lock)
{
	if (disk >= lock->disks || set >= HOLES)
		return;

	lock->disk_ptr[disk] = set;
}

bool is_solved(Lock lock)
{
	for (size_t i = 0; i < lock.disks; i++)
		if (lock.disk_ptr[i] != PIN)
			return false;

	return true;
}

/**
 * Compare two locks states a and b
 * This will NOT compare dependencies but simply the state of the locks disks
 */
bool lock_state_cmp(Lock a, Lock b)
{
	if (a.disks != b.disks)
		return false;

	for (size_t i = 0; i < a.disks; i++)
		if (a.disk_ptr[i] != b.disk_ptr[i])
			return false;

	return true;
}

Lock lock_cp(Lock lock)
{
	Lock ret;

	ret = lock_new(lock.disks);

	for (size_t i = 0; i < lock.disks; i++) {
		ret.disk_ptr[i] = lock.disk_ptr[i];
		for (size_t j = 0; j < lock.disks; j++) {
			ret.disk_deps[i][j] = lock.disk_deps[i][j];
		}
	}

	return ret;
}

/**
 * Copy the disk states from one lock to another
 */
bool lock_state_cpy(Lock from, Lock *to)
{
	if (from.disks != to->disks)
		return false;

	for (size_t i = 0; i < from.disks; i++) {
		to->disk_ptr[i] = from.disk_ptr[i];
	}

	return true;
}

/**
 * True for disks which have no dependencies
 */
bool disk_is_indepedent(Lock lock, uint8_t disk)
{
	for (size_t i = 0; i < lock.disks; i++) {
		if (i == disk)
			continue;
		if (lock.disk_deps[disk][i] != 0)
			return false;
	}

	return true;
}

bool shift(Shift shift, Lock *lock)
{
	if (shift.disk >= lock->disks)
		return false;
	int16_t mod = shift.inverted ? -1 : 1;
	Lock c = lock_cp(*lock);

	// For disk in lock
	for (size_t i = 0; i < lock->disks; i++) {
		// Continue if no dependency
		if (lock->disk_deps[shift.disk][i] == 0)
			continue;

		// apply inverted state
		int16_t m = (int16_t) lock->disk_deps[shift.disk][i] * mod;

		// apply dependency
		if ((int16_t) lock->disk_ptr[i] +  m < 0 || (int16_t) lock->disk_ptr[i] + m >= HOLES) {
			// Exit early if move not possible
			lock_free(c);
			return false;
		}
		c.disk_ptr[i] += m;
	}

	// Apply state from working copy to lock
	lock_state_cpy(c, lock);

	lock_free(c);
	return true;
}

/**
 * Fix independent disks
 */
void correct(Lock *l)
{
	for (size_t j = 0; j < l->disks; j++) {
		if (!disk_is_indepedent(*l, j))
			continue;
		if (l->disk_ptr[j] == PIN)
			continue;
		Shift s = { .disk = j, .inverted = l->disk_ptr[j] < PIN ? false : true };
		while (l->disk_ptr[j] != PIN)
			shift(s, l);
	}
}

uint16_t evaluate_lock_state(Lock lock, uint8_t depth)
{
	uint16_t value = 0;

	if (is_solved(lock))
		return UINT16_MAX - depth;

	for (size_t i = 0; i < lock.disks; i++) {
		if (disk_is_indepedent(lock, i))
			continue;
		if (lock.disk_ptr[i] == PIN) {
			value += 1000;
			for (size_t j = 0; j < lock.disks; j++)
				if (i != j && lock.disk_deps[i][j] != 0)
					value += 100;
		} else if (lock.disk_ptr[i] < PIN) {
			value += lock.disk_ptr[i] * 10;
		} else {
			value += (PIN - (lock.disk_ptr[i] - PIN)) * 10;
		}
	}

	if (value >= depth)
		value -= depth;

	return value;
}

bool shift_is_inverted(Shift a, Shift b)
{
	if (a.disk == b.disk && a.inverted != b.inverted)
		return true;
	return false;
}

uint16_t research(Lock lock, uint8_t depth, Shift last)
{
	uint16_t max = 0;

	if (depth == MAX_DEPTH || is_solved(lock))
		return evaluate_lock_state(lock, depth);
	for (size_t i = 0; i < lock.disks; i++) {
		if (disk_is_indepedent(lock, i))
			continue;
		for (size_t j = 0; j < 2; j++) {
			Lock copy = lock_cp(lock);
			Shift s = { .disk = i, .inverted = j ? false : true };
			if (shift_is_inverted(last, s))
				goto skip;
			if (!shift(s, &copy))
				goto skip;
			correct(&copy);
			uint16_t ret = research(copy, depth + 1, s);
			if (ret > max)
				max = ret;
skip:
			lock_free(copy);
		}
	}

	return max;
}

Shift solve(Lock lock, Shift last)
{
	uint16_t max = 0;
	Shift best = { .disk = 0, .inverted = false };

	for (size_t i = 0; i < lock.disks; i++) {
		if (disk_is_indepedent(lock, i))
			continue;
		for (size_t j = 0; j < 2; j++) {
			Lock copy = lock_cp(lock);
			Shift s = { .disk = i, .inverted = j ? false : true };
			if (shift_is_inverted(last, s))
				goto next;
			if (!shift(s, &copy))
				goto next;
			uint16_t val = research(copy, 1, s);
			if (val > max || (val == max && rand() % 2 == 0)) {
				best.disk = i;
				best.inverted = j ? false : true;
				max = val;
			}
next:
			lock_free(copy);
		}
	}

	printf("\n");

	return best;
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	int rc = 0;
	Shift next;
	Lock l;
       
	next.disk = UINT8_MAX;
       	next.inverted = false;

	// double the size to be able to store one correction shift per shift (and hopw this is enough)
	solution = calloc(ITERATIONS, sizeof(Shift));
	idx = 0;

	printf("\n====== Welcome to the Gothic 1 Remake Lock Solver ======\n\n");

	printf("How many disks does your lock have? (1-8) ");
	scanf("%hhd", &l.disks);
	while (l.disks > 8 || l.disks == 0) {
		printf("Invalid disk count %hhd is not between 1 and 8\n\n", l.disks);
		printf("How many disks does your lock have? (1-8) ");
		scanf("%hhd", &l.disks);
	}
	printf("\n");
	l = lock_new(l.disks);

	for (size_t i = 0; i < l.disks; i++) {
		printf("What is the position of disk %ld? (0-6) ", i);
		scanf("%hhd", &l.disk_ptr[i]);
		while (l.disk_ptr[i] >= HOLES) {
			printf("Invalid disk position %hhn is greater than maximum disk position %d\n\n", l.disk_ptr, HOLES - 1);
			printf("What is the position of disk %ld? (0-6) ", i);
			scanf("%hhd", &l.disks);
		}

		for (size_t j = 0; j < l.disks; j++) {
			char c;
			if (i == j)
				continue;
			printf("Does disk %ld have a dependency on disk %ld? (y/n/i/N) ", i, j);
			scanf(" %c", &c);
			while (c != 'y' && c != 'n' && c != 'i' && c != 'N') {
				printf("Invalid character %c\n\n", c);
				printf("Does disk %ld have a dependency on disk %ld? (y/n/i/N) ", i, j);
				scanf(" %c", &c);
			}
			if (c == 'N') {
				j = l.disks;
				continue;
			}
			l.disk_deps[i][j] = c == 'y' ? 1 : c == 'n' ? 0 : -1;
		}
		printf("\n");
	}

	lock_print_shifts(l);
	lock_print(l);

	// solve independent
	correct(&l);
	lock_print(l);

	// solve
	for (size_t i = 0; i < ITERATIONS && !is_solved(l); i++) {
		next = solve(l, next);
		printf("%d, %s\n", next.disk, next.inverted ? "d" : "a");

		// apply best shift
		solution[idx] = next;
		idx++;
		if (idx >= ITERATIONS) {
			printf("out of bounds\n");
			return 1;
		}
		shift(next, &l);

		correct(&l);

		lock_print(l);
	}

	printf("\n");
	if (!is_solved(l)) {
		printf("Not solved in %d iterations\n", ITERATIONS);
		rc = 1;
	} else {
		printf("Solved!\n");
		for (size_t i = 0; i < idx; i++)
			printf("%d, %s\n", solution[i].disk, solution[i].inverted ? "d" : "a");
	}

	free(solution);
	lock_free(l);

	return rc;
}
