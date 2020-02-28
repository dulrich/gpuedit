#ifndef __EACSMB_GUI_H__
#define __EACSMB_GUI_H__

#include <stdatomic.h>

#include "common_gl.h"
#include "common_math.h"

#include "utilities.h"

#include "input.h"
#include "texture.h"
#include "pass.h"
#include "pcBuffer.h"

#include "font.h"


struct GameState;
typedef struct GameState GameState;

struct GUIManager;
typedef struct GUIManager GUIManager;



#define GUI_GRAV_TOP_LEFT      0x00
#define GUI_GRAV_LEFT_TOP      0x00
#define GUI_GRAV_CENTER_LEFT   0x01
#define GUI_GRAV_LEFT_CENTER   0x01
#define GUI_GRAV_BOTTOM_LEFT   0x02
#define GUI_GRAV_LEFT_BOTTOM   0x02
#define GUI_GRAV_CENTER_BOTTOM 0x03
#define GUI_GRAV_BOTTOM_CENTER 0x03
#define GUI_GRAV_BOTTOM_RIGHT  0x04
#define GUI_GRAV_RIGHT_BOTTOM  0x04
#define GUI_GRAV_CENTER_RIGHT  0x05
#define GUI_GRAV_RIGHT_CENTER  0x05
#define GUI_GRAV_TOP_RIGHT     0x06
#define GUI_GRAV_RIGHT_TOP     0x06
#define GUI_GRAV_CENTER_TOP    0x07
#define GUI_GRAV_TOP_CENTER    0x07
#define GUI_GRAV_CENTER        0x08
#define GUI_GRAV_CENTER_CENTER 0x08


struct Color4 {
	uint8_t r,g,b,a;
} __attribute__ ((packed));

struct Color3 {
	uint8_t r,g,b;
} __attribute__ ((packed));


typedef struct GUIUnifiedVertex {
	struct { float l, t, r, b; } pos;
	struct { float l, t, r, b; } clip;
	uint8_t texIndex1, texIndex2, texFade, guiType; 
	struct { uint16_t x, y; } texOffset1, texOffset2;
	struct { uint16_t x, y; } texSize1, texSize2;
	
	struct Color4 fg;
	struct Color4 bg;
	
	float z, alpha, opt1, opt2;
	
} __attribute__ ((packed)) GUIUnifiedVertex;





typedef union GUIObject GUIObject;
struct GUIManager;

struct GUIRenderParams;
typedef struct GUIRenderParams GUIRenderParams;


struct gui_vtbl {
	void (*UpdatePos)(GUIObject* go, GUIRenderParams* grp, PassFrameParams* pfp);
	void (*Render)(GUIObject* go, PassFrameParams* pfp);
	void (*Delete)(GUIObject* go);
	void (*Reap)(GUIObject* go);
	void (*Resize)(GUIObject* go, Vector2 newSz); // exterior size
	Vector2 (*GetClientSize)(GUIObject* go);
	void    (*SetClientSize)(GUIObject* go, Vector2 cSize); // and force exterior size to match
	Vector2 (*RecalcClientSize)(GUIObject* go); // recalc client size from the client children and call SetClientSize
	GUIObject* (*HitTest)(GUIObject* go, Vector2 testPos);
	
	void (*AddClient)(GUIObject* parent, GUIObject* child);
	void (*RemoveClient)(GUIObject* parent, GUIObject* child);
};


struct GUIEvent;
typedef struct GUIEvent GUIEvent;
typedef void (*GUI_EventHandlerFn)(GUIObject*, GUIEvent*);

// bubbling: 0=none, just the target
//           1=rise until cancelled
//           2=all parents

//  X(name, bubbling) 
#define GUIEEVENTTYPE_LIST \
	X(Any, 0) \
	X(Click, 1) \
	X(DoubleClick, 1) \
	X(MiddleClick, 1) \
	X(RightClick, 1) \
	X(AuxClick, 1) \
	X(MouseDown, 2) \
	X(MouseUp, 2) \
	X(MouseMove, 2) \
	X(KeyDown, 1) \
	X(KeyUp, 1) \
	X(MouseEnter, 1) \
	X(MouseLeave, 1) \
	X(MouseEnterChild, 2) \
	X(MouseLeaveChild, 2) \
	\
	X(DragStart, 1) \
	X(DragStop, 1) \
	X(DragMove, 1) \
	\
	X(Scroll, 1) \
	X(ScrollDown, 1) \
	X(ScrollUp, 1) \
	\
	X(GainedFocus, 0) \
	X(LostFocus, 0) \
	X(ParentResize, 0) \
	X(Paste, 1) 


enum GUIEventType {
#define X(name, b) GUIEVENT_##name,
	GUIEEVENTTYPE_LIST
#undef X
};
enum GUIEventType_Bit {
#define X(name, b) GUIEVENT_##name##_BIT = (1 << GUIEVENT_##name),
	GUIEEVENTTYPE_LIST
#undef X
};

