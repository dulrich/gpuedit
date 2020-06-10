

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "window.h"
#include "app.h"
#include "texture.h"
#include "sti/sti.h"

#include "gui.h"
#include "gui_internal.h"

#include "utilities.h"




static void preFrame(PassFrameParams* pfp, GUIManager* gm);
static void draw(GUIManager* gm, GLuint progID, PassDrawParams* pdp);
static void postFrame(GUIManager* gm);



GUIManager* GUIManager_alloc(GlobalSettings* gs) {
	GUIManager* gm;
	pcalloc(gm);
	
	GUIManager_init(gm, gs);
	
	return gm;
}



static void updatePosRoot(GUIHeader* gh, GUIRenderParams* always_null, PassFrameParams* pfp) {
	GUIRenderParams grp = {
		.size = gh->size,
		.offset = {0,0},
		.clip = {{0,0}, gh->size},
		.baseZ = 0,
	};
	
	VEC_EACH(&gh->children, ind, child) {
		GUIHeader_updatePos(child, &grp, pfp);
	}
}

static void renderRoot(GUIHeader* gh, PassFrameParams* pfp) {
	GUIHeader_renderChildren(gh, pfp);
}



// _init is always called before _initGL
void GUIManager_init(GUIManager* gm, GlobalSettings* gs) {
	
	static struct gui_vtbl root_vt = {
		.UpdatePos = updatePosRoot,
		.Render = renderRoot,
// 		.HitTest = hitTestRoot,
	};
		
	static struct GUIEventHandler_vtbl event_vt = {
		.ParentResize = gui_default_ParentResize,
	};
	
	VEC_INIT(&gm->reapQueue);
	
	gm->gs = gs;
	
	gm->maxInstances = gs->GUIManager_maxInstances;
	
	gm->elementCount = 0;
	gm->elementAlloc = 64;
	gm->elemBuffer = calloc(1, sizeof(*gm->elemBuffer) * gm->elementAlloc);
	
	gm->fm = FontManager_alloc(gs);
	
// 	gm->ta = TextureAtlas_alloc(gs);
// 	gm->ta->width = 256;
// 	TextureAtlas_addFolder(gm->ta, "pre", "assets/ui/icons", 0);
// 	TextureAtlas_finalize(gm->ta);
	
	gm->minDragDist = 2;
	gm->doubleClickTime = 0.300;
	
	gm->root = calloc(1, sizeof(GUIHeader));
	gui_headerInit(gm->root, NULL, &root_vt, &event_vt); 
	
	VEC_INIT(&gm->focusStack);
	VEC_PUSH(&gm->focusStack, gm->root);
	
	gm->defaults.font = FontManager_findFont(gm->fm, "Arial");
	gm->defaults.fontSize = .45;
	gm->defaults.textColor = COLOR4_FROM_HEX(200,200,200,255);
	gm->defaults.windowBgColor = COLOR4_FROM_HEX(10,10,10,255);
	gm->defaults.buttonTextColor = COLOR4_FROM_HEX(200,200,225,255);
	gm->defaults.buttonHoverTextColor = COLOR4_FROM_HEX(200,2,2,255);
	gm->defaults.buttonDisTextColor = COLOR4_FROM_HEX(20,20,20,255);
	gm->defaults.buttonBgColor = COLOR4_FROM_HEX(2,2,225,255);
	gm->defaults.buttonHoverBgColor = COLOR4_FROM_HEX(200,200,2,255);
	gm->defaults.buttonDisBgColor = COLOR4_FROM_HEX(100,100,100,255);
	gm->defaults.buttonBorderColor = COLOR4_FROM_HEX(200,2,225,255);
	gm->defaults.buttonHoverBorderColor = COLOR4_FROM_HEX(2,200,225,255);
	gm->defaults.buttonDisBorderColor = COLOR4_FROM_HEX(20,20,125,255);
	gm->defaults.editBorderColor = COLOR4_FROM_HEX(25,245,25,255);
	gm->defaults.editBgColor = COLOR4_FROM_HEX(20,50,25,255);
	gm->defaults.editWidth = 150;
	gm->defaults.editHeight = 18;
	gm->defaults.cursorColor = COLOR4_FROM_HEX(240,240,240,255);
	
	gm->defaults.windowBgBorderColor = COLOR4_FROM_HEX(180, 180, 0, 255);
	gm->defaults.windowBgBorderWidth = 1;
	gm->defaults.windowBgColor = COLOR4_FROM_HEX(20, 20, 20, 255);
	gm->defaults.windowTitleBorderColor = COLOR4_FROM_HEX(180, 180, 0, 255);
	gm->defaults.windowTitleBorderWidth = 1;
	gm->defaults.windowTitleColor = COLOR4_FROM_HEX(40, 40, 40, 255);
	gm->defaults.windowTitleTextColor = COLOR4_FROM_HEX(210, 210, 0, 255);
	gm->defaults.windowCloseBtnBorderColor = COLOR4_FROM_HEX(210, 40, 0, 255);
	gm->defaults.windowCloseBtnBorderWidth = 1;
	gm->defaults.windowCloseBtnColor = COLOR4_FROM_HEX(180, 60, 0, 255);
	gm->defaults.windowScrollbarColor = COLOR4_FROM_HEX(150, 150, 0, 255);
	gm->defaults.windowScrollbarBorderColor = COLOR4_FROM_HEX(150, 150, 0, 255);
	gm->defaults.windowScrollbarBorderWidth = 1;
	
	gm->defaultCursor = GUIMOUSECURSOR_ARROW;
}


