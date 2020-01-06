
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <math.h>
#include <time.h>


#include <unistd.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include <GL/glew.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include "c3dlas/c3dlas.h"
#include "c_json/json.h"
#include "json_gl.h"

#include "sti/sti.h"

#include "utilities.h"
#include "config.h"
#include "shader.h"
#include "texture.h"
#include "window.h"
#include "app.h"
#include "gui.h"

// temp
#include "highlighters/c.h"


GLuint proj_ul, view_ul, model_ul;




RenderPipeline* rpipe;


// in renderLoop.c, a temporary factoring before a proper renderer is designed
void drawFrame(XStuff* xs, AppState* as, InputState* is);
void setupFBOs(AppState* as, int resized);


// MapBlock* map;
// TerrainBlock* terrain;

void resize_callback(XStuff* xs, void* gm_) {
	GUIManager* gm = (GUIManager*)gm_;
	
	GUIEvent gev = {
		.type = GUIEVENT_ParentResize,
		.size = {.x = xs->winSize.x, .y = xs->winSize.y},
		.originalTarget = gm->root,
	};
	
	GUITriggerEvent(gm->root, &gev);
}


// nothing in here can use opengl at all.
void initApp(XStuff* xs, AppState* as, int argc, char* argv[]) {
	
	srand((unsigned int)time(NULL));
	
	
	
	
	// this costs 5mb of ram
// 	json_gl_init_lookup();
	
	
// 	TextureAtlas* ta = TextureAtlas_alloc();
// 	ta->width = 256;
// 	TextureAtlas_addFolder(ta, "pre", "assets/ui/icons", 0);
// 	TextureAtlas_finalize(ta);
// 	
	
	/*
	Highlighter* ch = pcalloc(ch);
	initCStyles(ch);
	Highlighter_PrintStyles(ch);
	Highlighter_LoadStyles(ch, "config/c_colors.txt");
	*/
	
	as->gui = GUIManager_alloc(&as->globalSettings);
	xs->onResize = resize_callback;
	xs->onResizeData = as->gui;
	as->gui->defaults.tabBorderColor = (struct Color4){120,120,120,255};
	as->gui->defaults.tabActiveBgColor = (struct Color4){80,80,80,255};
	as->gui->defaults.tabBgColor = (struct Color4){10,10,10,255};
	as->gui->defaults.tabTextColor = (struct Color4){200,200,200,255};
	
	as->gui->windowTitleSetFn = XStuff_SetWindowTitle;
	as->gui->windowTitleSetData = xs;
	
	as->gui->mouseCursorSetFn = XStuff_SetMouseCursor;
	as->gui->mouseCursorSetData = xs;
	
	as->mc = GUIMainControl_New(as->gui, &as->globalSettings);
	GUIRegisterObject(as->mc, as->gui->root);

	
	
	// look for files to load in arguments
	for(int i = 1; i < argc; i++) {
		char* a = argv[i];
		
		// -f works too
		if(a[0] == '-') {
			if(a[1] == 'f' && a[2] == NULL) {
				i++;
				if(i < argc) {
					GUIMainControl_LoadFile(as->mc, argv[i]);
				}
			}
			
			continue;
		}
		
		GUIMainControl_LoadFile(as->mc, a);
	}
	
	
	// for debugging
	GUIMainControl_LoadFile(as->mc, "src/buffer.h");
	GUIMainControl_LoadFile(as->mc, "src/buffer.c");
	GUIMainControl_LoadFile(as->mc, "src/bufferEditor.c");
	
	GUIManager_pushFocusedObject(as->gui, as->mc);
	
	
	
	
	
	as->frameCount = 0;
	
	as->debugMode = 0;
	
	int ww, wh;
	ww = xs->winAttr.width;
	wh = xs->winAttr.height;
	
	as->screen.wh.x = (float)ww;
	as->screen.wh.y = (float)wh;
	as->gui->screenSize = (Vector2i){ww, wh};
	
	as->screen.aspect = as->screen.wh.x / as->screen.wh.y;
	as->screen.resized = 0;
	

	// set up matrix stacks
	MatrixStack* view, *proj;
	
	view = &as->view;
	proj = &as->proj;
	
	msAlloc(2, view);
	msAlloc(2, proj);

	msIdent(view);
	msIdent(proj);

}