struct GUIEventHandler_vtbl {
#define X(name, b) GUI_EventHandlerFn name;
	GUIEEVENTTYPE_LIST
#undef X
};

static char GUIEventBubbleBehavior[] = {
	#define X(name, bubble) [GUIEVENT_##name] = bubble,
		GUIEEVENTTYPE_LIST
	#undef X
};

// specific keys
#define GUIMODKEY_LSHIFT (1 << 1)
#define GUIMODKEY_RSHIFT (1 << 2)
#define GUIMODKEY_LCTRL  (1 << 3)
#define GUIMODKEY_RCTRL  (1 << 4)
#define GUIMODKEY_LALT   (1 << 5)
#define GUIMODKEY_RALT   (1 << 6)
#define GUIMODKEY_LTUX   (1 << 7)
#define GUIMODKEY_RTUX   (1 << 8)
#define GUIMODKEY_MENU   (1 << 9)

// set if either L or R is pressed
#define GUIMODKEY_SHIFT  (1 << 30)
#define GUIMODKEY_CTRL   (1 << 29)
#define GUIMODKEY_ALT    (1 << 28)
#define GUIMODKEY_TUX    (1 << 27)


typedef struct GUIEvent {
	enum GUIEventType type;
	
	double eventTime;
	Vector2 eventPos;
	GUIObject* originalTarget;
	GUIObject* currentTarget;
	
	union {
		Vector2 pos; // for mouse events; absolute position
		Vector2 size; // for window size
	};
	
	union {
		int character; // for kb events
		int button; // for mouse events
	};
	union {
		Vector2 dragStartPos;
		int keycode;
	};
	
	unsigned int modifiers;
	
	char multiClick;
	char cancelled;
	char requestRedraw;
} GUIEvent;


typedef int  (*GUI_OnClickFn)(GUIObject* go, Vector2 clickPos);
typedef void (*GUI_OnMouseEnterFn)(GUIEvent* e);
typedef void (*GUI_OnMouseLeaveFn)(GUIEvent* e);


#define GUIMOUSECURSOR_ARROW 0x01
#define GUIMOUSECURSOR_TEXT  0x02
#define GUIMOUSECURSOR_WAIT  0x03


typedef struct GUIHeader {
	struct GUIManager* gm;
	GUIObject* parent;
	struct gui_vtbl* vt;
	struct GUIEventHandler_vtbl* event_vt;
	char* name;

	// fallback for easy hit testing
	VEC(union GUIObject*) children;
	
	Vector2 topleft; // relative to parent (and window padding)
	Vector2 size; // absolute
	float scale;
	float alpha;
	float z; // relative to the parent
	
	// calculated absolute coordinates of the top left corner
	// updated every frame before any rendering or hit testing
	Vector2 absTopLeft; 
	// calculated tl coords relative to the parent
	Vector2 relTopLeft; 
	// calculated absolute clipping box. this element may be entirely clipped.
	AABB2 absClip;
	// calculated absolute z index
	float absZ;
	
	
	AABB2 hitbox; // in local coordinates
	
	char hidden;
	char deleted;
	char gravity;
	
	int cursor;
	
	GUI_OnClickFn onClick;
	GUI_OnMouseEnterFn onMouseEnter;
	GUI_OnMouseLeaveFn onMouseLeave;
	
} GUIHeader;



// Animations
#include "ui/animations/pulse.h"



// GUI elements
#include "ui/window.h"
#include "ui/text.h"
#include "ui/scrollWindow.h"
#include "ui/simpleWindow.h"
#include "ui/image.h"
#include "ui/imgButton.h"
#include "ui/tree.h"
#include "ui/edit.h"
#include "ui/slider.h"
#include "ui/columnLayout.h"
#include "ui/gridLayout.h"
#include "ui/tabBar.h"
#include "ui/tabControl.h"
#include "ui/monitors.h"
#include "ui/debugAdjuster.h"
#include "ui/structAdjuster.h"
#include "ui/performanceGraph.h"



union GUIObject {
	GUIHeader h; // legacy
	GUIHeader header;
	GUIText text;
	GUIWindow window;
	GUISimpleWindow simpleWindow;
	GUIImage image;
	GUIColumnLayout columnLayout;
	GUIGridLayout gridLayout;
	GUIValueMonitor valueMonitor;
	GUIDebugAdjuster debugAdjuster;
	GUISlider Slider;
	GUITabControl TabControl;
// 	GUIScrollbar Scrollbar;
	GUIEdit Edit;
	GUIStructAdjuster structAdjuster;
	GUIImageButton ImageButton;
	GUIPerformanceGraph PerformanceGraph;
};




