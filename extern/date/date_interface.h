#pragma once

typedef struct {
	int year;
	int month;
	int day;
} date_time;

#ifdef __cplusplus
extern "C" {
#endif
int64_t date_to_epoch(const date_time *dt);
void date_from_epoch(int64_t offset, date_time *dt);
#ifdef __cplusplus
}
#endif