void initAppGL(XStuff* xs, AppState* as) {
	
	
	glerr("left over error on app init");
	
	
	GUIManager_initGL(as->gui, &as->globalSettings);
	as->guiPass = GUIManager_CreateRenderPass(as->gui);
	

	initRenderLoop(as);
	initRenderPipeline();
	

	/*
	getPrintGLEnum(GL_MAX_COLOR_ATTACHMENTS, "meh");
	getPrintGLEnum(GL_MAX_DRAW_BUFFERS, "meh");
	getPrintGLEnum(GL_MAX_FRAMEBUFFER_WIDTH, "meh");
	getPrintGLEnum(GL_MAX_FRAMEBUFFER_HEIGHT, "meh");
	getPrintGLEnum(GL_MAX_FRAMEBUFFER_LAYERS, "meh");
	getPrintGLEnum(GL_MAX_FRAMEBUFFER_SAMPLES, "meh");
	getPrintGLEnum(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, "meh");
	getPrintGLEnum(GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS, "meh");
	getPrintGLEnum(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, "meh");
	getPrintGLEnum(GL_MAX_TEXTURE_IMAGE_UNITS, "meh");
	getPrintGLEnum(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, "meh");
	getPrintGLEnum(GL_MAX_ARRAY_TEXTURE_LAYERS, "meh");
	getPrintGLEnum(GL_MAX_SAMPLES, "meh");
	getPrintGLEnum(GL_MAX_VERTEX_ATTRIBS, "meh");
	getPrintGLEnum(GL_MIN_PROGRAM_TEXEL_OFFSET, "meh");
	getPrintGLEnum(GL_MAX_PROGRAM_TEXEL_OFFSET, "meh");
	getPrintGLEnum(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, "meh");
	getPrintGLEnum(GL_MAX_UNIFORM_BLOCK_SIZE, "meh");
	getPrintGLEnum(GL_MAX_TEXTURE_SIZE, "meh");
	
	*/
	
	
	
	initTextures();

	
	
	
/*	
	json_file_t* guijsf;
	
	guijsf = json_load_path("assets/config/main_ui.json");
	json_value_t* kids;
	json_obj_get_key(guijsf->root, "children", &kids);
	
	GUICL_LoadChildren(as->gui, as->gui->root, kids);
	
	GUIObject* ps = GUIObject_findChild(as->gui->root, "perfstats");
	gt_terrain = GUIObject_findChild(ps, "terrain");
	gt_solids = GUIObject_findChild(ps, "solids");
	gt_selection = GUIObject_findChild(ps, "selection");
	gt_decals = GUIObject_findChild(ps, "decals");
	gt_emitters = GUIObject_findChild(ps, "emitters");
	gt_effects = GUIObject_findChild(ps, "effects");
	gt_lighting = GUIObject_findChild(ps, "lighting");
	gt_sunShadow = GUIObject_findChild(ps, "sunShadow");
	gt_shading = GUIObject_findChild(ps, "shading");
	gt_gui = GUIObject_findChild(ps, "gui");
	
	
*/

		

}