void GUIManager_initGL(GUIManager* gm, GlobalSettings* gs) {
	static VAOConfig vaoConfig[] = {
		{0, 4, GL_FLOAT, 0, GL_FALSE}, // top, left, bottom, right
		{0, 4, GL_FLOAT, 0, GL_FALSE}, // tlbr clipping planes
		{0, 4, GL_UNSIGNED_BYTE, 0, GL_FALSE}, // tex indices 1&2, tex fade, gui type
		{0, 4, GL_UNSIGNED_SHORT, 0, GL_TRUE}, // tex offset 1&2
		{0, 4, GL_UNSIGNED_SHORT, 0, GL_TRUE}, // tex size 1&2
		
		{0, 4, GL_UNSIGNED_BYTE, 0, GL_TRUE}, // fg color
		{0, 4, GL_UNSIGNED_BYTE, 0, GL_TRUE}, // bg color
		
		{0, 4, GL_FLOAT, 0, GL_FALSE}, // z-index, alpha, opts 1-2
		
		{0, 0, 0}
	};

	
	gm->vao = makeVAO(vaoConfig);
	glBindVertexArray(gm->vao);
	
	int stride = calcVAOStride(0, vaoConfig);

	PCBuffer_startInit(&gm->instVB, gm->maxInstances * stride, GL_ARRAY_BUFFER);
	updateVAO(0, vaoConfig); 
	PCBuffer_finishInit(&gm->instVB);
	
	///////////////////////////////
	// font texture
	
	glGenTextures(1, &gm->fontAtlasID);
	glBindTexture(GL_TEXTURE_2D_ARRAY, gm->fontAtlasID);
	glerr("bind font tex");
	
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0); // no mipmaps for this; it'll get fucked up
	glerr("param font tex");
	
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R8, gm->fm->atlasSize, gm->fm->atlasSize, VEC_LEN(&gm->fm->atlas));
	
	VEC_EACH(&gm->fm->atlas, ind, at) {
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 
			0, 0, ind, // offsets
			gm->fm->atlasSize, gm->fm->atlasSize, 1, 
			GL_RED, GL_UNSIGNED_BYTE, at);
		glerr("load font tex");
	}
	
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	
	
	///////////////////////////////
	// regular texture atlas
	
	glGenTextures(1, &gm->atlasID);
	glBindTexture(GL_TEXTURE_2D_ARRAY, gm->atlasID);
	glerr("bind font tex");
	
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0); // no mipmaps for this; it'll get fucked up
	glerr("param font tex");
	
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, gm->ta->width, gm->ta->width, VEC_LEN(&gm->ta->atlas));
	
	char buf [50];
	
	VEC_EACH(&gm->ta->atlas, ind, at2) {
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 
			0, 0, ind, // offsets
			gm->ta->width, gm->ta->width, 1, 
			GL_RGBA, GL_UNSIGNED_BYTE, at2);
		glerr("load font tex");	
	}
	
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	
	//////////////////////////////////
	
}




