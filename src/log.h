#pragma once

#include <stdio.h>

#define LOG_ERROR(MSG...) \
	do { \
		fprintf(stderr, MSG); \
		fprintf(stderr, "\n"); \
	} while(false)

#define LOG_WARN(MSG...) \
	do { \
		fprintf(stderr, MSG); \
		fprintf(stderr, "\n"); \
	} while(false)

#define LOG_INFO(MSG...) \
	do { \
		fprintf(stderr, MSG); \
		fprintf(stderr, "\n"); \
	} while(false)

#define LOG_DEBUG(MSG...) \
	do { \
		fprintf(stderr, "[DEBUG %s, line %d] ", __FILE__, __LINE__); \
		fprintf(stderr, MSG); \
		fprintf(stderr, "\n"); \
	} while(false)