void preFrame(AppState* as) {

	// update timers
	char frameCounterBuf[128];
	
	static int frameCounter = 0;
	static double last_frame = 0;
	static double lastPoint = 0;
	
	double now;
	
	as->frameTime = now = getCurrentTime();
	
	if (last_frame == 0)
		last_frame = now;
	
	as->frameSpan = (double)(now - last_frame);
	last_frame = now;
	
	frameCounter = (frameCounter + 1) % 60;
	
	
	
	static double sdtime, sseltime, semittime;
	
	if(lastPoint == 0.0f) lastPoint = as->frameTime;
	if(1 /*frameCounter == 0*/) {
		float fps = 60.0f / (as->frameTime - lastPoint);
		
		uint64_t qtime;

#define query_update_gui(qname)		\
		if(!query_queue_try_result(&as->queries.qname, &qtime)) {\
			sdtime = ((double)qtime) / 1000000.0;\
		}\
		snprintf(frameCounterBuf, 128, #qname ":  %.2fms", sdtime);\
		GUIText_setString(gt_##qname, frameCounterBuf);


		//query_update_gui(gui);
		
		lastPoint = now;
	}
	
	

	
}




void postFrame(AppState* as) {
	
	double now;
	
	now = getCurrentTime();
	
	as->perfTimes.draw = now - as->frameTime;
	
}







Vector2i viewWH = {
	.x = 0,
	.y = 0
};
void checkResize(XStuff* xs, AppState* as) {
	if(viewWH.x != xs->winAttr.width || viewWH.y != xs->winAttr.height) {
		
		// TODO: destroy all the textures too
		
		//printf("screen 0 resized\n");
		
		viewWH.x = xs->winAttr.width;
		viewWH.y = xs->winAttr.height;
		
		as->screen.wh.x = (float)xs->winAttr.width;
		as->screen.wh.y = (float)xs->winAttr.height;
		as->gui->screenSize = (Vector2i){xs->winAttr.width, xs->winAttr.height};
		
		as->screen.aspect = as->screen.wh.x / as->screen.wh.y;
		
		as->screen.resized = 1;
		
	}
}



void handleEvent(AppState* as, InputState* is, InputEvent* ev) {
// 	printf("%d %c/* */%d-\n", ev->type, ev->character, ev->keysym);
	
	if(ev->type == EVENT_KEYUP && is->keyState[64]) {
		if(ev->keysym == XK_Right) {
			GUIObject* o = GUIMainControl_NextTab(as->mc, 1);
			GUIManager_popFocusedObject(as->gui);
			GUIManager_pushFocusedObject(as->gui, o);
			return;
		}
		else if(ev->keysym == XK_Left) {
			GUIObject* o = GUIMainControl_PrevTab(as->mc, 1);
			GUIManager_popFocusedObject(as->gui);
			GUIManager_pushFocusedObject(as->gui, o);
			return;
		}
	}
	
	
	switch(ev->type) {
		case EVENT_KEYUP:
		case EVENT_KEYDOWN:
			GUIManager_HandleKeyInput(as->gui, is, ev);
			break;
		case EVENT_MOUSEUP:
		case EVENT_MOUSEDOWN:
			GUIManager_HandleMouseClick(as->gui, is, ev);
			break;
		case EVENT_MOUSEMOVE:
			GUIManager_HandleMouseMove(as->gui, is, ev);
			break;
	}
}


void prefilterEvent(AppState* as, InputState* is, InputEvent* ev) {
	// drags, etc
	
	// TODO: fix; passthrough atm
	handleEvent(as, is, ev);
	
}




#define PF_START(x) as->perfTimes.x = getCurrentTime()
#define PF_STOP(x) as->perfTimes.x = timeSince(as->perfTimes.x)

void appLoop(XStuff* xs, AppState* as, InputState* is) {
	
	// main running loop
	while(1) {
		InputEvent iev;
		
		for(int i = 0; i < 1000; i++) {
			int drawRequired = 0;
			if(processEvents(xs, is, &iev, -1)) {
	// 			// handle the event
				prefilterEvent(as, is, &iev);
			}
			
			
			if(drawRequired) break;
		}
		
		
		checkResize(xs, as);
		
// 		double now = getCurrentTime();
		preFrame(as); // updates timers
		
		drawFrame(xs, as, is);
		
		as->screen.resized = 0;
		
		postFrame(as); // finishes frame-draw timer
// 		printf("frame time: %fms\n", timeSince(now) * 1000.0);
		
		if(as->frameSpan < 1.0/60.0) {
			// shitty estimation based on my machine's heuristics, needs improvement
			float sleeptime = (((1.0/60.0) * 1000000) - (as->frameSpan * 1000000)) * 1.7;
			//printf("sleeptime: %f\n", sleeptime / 1000000);
			//sleeptime = 1000;
			if(sleeptime > 0) usleep(sleeptime); // problem... something is wrong in the math
		}
// 		sleep(1);
	}
}

