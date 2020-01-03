#ifndef __EACSMB_WINDOW_H__
#define __EACSMB_WINDOW_H__

#include <X11/X.h>
#include <X11/Xlib.h>


#include "common_gl.h"
#include "input.h"


typedef struct XStuff {
	Display*     display;
	Window       rootWin;
	Window       clientWin;
	GLXContext   glctx;
	
	XVisualInfo* vi;
	Colormap     colorMap;
	
	XWindowAttributes winAttr;
	
	Cursor noCursor;
	Cursor arrowCursor;
	Cursor textCursor;
	Cursor waitCursor;
	
	Vector2i winSize;
	
	void (*onExpose)(struct XStuff*, void*);
	void* onExposeData;

	void (*onResize)(struct XStuff*, void*);
	void* onResizeData;

	int targetMSAA;
	char* windowTitle;
	
	int XFixes_eventBase;
	int XFixes_errorBase;
	
	Atom clipboardID;
	Atom primaryID;
	Atom secondaryID;
	Atom selDataID;
	Atom utf8ID;
	Atom textID;
	Atom targetsID;
	
	
	Bool ready;
} XStuff;



void XStuff_hideCursor(XStuff* xs);
void XStuff_showCursor(XStuff* xs);

XErrorEvent* xLastError;
char xLastErrorStr[1024];
 
int initXWindow(XStuff* xs);

int processEvents(XStuff* xs, InputState* st, InputEvent* iev, int max_events);



#endif // __EACSMB_WINDOW_H__
