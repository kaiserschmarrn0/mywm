void cycle(void *arg) {
	if (!stack[curws].lists[TYPE_NORMAL] ||
			!stack[curws].lists[TYPE_NORMAL]->next[curws][TYPE_NORMAL]) {
		return;
	}

	events[XCB_ENTER_NOTIFY] = NULL;
	
	if (state != CYCLE) {
		state = CYCLE;
		traverse(curws, TYPE_NORMAL, release_events);

		if (stack[curws].fwin->next[curws][TYPE_NORMAL]) {
			marker = stack[curws].fwin->next[curws][TYPE_NORMAL];
		} else {
			marker = stack[curws].lists[TYPE_NORMAL];
		}
	}

	if (marker->next[curws][TYPE_NORMAL]) {
		if (marker = stack[curws].lists[TYPE_NORMAL]->next[curws][TYPE_NORMAL]) {
			mywm_lower(stack[curws].lists[TYPE_NORMAL]);
		}

		window *temp = marker;
		marker = marker->next[curws][TYPE_NORMAL];

		cycle_raise(temp);

		mywm_raise(temp);
		center_pointer(temp);
		focus(temp);
	} else {
		cycle_raise(marker);
		
		window *temp = marker;
		marker = stack[curws].lists[TYPE_NORMAL];

		mywm_raise(temp);
		center_pointer(temp);
		focus(temp);
	}
}