void GUIManager_SetMainWindowTitle(GUIManager* gm, char* title) {
	if(gm->windowTitleSetFn) gm->windowTitleSetFn(gm->windowTitleSetData, title);
}

void GUIManager_SetCursor(GUIManager* gm, int cursor) {
	if(gm->currentCursor == cursor) return;
	gm->currentCursor = cursor;
	if(gm->mouseCursorSetFn) gm->mouseCursorSetFn(gm->mouseCursorSetData, cursor);
}


GUIObject* GUIManager_triggerClick(GUIManager* gm, Vector2 testPos) {
	GUIObject* go = GUIManager_hitTest(gm, testPos);
	
	if(go) GUIObject_triggerClick(go, testPos);
	
	return go;
}


GUIObject* GUIManager_hitTest(GUIManager* gm, Vector2 testPos) {
	GUIObject* go = GUIObject_hitTest(gm->root, testPos);
	return go == gm->root ? NULL : go;
}


void GUIManager_updatePos(GUIManager* gm, PassFrameParams* pfp) {
	GUIHeader_updatePos(gm->root, NULL, pfp);
}




void GUIManager_Reap(GUIManager* gm) {
	VEC_EACH(&gm->reapQueue, i, h) {
		GUIObject_Reap_(h);
		
	}
	
	#define check_nullfiy(n) if(gm->n && gm->n->deleted) gm->n = NULL;
	check_nullfiy(lastHoveredObject)
	check_nullfiy(dragStartTarget)
	#undef check_nullify
	
	VEC_EACH(&gm->focusStack, i, fh) {
		if(fh->header.deleted) {
			VEC_RM_SAFE(&gm->focusStack, i);
		}
	}
	
	VEC_TRUNC(&gm->reapQueue);
}


GUIUnifiedVertex* GUIManager_checkElemBuffer(GUIManager* gm, int count) {
	if(gm->elementAlloc < gm->elementCount + count) {
		gm->elementAlloc = MAX(gm->elementAlloc * 2, gm->elementAlloc + count);
		gm->elemBuffer = realloc(gm->elemBuffer, sizeof(*gm->elemBuffer) * gm->elementAlloc);
	}
	
	return gm->elemBuffer + gm->elementCount;
}

GUIUnifiedVertex* GUIManager_reserveElements(GUIManager* gm, int count) {
	GUIUnifiedVertex* v = GUIManager_checkElemBuffer(gm, count);
	gm->elementCount += count;
	return v;
}



// Event Handling


static unsigned int translateModKeys(GUIManager* gm, InputEvent* iev) {
	unsigned int m = 0;
	
	if(iev->kbmods & IS_CONTROL) {
		m |= GUIMODKEY_CTRL;
		// check which one
	}
	if(iev->kbmods & IS_ALT) {
		m |= GUIMODKEY_ALT;
		// check which one
	}
	if(iev->kbmods & IS_SHIFT) {
		m |= GUIMODKEY_SHIFT;
		// check which one
	}
	if(iev->kbmods & IS_TUX) {
		m |= GUIMODKEY_TUX;
		// check which one
	}
	
	return m;
}


