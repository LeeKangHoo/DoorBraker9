#pragma once

#include <linux/types.h>

void add_yellow_card(pid_t tgid);
bool check_yellow_card(pid_t tgid);
void remove_yellow_card(pid_t tgid);
bool mark_yellow_card(pid_t tgid, int newfd);
void clear_yellow_card(void);