#include "workspace.h"
#include "window.h"

workspace stack[NUM_WS] = { { { NULL, NULL, 0 }, NULL } };
int curws = 0;

void print_stack(int ws, int type) {
	printf("workspace %x [fwin: %x]: ", ws, stack[ws].fwin ? stack[ws].fwin->windows[WIN_CHILD] : 0);
	for (window *list = stack[ws].lists[type].first; list; list = list->next[ws][type]) {
		printf("win %x -> ", list->windows[WIN_CHILD]);
	}
	printf("NULL\n");
}

void print_all_stacks(int ws) {
	for (int i = 0; i < TYPE_COUNT; i++) {
		print_stack(ws, i);
	}
}

void traverse(int ws, int type, void (*func)(window *)) {
	for (window *list = stack[ws].lists[type].first; list; list = list->next[ws][type]) {
		func(list);
	}
}

void safe_traverse(int ws, int type, void (*func)(window *)) {
	for (window *list = stack[ws].lists[type].first; list;) {
		window *temp = list->next[ws][type];
		func(list);
		list = temp;
	}
}

static void insert_into_helper(int ws, int type, window *win) {
	win->next[ws][type] = stack[ws].lists[type].first;
	win->prev[ws][type] = NULL;

	if (stack[ws].lists[type].first) {
		stack[ws].lists[type].first->prev[ws][type] = win;
	} else {
		stack[ws].lists[type].last = win;
	}

	stack[ws].lists[type].first = win;
	
	stack[ws].lists[type].count++;
}

void insert_into(int ws, window *win) {
	insert_into_helper(ws, TYPE_ALL, win);

	if (win->normal) {
		stack_above(win);
		insert_into_helper(ws, TYPE_NORMAL, win);
	} else {
		stack_above_abnormal(win);
	}
	
	if (win->above) {
		insert_into_helper(ws, TYPE_ABOVE, win);
	} else if (!win->is_i_full && !win->is_e_full) {
		safe_traverse(ws, TYPE_ABOVE, mywm_raise);
	}

	print_stack(ws, TYPE_ALL);
}

void append_to_helper(int ws, int type, window *win) {
	win->prev[ws][type] = stack[ws].lists[type].last;
	win->next[ws][type] = NULL;

	if (stack[ws].lists[type].last) {
		stack[ws].lists[type].last->next[ws][type] = win;
	} else {
		stack[ws].lists[type].first = win;
	}

	stack[ws].lists[type].last = win;
	
	stack[ws].lists[type].count++;
}

void append_to(int ws, window *win) {
	append_to_helper(ws, TYPE_ALL, win);

	if (win->normal) {
		stack_below(win);
		append_to_helper(ws, TYPE_NORMAL, win);
	} else {
		stack_below_abnormal(win);
	}

	if (win->above) {
		fprintf(stderr, "mywm: can't append always-above windows.\n");
		return;
	} else {
		//handle always-below :^)
	}
}

static void excise_from_helper(int ws, int type, window *subj) {
	if (subj->next[ws][type]) {
		subj->next[ws][type]->prev[ws][type] = subj->prev[ws][type];
	} else {
		stack[ws].lists[type].last = subj->prev[ws][type];
	}
	
	if (subj->prev[ws][type]) {
		subj->prev[ws][type]->next[ws][type] = subj->next[ws][type];
	} else {
		stack[ws].lists[type].first = subj->next[ws][type];
	}
	
	stack[ws].lists[type].count--;
}

void excise_from(int ws, window *win) {
	if (win->normal) {
		excise_from_helper(ws, TYPE_NORMAL, win);
	}

	if (win->above) {
		excise_from_helper(ws, TYPE_ABOVE, win);
	}
	
	excise_from_helper(ws, TYPE_ALL, win);
}

static void all_but_helper(int ws, window *win, void (*func)(int, window *)) {
	int i;
	for (i = 0; i < ws; i++) {
		func(i, win);
	}

	for (i++; i < NUM_WS; i++) {
		func(i, win);
	}
}

void insert_into_all_but(int ws, window *win) {
	all_but_helper(ws, win, insert_into);
}

void excise_from_all_but(int ws, window *win) {
	all_but_helper(ws, win, excise_from);
}

search_data search_range(int ws, int type, int start_index, int end_index, xcb_window_t id) {
	search_data ret;
	for (ret.win = stack[ws].lists[type].first; ret.win; ret.win = ret.win->next[ws][type]) {
		for (int i = start_index; i < end_index; i++) {
			if (ret.win->windows[i] == id) {
				ret.index = i;
				return ret;
			}
		}
	}
	return ret;
}

window *search_ws(int ws, int type, int window_index, xcb_window_t id) {
	window *cur;
	for (cur = stack[ws].lists[type].first; cur; cur = cur->next[ws][type]) {
		if (cur->windows[window_index] == id) {
			break;
		}
	}
	return cur;
}

window *search_all(int *ws, int type, int window_index, xcb_window_t id) {
	window *ret;
	for (int i = 0; i < NUM_WS; i++) {
		ret = search_ws(i, type, window_index, id);
		if (ret) {
			if (ws) {
				*ws = i;
			}
			break;
		}
	}
	return ret;
}

void refocus(int ws) {
	window *win;
	if (stack[ws].fwin) {
		win = stack[ws].fwin;
	} else if (stack[ws].lists[TYPE_NORMAL].first) {
		win = stack[ws].lists[TYPE_NORMAL].first;	
	} else {
		return;
	}

	if (win->is_i_full || win->is_e_full) {
		mywm_raise(win);
	}

	focus(win);
}
