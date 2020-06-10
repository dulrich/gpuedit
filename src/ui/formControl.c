
#include "stdlib.h"
#include "string.h"



#include "../gui.h"
#include "../gui_internal.h"



void GUIFormControl_SetString(GUIFormControl* w, char* str) {
	int64_t n;
	double d;
	
	switch(w->type) {
		case GUIFORMCONTROL_STRING:
			GUIEdit_SetText(w->edit, str);
			break;
			
		case GUIFORMCONTROL_INT:
			n = strtol(str, NULL, 10);
			GUIEdit_SetInt(w->edit, n);
			break;
			
		case GUIFORMCONTROL_FLOAT:
			d = strtod(str, NULL);
			GUIEdit_SetDouble(w->edit, d);
			break;
			
		default:
			printf("Unsupported type in GUIFormControl_SetString: %d\n", w->type);
	}
}


static void render(GUIFormControl* w, PassFrameParams* pfp) {
	
	GUIHeader_renderChildren(&w->header, pfp);
	
	// title
	Vector2 tl = w->header.absTopLeft;

	AABB2 box;
	box.min.x = tl.x + 5;
	box.min.y = tl.y + 1;
	box.max.x = tl.x + w->header.size.x - 10;
	box.max.y = tl.y + 20;
	
	gui_drawDefaultUITextLine(w->header.gm, &box, &w->header.absClip, &w->header.gm->defaults.windowTitleTextColor, w->header.absZ+0.1, w->label, strlen(w->label));

}

static void delete(GUIFormControl* w) {
	
}

static void updatePos(GUIFormControl* w, GUIRenderParams* grp, PassFrameParams* pfp) {
	GUIHeader* h = &w->header;
	
// 	if(h->flags & GUIFLAG_MAXIMIZE_X) h->size.x = h->parent->header.size.x;
// 	if(h->flags & GUIFLAG_MAXIMIZE_Y) h->size.y = h->parent->header.size.y;
	
	gui_defaultUpdatePos(h, grp, pfp);
}



static void click(GUIObject* w_, GUIEvent* gev) {
	GUIFormControl* w = (GUIFormControl*)w_;
	
}


GUIFormControl* GUIFormControl_New(GUIManager* gm, int type, char* label) {
	
	
	static struct gui_vtbl static_vt = {
		.Render = (void*)render,
// 		.Delete = (void*)delete,
		.UpdatePos = (void*)updatePos,
	};
	
	static struct GUIEventHandler_vtbl event_vt = {
		.Click = click,
	};
	
	GUIFormControl* w = pcalloc(w);
	
	gui_headerInit(&w->header, gm, &static_vt, &event_vt);
	
	w->type = type;
	w->label = strdup(label);
	
	switch(type) {
		case GUIFORMCONTROL_STRING:
		case GUIFORMCONTROL_INT:
		case GUIFORMCONTROL_FLOAT:
			w->edit = GUIEdit_New(gm, "asdfasdf");
			w->edit->header.topleft = (Vector2){0, 0};
			w->edit->header.size = (Vector2){100, 20};
			w->edit->header.gravity = GUI_GRAV_TOP_RIGHT;
			
			GUIRegisterObject(w, w->edit);
			
			break;
	}
	

	
	return w;
}