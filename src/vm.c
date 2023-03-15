#define _POSIX_C_SOURCE 200809L // clock_gettime() and related definitions

#include <stdlib.h>
#include <time.h>

#include "vm.h"
#include "memory.h"

#define INSTRUCTIONS_INITIAL_SIZE 256
#define FRAME_LENGTH (1.0 / 60.0)

uint8_t *instructions;
size_t instruction_capacity;
size_t instruction_count;

bool init_vm()
{
	instructions = gvm_malloc(INSTRUCTIONS_INITIAL_SIZE);
	instruction_capacity = INSTRUCTIONS_INITIAL_SIZE;
	instruction_count = 0;
	if (!instructions) {
		return false;
	}

	return true;
}

void close_vm()
{
	gvm_free(instructions);
}

bool instruction(uint8_t byte)
{
	if (instruction_count >= instruction_capacity) {
		instructions = gvm_realloc(instructions, instruction_capacity, 2 * instruction_capacity);
		if (!instructions) {
			return false;
		}
	}

	instructions[instruction_count++] = byte;
	return true;
}

// Return value: true if should quit
bool run_chunk()
{
	for (int i = 0; i < instruction_count; ++i) {
		switch (instructions[i]) {
			case OP_RETURN:
				return true;
			default:
				return true;
		}
	}
	return false;
}

// Return seconds since some arbitrary point
double time_seconds()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double secs = (double)now.tv_sec;
	double nsecs = (double)now.tv_nsec;
	return secs + nsecs / 1000000000.0;
}

// Returns control after 'duration' seconds
void delay(double duration)
{
	if (duration <= 0) {
		return;
	}

	double target = time_seconds() + duration;
	while (time_seconds() < target) {
		// TODO: Evil and bad, improve
	}
}

void run_vm()
{
	bool loop_done = false;

	while (!loop_done) {
		double next_frame = time_seconds() + FRAME_LENGTH;
		loop_done = run_chunk();
		delay(next_frame - time_seconds());
	}
}
