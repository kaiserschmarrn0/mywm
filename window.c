#include <stdio.h>

#include "window.h"
#include "mywm.h"
#include "workspace.h"

void stack_above(window *subj) {
	uint32_t mask = XCB_CONFIG_WINDOW_STACK_MODE;
	uint32_t val  = XCB_STACK_MODE_ABOVE;
	xcb_configure_window(conn, subj->windows[WIN_PARENT], mask, &val);
}

void raise(window *subj) {
	printf("raising %d\n", subj->windows[WIN_CHILD]);
	if (subj == stack[curws].lists[TYPE_ALL]) {
		printf("raise aborted\n");
		return;
	}

	excise_from(curws, subj);
	insert_into(curws, subj);
}
