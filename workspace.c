#include "workspace.h"
#include "window.h"

workspace stack[NUM_WS] = { { { NULL }, NULL } };
int curws = 0;

void print_stack(int ws, int type) {
	printf("workspace %x [%x]: ", ws, stack[ws].lists[type]);
	for (window *list = stack[ws].lists[type]; list; list = list->next[ws][type]) {
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
	for (window *list = stack[ws].lists[type]; list; list = list->next[ws][type]) {
		func(list);
	}
}

void safe_traverse(int ws, int type, void (*func)(window *)) {
	for (window *list = stack[ws].lists[type]; list;) {
		window *temp = list;
		list = temp->next[ws][type];
		func(temp);
	}
}

static void insert_into_helper(int ws, int type, window *win) {
	win->next[ws][type] = stack[ws].lists[type];
	win->prev[ws][type] = NULL;

	if (stack[ws].lists[type]) {
		stack[ws].lists[type]->prev[ws][type] = win;
	}

	stack[ws].lists[type] = win;
}

void insert_into(int ws, window *win) {
	stack_above(win);
	
	insert_into_helper(ws, TYPE_ALL, win);

	if (win->normal) {
		insert_into_helper(ws, TYPE_NORMAL, win);
	}

	print_stack(ws, TYPE_ALL);

	if (win->above) {
		insert_into_helper(ws, TYPE_ABOVE, win);
	} else if (!win->is_i_full && !win->is_e_full) {
		safe_traverse(ws, TYPE_ABOVE, mywm_raise);
	}
}

static void excise_from_helper(int ws, int type, window *subj) {
	if (subj->next[ws][type]) {
		subj->next[ws][type]->prev[ws][type] = subj->prev[ws][type];
	}
	
	if (subj->prev[ws][type]) {
		subj->prev[ws][type]->next[ws][type] = subj->next[ws][type];
	} else {
		stack[ws].lists[type] = subj->next[ws][type];
	}
}

void excise_from(int ws, window *win) {
	/*printf("excised: %x\n", win->windows[WIN_CHILD]); meh */
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

window *search_ws(int ws, int type, int window_index, xcb_window_t id) {
	window *cur;
	for (cur = stack[ws].lists[type]; cur; cur = cur->next[ws][type]) {
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
	} else if (stack[ws].lists[TYPE_NORMAL]) {
		win = stack[ws].lists[TYPE_NORMAL];	
	} else {
		return;
	}

	if (win->is_i_full || win->is_e_full) {
		mywm_raise(win);
	}

	focus(win);
}