void GUIManager_HandleMouseMove(GUIManager* gm, InputState* is, InputEvent* iev) {
	Vector2 newPos = {
		iev->intPos.x, iev->intPos.y
	};
	
	// find the deepest target
	GUIObject* t = GUIManager_hitTest(gm, newPos);
	if(t && t->header.deleted) {
		printf("Event target is deleted\n");
		return;
	}
	
	GUIEvent gev = {
		.type = GUIEVENT_MouseMove,
		.originalTarget = NULL,
		.currentTarget = NULL,
		.eventTime = 0,
		.pos = newPos,
		.character = 0, // N/A
		.keycode = 0, // N/A
		.modifiers = translateModKeys(gm, iev), 
		.cancelled = 0,
		.requestRedraw = 0,
	};
	
	
	if(!t) { // mouse left the window
		if(gm->lastHoveredObject) {
			gev.type = GUIEVENT_MouseLeave;
			gev.originalTarget = gm->lastHoveredObject;
			gev.currentTarget = gm->lastHoveredObject;
			gev.cancelled = 0;
			GUIManager_BubbleEvent(gm, gm->lastHoveredObject, &gev);
			
			gm->lastHoveredObject = NULL;
		}
		
		return;
	}
	
	
	// set the cursor, maybe
	int cur = t->h.cursor; 
	GUIManager_SetCursor(gm, cur);

	// mouse enter/leave
	if(!gm->lastHoveredObject) {
		gev.type = GUIEVENT_MouseEnter;
		gev.originalTarget = t;
		gev.currentTarget = t;
		gev.cancelled = 0;
		GUIManager_BubbleEvent(gm, t, &gev);
	}
	else if(gm->lastHoveredObject != t) {
		gev.type = GUIEVENT_MouseLeave;
		gev.originalTarget = gm->lastHoveredObject;
		gev.currentTarget = gm->lastHoveredObject;
		gev.cancelled = 0;
		GUIManager_BubbleEvent(gm, gm->lastHoveredObject, &gev);
		
		gev.type = GUIEVENT_MouseEnter;
		gev.originalTarget = t;
		gev.currentTarget = t;
		gev.cancelled = 0;
		GUIManager_BubbleEvent(gm, t, &gev);
	}
	
	gm->lastHoveredObject = t;
	
	// check for dragging
	if(gm->isMouseDown && !gm->isDragging && gm->dragStartTarget) {
		float dragDist = vDist2(&newPos, &gm->dragStartPos);
		if(dragDist >= gm->minDragDist) {
			gm->isDragging = 1;
			
			gev.type = GUIEVENT_DragStart;
			gev.originalTarget = gm->dragStartTarget;
			gev.currentTarget = t;
			gev.dragStartPos = gm->dragStartPos;
			gev.cancelled = 0;
			GUIManager_BubbleEvent(gm, gm->dragStartTarget, &gev);
		};
	}
	
	// DragMove event
	if(gm->isDragging) {
		gev.type = GUIEVENT_DragMove;
		gev.originalTarget = gm->dragStartTarget;
		gev.currentTarget = t;
		gev.dragStartPos = gm->dragStartPos;
		gev.cancelled = 0;
		GUIManager_BubbleEvent(gm, gm->dragStartTarget, &gev);
	}
	
	gev.type = GUIEVENT_MouseMove;
	gev.originalTarget = t;
	gev.currentTarget = t;
	gev.cancelled = 0;
	GUIManager_BubbleEvent(gm, t, &gev);
	
	// TODO: handle redraw request
}

