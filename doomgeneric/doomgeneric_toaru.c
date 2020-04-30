#include "doomkeys.h"

#include "doomgeneric.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <toaru/yutani.h>
#include <toaru/graphics.h>
#include <toaru/decorations.h>
#include <toaru/spinlock.h>
#include <toaru/menu.h>

static yutani_t * yctx;
static yutani_window_t * wina;
static gfx_context_t * ctx;
static char * wintitle = NULL;

#define KEYQUEUE_SIZE 512

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(unsigned int key) {
	switch (key) {
		case 27: return KEY_ESCAPE;
		case 257: return KEY_UPARROW;
		case 258: return KEY_DOWNARROW;
		case 260: return KEY_LEFTARROW;
		case 259: return KEY_RIGHTARROW;
		case ' ': return KEY_USE;
		case 1001: return KEY_FIRE; // left ctrl
		case 1011: return KEY_FIRE; // right ctrl
		/* Modern WASD bindings */
		case 'a': return KEY_STRAFE_L;
		case 'd': return KEY_STRAFE_R;
		case 'w': return KEY_UPARROW; // w should move forward
		case 's': return KEY_DOWNARROW; // s should move back
		//case 'r': return KEY_USE;
		//case ' ': return KEY_FIRE;
		case '\n': return KEY_ENTER;
		case 1002: return KEY_RSHIFT;
		case 1003: return KEY_RALT;
	}

	return key;
}

static void addKeyToQueue(int pressed, unsigned int keyCode) {
	unsigned char key = convertToDoomKey(keyCode);

	unsigned short keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

void DG_Init() {
	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));

	yctx = yutani_init();
	if (!yctx) {
		fprintf(stderr, "doomgeneric: failed to connect to compositor\n");
		exit(1);
	}

	init_decorations();
	struct decor_bounds bounds;
	decor_get_bounds(NULL, &bounds);

	wina = yutani_window_create(yctx, DOOMGENERIC_RESX + bounds.width, DOOMGENERIC_RESY + bounds.height);
	yutani_window_move(yctx, wina, 300, 300);
	ctx = init_graphics_yutani_double_buffer(wina);
	yutani_window_advertise_icon(yctx, wina, "Doom", "doom");

}


void DG_DrawFrame() {
	struct decor_bounds bounds;
	decor_get_bounds(wina, &bounds);

	for (int y = 0; y < DOOMGENERIC_RESY; ++y) {
		for (int x = 0; x < DOOMGENERIC_RESX; ++x) {
			unsigned int pixel = DG_ScreenBuffer[y * DOOMGENERIC_RESX + x];
			GFX(ctx,x+bounds.left_width,y+bounds.top_height) = pixel | rgba(0,0,0,255);
		}
	}

	render_decorations(wina, ctx, wintitle ? wintitle : "Doom");
	flip(ctx);
	yutani_flip(yctx, wina);

	/* Get window events */
	yutani_msg_t * m = yutani_poll_async(yctx);
	while (m) {
		menu_process_event(yctx, m);
		switch (m->type) {
			case YUTANI_MSG_KEY_EVENT:
				{
					struct yutani_msg_key_event * ke = (void*)m->data;
					if (ke->wid == wina->wid) {
						addKeyToQueue(ke->event.action == KEY_ACTION_DOWN, ke->event.keycode);
					}
				}
				break;
			case YUTANI_MSG_WINDOW_FOCUS_CHANGE:
				{
					struct yutani_msg_window_focus_change * wf = (void*)m->data;
					if (wf->wid == wina->wid) {
						wina->focused = wf->focused;
					}
				}
				break;
			case YUTANI_MSG_WINDOW_CLOSE:
			case YUTANI_MSG_SESSION_END:
				exit(0);
				break;
			case YUTANI_MSG_WINDOW_MOUSE_EVENT:
				{
					struct yutani_msg_window_mouse_event * me = (void*)m->data;
					if (me->wid == wina->wid) {
						switch (decor_handle_event(yctx, m)) {
							case DECOR_CLOSE:
								exit(0);
								break;
							case DECOR_RIGHT:
								decor_show_default_menu(wina, wina->x + me->new_x, wina->y + me->new_y);
								break;
						}
					}
				}
				break;
		}
		m = yutani_poll_async(yctx);
	}
}

void DG_SleepMs(uint32_t ms) {
	usleep (ms * 1000);
}

uint32_t DG_GetTicksMs() {
	struct timeval  tp;
	struct timezone tzp;
	gettimeofday(&tp, &tzp);
	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); /* return milliseconds */
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	/* Poll for keys */
	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
	{
		//key queue is empty

		return 0;
	}
	else
	{
		unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
		s_KeyQueueReadIndex++;
		s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

		*pressed = keyData >> 8;
		*doomKey = keyData & 0xFF;

		return 1;
	}
}

void DG_SetWindowTitle(const char * title) {
	if (wintitle) free(wintitle);
	wintitle = strdup(title);
	yutani_window_advertise_icon(yctx, wina, wintitle, "doom");
}