/*
The general idea is this:
The gui is held in a big tree. The tree is walked depth-first from the bottom up, resulting in
a mostly-sorted list. A sort is then run on z-index to ensure proper order. The list is then
rendered all at once through a single unified megashader using the geometry stage to expand
points into quads.

*/
typedef struct GUIManager {
	int maxInstances;
	PCBuffer instVB;
	GLuint vao;
	
	int elementCount;
	int elementAlloc;
	GUIUnifiedVertex* elemBuffer;
	
	Vector2i screenSize;
	
	GUIObject* root;
	VEC(GUIObject*) reapQueue; 
	
	FontManager* fm;
	TextureAtlas* ta;
	
	void (*windowTitleSetFn)(void*, char*);
	void* windowTitleSetData;
	
	void (*mouseCursorSetFn)(void*, int);
	void* mouseCursorSetData;
	
	// input 
	Vector2 lastMousePos;
	char mouseIsOutOfWindow;
	
	char isDragging;
	char isMouseDown;
	int dragButton;
	Vector2 dragStartPos;
	float dragStartTime;
	GUIObject* dragStartTarget;
	float minDragDist;
	
	struct {
		float time;
		int button;
	} clickHistory[3];
	float doubleClickTime;
	float tripleClickTime;
	
	int defaultCursor;
	int currentCursor;
	
	VEC(GUIObject*) focusStack;
	
	struct {
		GUIFont* font;
		float fontSize;
		struct Color4 textColor;
		struct Color4 windowBgColor;
		struct Color4 editBgColor;
		struct Color4 cursorColor;
		struct Color4 tabTextColor;
		struct Color4 tabBorderColor;
		struct Color4 tabActiveBgColor;
		struct Color4 tabHoverBgColor;
		struct Color4 tabBgColor;
	} defaults;
	
	
	// temp 
	GLuint fontAtlasID;
	GLuint atlasID;
	
	VEC(GLuint64) texHandles;
	
	GlobalSettings* gs;
	
} GUIManager;



void GUIManager_init(GUIManager* gm, GlobalSettings* gs);
void GUIManager_initGL(GUIManager* gm, GlobalSettings* gs);
GUIManager* GUIManager_alloc(GlobalSettings* gs);


RenderPass* GUIManager_CreateRenderPass(GUIManager* gm);
PassDrawable* GUIManager_CreateDrawable(GUIManager* gm);

void GUIManager_SetMainWindowTitle(GUIManager* gm, char* title);
void GUIManager_SetCursor(GUIManager* gm, int cursor);



void GUIManager_updatePos(GUIManager* gm, PassFrameParams* pfp);

GUIObject* GUIObject_hitTest(GUIObject* go, Vector2 testPos);
GUIObject* GUIManager_hitTest(GUIManager* gm, Vector2 testPos);
// 
// void GUIObject_triggerClick(GUIObject* go, GUIEvent* e); 
void GUIObject_triggerClick(GUIObject* go, Vector2 testPos);
GUIObject* GUIManager_triggerClick(GUIManager* gm, Vector2 testPos);

GUIObject* GUIObject_findChild(GUIObject* obj, char* childName);

// focus stack
GUIObject* GUIManager_getFocusedObject(GUIManager* gm);
#define GUIManager_pushFocusedObject(gm, o) GUIManager_pushFocusedObject_(gm, &(o)->header)
void GUIManager_pushFocusedObject_(GUIManager* gm, GUIHeader* h);
GUIObject* GUIManager_popFocusedObject(GUIManager* gm);

// GUIObject* guiHitTest(GUIObject* go, Vector2 testPos);
void guiDelete(GUIObject* go);
// void guiRender(GUIObject* go, GameState* gs, PassFrameParams* pfp);
void guiReap(GUIObject* go);
void GUIResize(GUIHeader* gh, Vector2 newSz);
int guiRemoveChild(GUIObject* parent, GUIObject* child);


void guiTriggerClick(GUIEvent* e); 



#define GUIRegisterObject(o, p) GUIRegisterObject_(&(o)->header, (p) ? (&((GUIObject*)(p))->header) : NULL)
void GUIRegisterObject_(GUIHeader* o, GUIHeader* parent);


void GUIManager_TriggerEvent(GUIManager* o, GUIEvent* gev);
#define GUITriggerEvent(o, gev) GUITriggerEvent_(&(o)->header, gev)
void GUITriggerEvent_(GUIHeader* o, GUIEvent* gev);
void GUIManager_BubbleEvent(GUIManager* gm, GUIObject* target, GUIEvent* gev);

void GUIManager_HandleMouseMove(GUIManager* gm, InputState* is, InputEvent* iev);
void GUIManager_HandleMouseClick(GUIManager* gm, InputState* is, InputEvent* iev);
void GUIManager_HandleKeyInput(GUIManager* gm, InputState* is, InputEvent* iev);



void guiSetClientSize(GUIObject* go, Vector2 cSize);
Vector2 guiGetClientSize(GUIObject* go);
Vector2 guiRecalcClientSize(GUIObject* go);
void guiAddClient(GUIObject* parent, GUIObject* child);
void guiRemoveClient(GUIObject* parent, GUIObject* child);





#include "ui/configLoader.h"





#endif // __EACSMB_GUI_H__