void GUIManager_HandleMouseClick(GUIManager* gm, InputState* is, InputEvent* iev) {
	
	Vector2 newPos = {
		iev->intPos.x, iev->intPos.y
	};
	
	// find the deepest target
	GUIObject* t = GUIManager_hitTest(gm, newPos);
	if(!t) return; // TODO handle mouse leaves;
	
	// buttons: 
	// 1 - left
	// 2 - mid
	// 3 - right
	// 4 - scroll up
	// 5 - scroll down
	// 6 - scroll left
	// 7 - scroll right
	// 8 - front left side (on my mouse)
	// 9 - rear left side (on my mouse)
	
	GUIEvent gev = {
		.type = GUIEVENT_MouseUp,
		.originalTarget = t,
		.currentTarget = t,
		.eventTime = iev->time,
		.pos = newPos,
		.button = iev->button, 
		.keycode = 0, // N/A
		.modifiers = translateModKeys(gm, iev),
		.multiClick = 1,
		.cancelled = 0,
		.requestRedraw = 0,
	};
	
	
	// TODO doubleclick
	
	
	// TODO: check requestRedraw between each dispatch
	if(iev->type == EVENT_MOUSEDOWN) {
		gev.type = GUIEVENT_MouseDown;
		gev.currentTarget = t,
		gev.cancelled = 0;
		GUIManager_BubbleEvent(gm, t, &gev);
		
		gm->isMouseDown = 1;
		gm->dragStartPos = newPos;
		gm->dragStartTarget = t;
		gev.dragStartPos = gm->dragStartPos;
		
	} else if(iev->type == EVENT_MOUSEUP) {
		char suppressClick = 0;
		
		// check for drag end
		if(gm->isDragging) {
			gev.type = GUIEVENT_DragStop,
			gev.currentTarget = t,
			gev.cancelled = 0;
			gev.dragStartPos = gm->dragStartPos;
			GUIManager_BubbleEvent(gm, t, &gev);
			
			// end dragging
			gm->dragStartTarget = NULL;
			gm->isDragging = 0;
			gm->dragButton = 0;
			
			suppressClick = 1;
		}
		
		gm->isMouseDown = 0;
		
		gev.type = GUIEVENT_MouseUp,
		gev.currentTarget = t,
		gev.cancelled = 0;
		GUIManager_BubbleEvent(gm, t, &gev);
		
		// scroll wheel
		if(iev->button == 4 || iev->button == 5) {
			
			if(iev->button == 4) {
				gev.type = GUIEVENT_ScrollUp;
				gev.currentTarget = t;
				gev.cancelled = 0;
				GUIManager_BubbleEvent(gm, t, &gev);
			}
			else {
				gev.type = GUIEVENT_ScrollDown;
				gev.currentTarget = t;
				gev.cancelled = 0;
				GUIManager_BubbleEvent(gm, t, &gev);
			}
			
			gev.type = GUIEVENT_Scroll;
			gev.currentTarget = t;
			gev.cancelled = 0;
			GUIManager_BubbleEvent(gm, t, &gev);
			
			return;
		}
		
		// aux click; hscroll, etc
		if(iev->button > 5) {
			gev.type = GUIEVENT_AuxClick;
			gev.currentTarget = t;
			gev.cancelled = 0;
			GUIManager_BubbleEvent(gm, t, &gev);
			
			return;
		}
		
		
		// normal clicks
		if(iev->button == 1) {
			
			if(!suppressClick) {
				if(gm->clickHistory[0].button == 1 && 
					iev->time < gm->clickHistory[0].time + gm->doubleClickTime) {
					
					gev.multiClick = 2; 
				}
				
				gev.type = GUIEVENT_Click;
				gev.currentTarget = t;
				gev.cancelled = 0;
				GUIManager_BubbleEvent(gm, t, &gev);
			
			}
		}
		else if(iev->button == 2) {
			// TODO: replace when better input management exists
			gev.type = GUIEVENT_MiddleClick;
			gev.currentTarget = t;
			gev.cancelled = 0;
			GUIManager_BubbleEvent(gm, t, &gev);
		}
		else if(iev->button == 3) {
			// TODO: replace when better input management exists
			gev.type = GUIEVENT_RightClick;
			gev.currentTarget = t;
			gev.cancelled = 0;
			GUIManager_BubbleEvent(gm, t, &gev);
		}
		
		
		
		// push the latest click onto the stack
		gm->clickHistory[2] = gm->clickHistory[1];
		gm->clickHistory[1] = gm->clickHistory[0];
		gm->clickHistory[0].time = iev->time;
		gm->clickHistory[0].button = iev->button;
		
	}
	
}

void GUIManager_HandleKeyInput(GUIManager* gm, InputState* is, InputEvent* iev) {
	
	int type;
	
	// translate event type
	switch(iev->type) {
		case EVENT_KEYDOWN: type = GUIEVENT_KeyDown; break; 
		case EVENT_KEYUP: type = GUIEVENT_KeyUp; break; 
		case EVENT_TEXT: type = GUIEVENT_KeyUp; break; 
		default:
			fprintf(stderr, "!!! Non-keyboard event in GUIManager_HandleKeyInput: %d\n", iev->type);
			return; // not actually a kb event
	}
	
	GUIObject* t = GUIManager_getFocusedObject(gm);
	if(!t) {
		fprintf(stderr, "key input with no focused object\n");
		return; // TODO ???
	}
	
	GUIEvent gev = {
		.type = type,
		.originalTarget = t,
		.currentTarget = t,
		.eventTime = 0,
		.pos = {0,0}, // N/A
		.character = iev->character, 
		.keycode = iev->keysym, 
		.modifiers = translateModKeys(gm, iev),
		.cancelled = 0,
		.requestRedraw = 0,
	};
	
	GUIManager_BubbleEvent(gm, t, &gev);
}


// handles event bubbling and logic
void GUIManager_BubbleEvent(GUIManager* gm, GUIObject* target, GUIEvent* gev) {
	
	GUIObject* obj = target;
	
	int bubble = GUIEventBubbleBehavior[gev->type];
	
	if(bubble == 0) {
		// no bubbling, just the target
		GUIObject_TriggerEvent(obj, gev);
	}
	else if(bubble == 1) {
		// bubble until cancelled
		while(obj && !gev->cancelled) {
			gev->currentTarget = obj;
			GUIObject_TriggerEvent(obj, gev);
			
			obj = obj->h.parent;
		}
	}
	else if(bubble = 2) {
		// trigger on all parents
		while(obj) {
			gev->currentTarget = obj;
			GUIObject_TriggerEvent(obj, gev);
			
			obj = obj->h.parent;
		}
	}
	else {
		fprintf(stderr, "!!! unknown bubbling behavior: %d\n", bubble);
	}
	
}


// lowest level of event triggering
// does not do bubbling
void GUIObject_TriggerEvent_(GUIHeader* o, GUIEvent* gev) {
	
	if(!o || !o->event_vt) return;
	
	switch(gev->type) {
		#define X(name, b) case GUIEVENT_##name: \
				if(o->event_vt && o->event_vt->name) (*o->event_vt->name)((GUIObject*)o, gev); \
				break;
			
			GUIEEVENTTYPE_LIST
		#undef X
	}
	
	if(o->event_vt && o->event_vt->Any) (*o->event_vt->Any)((GUIObject*)o, gev);
}




// focus stack functions

GUIObject* GUIManager_getFocusedObject(GUIManager* gm) {
	if(VEC_LEN(&gm->focusStack) == 0) return NULL;
	return VEC_TAIL(&gm->focusStack);
}

GUIObject* GUIManager_popFocusedObject(GUIManager* gm) {
	GUIObject* o;
	
	// can't pop off the root element at the bottom
	if(VEC_LEN(&gm->focusStack) <= 1) return VEC_TAIL(&gm->focusStack);
	
	VEC_POP(&gm->focusStack, o);
	
	// TODO focus events
	
	return o;
}


void GUIManager_pushFocusedObject_(GUIManager* gm, GUIHeader* h) {
	
	GUIHeader* old = (GUIHeader*)GUIManager_getFocusedObject(gm);
	
	GUIEvent gev = {};
	gev.type = GUIEVENT_LostFocus;
	gev.originalTarget = old;
	gev.currentTarget = old;
	
	GUIManager_BubbleEvent(gm, old, &gev);

	VEC_PUSH(&gm->focusStack, (GUIObject*)h);
	
	gev.type = GUIEVENT_GainedFocus;
	gev.originalTarget = h;
	gev.currentTarget = h;
	
	GUIManager_BubbleEvent(gm, h, &gev);
}



// Rendering



static int gui_elem_sort_fn(GUIUnifiedVertex* a, GUIUnifiedVertex* b) {
	return a->z == b->z ? 0 : (a->z > b->z ? 1 : -1);
}


static void preFrame(PassFrameParams* pfp, GUIManager* gm) {
	GUIUnifiedVertex* vmem = PCBuffer_beginWrite(&gm->instVB);
	if(!vmem) {
		printf("attempted to update invalid PCBuffer in GUIManager\n");
		return;
	}
	
	
	gm->elementCount = 0;
	
	
	GUIRenderParams grp = {
		.offset = {0,0}, 
		.size = {800,800},
		.clip = {(0,0),{800,800}},
	};
	gm->root->h.size = (Vector2){800, 800};
	
	GUIHeader_updatePos(gm->root, &grp, pfp);
	
	GUIHeader_render(gm->root, pfp);
	
	
	// test element 
	/*
	GUIUnifiedVertex* v = GUIManager_reserveElements(gm, 1);
		
	*v = (GUIUnifiedVertex){
// 		.pos = {gw->header.topleft.x, gw->header.topleft.y,
// 			gw->header.topleft.x + gw->header.size.x, gw->header.topleft.y + gw->header.size.y},
		.pos = { 250, 250, 700, 700},
		.clip = {0, 0, 800, 800},
		
		.texIndex1 = 2,
		.texIndex2 = 0,
		.texFade = .5,
		.guiType = 4, // bordered window (just a box)
		
		.texOffset1 = 0,
		.texOffset2 = 0,
		.texSize1 = 0,
		.texSize2 = 0,
		
		.fg = {255, 255, 255, 255}, // TODO: border color
		.bg = {128, 0, 0, 0}, // TODO: color
		
		.z = 99999,
		.alpha = 1.0,
	};
	/* */ 
	
	
	
	double sort;
	
// 	sort = getCurrentTime();
	qsort(gm->elemBuffer, gm->elementCount, sizeof(*gm->elemBuffer), (void*)gui_elem_sort_fn);
// 	printf("qsort time: [%d elem] %f\n", gm->elementCount, timeSince(sort)  * 1000.0);
	
// 	sort = getCurrentTime();
	memcpy(vmem, gm->elemBuffer, gm->elementCount * sizeof(*gm->elemBuffer));
// printf("memcpy time: %f\n", timeSince(sort) * 1000.0);
	
}

static void draw(GUIManager* gm, GLuint progID, PassDrawParams* pdp) {
	size_t offset;
	

// 	if(mdi->uniformSetup) {
// 		(*mdi->uniformSetup)(mdi->data, progID);
// 	}
	GLuint ts_ul = glGetUniformLocation(progID, "fontTex");
	GLuint ta_ul = glGetUniformLocation(progID, "atlasTex");
	
	glActiveTexture(GL_TEXTURE0 + 29);
	glUniform1i(ts_ul, 29);
	glexit("text sampler uniform");
 	glBindTexture(GL_TEXTURE_2D_ARRAY, gm->fontAtlasID);
//  	glBindTexture(GL_TEXTURE_2D, gt->font->textureID); // TODO check null ptr
 	glexit("bind texture");



	glActiveTexture(GL_TEXTURE0 + 28);
	glUniform1i(ta_ul, 28);
	glexit("text sampler uniform");
 	glBindTexture(GL_TEXTURE_2D_ARRAY, gm->atlasID);
//  	glBindTexture(GL_TEXTURE_2D, gt->font->textureID); // TODO check null ptr
 	glexit("bind texture");
	
	// ------- draw --------
	
	glBindVertexArray(gm->vao);
	
	PCBuffer_bind(&gm->instVB);
	offset = PCBuffer_getOffset(&gm->instVB);
	glDrawArrays(GL_POINTS, offset / sizeof(GUIUnifiedVertex), gm->elementCount);
	
	glexit("");
}



static void postFrame(GUIManager* gm) {
	PCBuffer_afterDraw(&gm->instVB);
}


RenderPass* GUIManager_CreateRenderPass(GUIManager* gm) {
	
	RenderPass* rp;
	PassDrawable* pd;

	pd = GUIManager_CreateDrawable(gm);

	rp = calloc(1, sizeof(*rp));
	RenderPass_init(rp);
	RenderPass_addDrawable(rp, pd);
	//rp->fboIndex = LIGHTING;
	
	return rp;
}


PassDrawable* GUIManager_CreateDrawable(GUIManager* gm) {
	
	PassDrawable* pd;
	static ShaderProgram* prog = NULL;
	
	if(!prog) {
		prog = loadCombinedProgram("guiUnified");
		glexit("");
	}
	
	
	pd = Pass_allocDrawable("GUIManager");
	pd->data = gm;
	pd->preFrame = preFrame;
	pd->draw = (PassDrawFn)draw;
	pd->postFrame = postFrame;
	pd->prog = prog;
	
	return pd;;
}