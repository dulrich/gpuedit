
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>


//#include <sys/types.h> // opendir
//#include <dirent.h> // readdir
//#include <unistd.h> // pathconf
//#include <sys/stat.h>


#include "ui/gui.h"
#include "ui/gui_internal.h"

#include "fuzzyMatch.h"
#include "fuzzyMatchControl.h"
#include "app.h" // for execProcess*


// #define DEBUG printf
#define DEBUG(...)


static void render(GUIHeader* w_, PassFrameParams* pfp) {
	GUIFuzzyMatchControl* w = (GUIFuzzyMatchControl*)w_;
	GUIManager* gm = w->header.gm;
	GUIUnifiedVertex* v;

	Vector2 tl = w->header.absTopLeft;

	// draw general background
	v = GUIManager_reserveElements(gm, 1);
	*v = (GUIUnifiedVertex){
		.pos = {
			tl.x,
			tl.y,
			tl.x + w->header.size.x,
			tl.y + w->header.size.y
		},
		.clip = GUI_AABB2_TO_SHADER(w->header.absClip),
		.guiType = 0, // window (just a box)
		.fg = {0, 0, 255, 255},
		.bg = GUI_COLOR4_TO_SHADER(gm->defaults.windowBgColor),
		.z = w->header.absZ,
		.alpha = 1,
	};

	
	// 	drawTextLine();
	float lh = w->lineHeight;
	float gutter = w->leftMargin + 20;
	
	int linesDrawn = 0;
	
	for(intptr_t i = 0; w->matches && i < w->matchCnt; i++) {
		if(lh * linesDrawn > w->header.size.y) break; // stop at the bottom of the window

		AABB2 box;
		box.min.x = tl.x + gutter;
		box.min.y = tl.y + 30 + (lh * linesDrawn);
		box.max.x = tl.x + w_->size.x - gutter;
		box.max.y = tl.y + 30 + (lh * (linesDrawn + 1));


		if(w->cursorIndex == i) { // backgrounds for selected items
			struct Color4* color = &gm->defaults.selectedItemBgColor;
			
			v = GUIManager_reserveElements(gm, 1);
			*v = (GUIUnifiedVertex){
				.pos = {box.min.x, box.min.y, box.max.x, box.max.y},
				.clip = GUI_AABB2_TO_SHADER(w->header.absClip),
				
				.guiType = 0, // window (just a box)
				
				.fg = GUI_COLOR4_TO_SHADER(*color),
				.bg = GUI_COLOR4_TO_SHADER(*color),
				
				.z = w->header.absZ,
				.alpha = 1,
			};
		}



		// the file name
		gui_drawTextLine(gm, (Vector2){box.min.x, box.min.y}, (Vector2){box.max.x - box.min.x,0}, &w->header.absClip, &gm->defaults.selectedItemTextColor , 10000000, w->matches[i].filepath, strlen(w->matches[i].filepath));
		
		linesDrawn++;
	}


	
	GUIHeader_renderChildren(&w->header, pfp);
}



static void updatePos(GUIHeader* w_, GUIRenderParams* grp, PassFrameParams* pfp) {
	GUIFuzzyMatchControl* w = (GUIFuzzyMatchControl*)w_;
	
	w->searchBox->header.topleft.y = 0;
	w->searchBox->header.topleft.x = 0;
	w->searchBox->header.size.y = 25;


	gui_defaultUpdatePos(&w->header, grp, pfp);
}



static void userEvent(GUIHeader* w_, GUIEvent* gev) {
	GUIFuzzyMatchControl* w = (GUIFuzzyMatchControl*)w_;
	
	if((GUIEdit*)gev->originalTarget == w->searchBox) {
		if(0 == strcmp(gev->userType, "change")) {
			w->cursorIndex = 0;
						
			if(w->searchTerm) free(w->searchTerm);
			w->searchTerm = strndup(gev->userData, gev->userSize);
			
			GUIFuzzyMatchControl_Refresh(w);
		}
	}
}



static void keyDown(GUIHeader* w_, GUIEvent* gev) {
	GUIFuzzyMatchControl* w = (GUIFuzzyMatchControl*)w_;
/*
	Cmd found;
	unsigned int iter = 0;
	while(Commands_ProbeCommand(gev, w->commands, 0, &found, &iter)) {
		// GUIBufferEditor will pass on commands to the buffer
		GUIFuzzyMatchControl_ProcessCommand(w, &found);		
		
	}
*/
}


static void gainedFocus(GUIHeader* w_, GUIEvent* gev) {
	GUIFuzzyMatchControl* w = (GUIFuzzyMatchControl*)w_;
	
	GUIManager_pushFocusedObject(w->header.gm, &w->searchBox->header);
}



static void handleCommand(GUIHeader* w_, GUI_Cmd* cmd) {
	GUIFuzzyMatchControl* w = (GUIFuzzyMatchControl*)w_;
	int needRehighlight = 0;
	
	GUIFuzzyMatchControl_ProcessCommand(w, cmd);
}


void GUIFuzzyMatchControl_ProcessCommand(GUIFuzzyMatchControl* w, GUI_Cmd* cmd) {
	long amt;

	switch(cmd->cmd) {
		case FuzzyMatcherCmd_Exit:
			GUIManager_BubbleUserEvent(w->header.gm, &w->header, "closeMe");			
			break;
			
		case FuzzyMatcherCmd_CursorMove:
			if(w->matchCnt == 0) break;
			w->cursorIndex = (cmd->amt + w->cursorIndex + w->matchCnt) % w->matchCnt;
			break;
			
		case FuzzyMatcherCmd_OpenFile: {
			if(w->matchCnt == 0) break;
			
			char* path_raw = path_join(w->matches[w->cursorIndex].basepath, w->matches[w->cursorIndex].filepath);
			char* path = resolve_path(path_raw);
			GUIFileOpt opt = {
				.path = path,
				.line_num = 1,
				.set_focus = 0,
			};
			if(w->gs->MainControl_openInPlace) {
				opt.set_focus = 1;
			}
		
			GUIEvent gev2 = {};
			gev2.type = GUIEVENT_User;
			gev2.eventTime = 0;//gev->eventTime;
			gev2.originalTarget = &w->header;
			gev2.currentTarget = &w->header;
			gev2.cancelled = 0;
			// handlers are responsible for cleanup
			gev2.userData = &opt;
			gev2.userSize = sizeof(opt);
			
			gev2.userType = "openFileOpt";
		
			GUIManager_BubbleEvent(w->header.gm, &w->header, &gev2);
			
			free(path_raw);
			free(path);

			if(w->gs->MainControl_openInPlace) {
				GUIManager_BubbleUserEvent(w->header.gm, &w->header, "closeMe");
			}

			break;
		}
		
	}
	
}


GUIFuzzyMatchControl* GUIFuzzyMatchControl_New(GUIManager* gm, char* path, char* searchTerm) {

	static struct gui_vtbl static_vt = {
		.Render = (void*)render,
		.UpdatePos = (void*)updatePos,
		.HandleCommand = (void*)handleCommand,
	};
	
	static struct GUIEventHandler_vtbl event_vt = {
		.KeyDown = keyDown,
		.GainedFocus = gainedFocus,
		//.Click = click,
		//.DoubleClick = click,
// 		.ScrollUp = scrollUp,
// 		.ScrollDown = scrollDown,
// 		.DragStart = dragStart,
// 		.DragStop = dragStop,
// 		.DragMove = dragMove,
// 		.ParentResize = parentResize,
		.User = userEvent,
	};
	
	
	GUIFuzzyMatchControl* w = pcalloc(w);
	
	gui_headerInit(&w->header, gm, &static_vt, &event_vt);
	w->header.cursor = GUIMOUSECURSOR_ARROW;
	w->header.flags = GUI_MAXIMIZE_X | GUI_MAXIMIZE_Y;
	w->header.cmdElementType = CUSTOM_ELEM_TYPE_FuzzyMatcher;
	
	w->lineHeight = 25;
	w->leftMargin = 20;
	
	if(searchTerm) {
		w->searchTerm = strdup(searchTerm);
		w->searchBox = GUIEdit_New(gm, searchTerm);
	} else {
		w->searchBox = GUIEdit_New(gm, "");
	}
	w->searchBox->header.flags |= GUI_MAXIMIZE_X;
	w->searchBox->header.gravity = GUI_GRAV_TOP_LEFT;
	
	GUI_RegisterObject(w, w->searchBox);
	
	return w;
}

void GUIFuzzyMatchControl_Refresh(GUIFuzzyMatchControl* w) {
	//printf("seearch term: '%s'\n", w->searchTerm);
	size_t max_candidates = 1024;
	fcandidate* candidates = NULL;
	size_t n_candidates = 0;

	size_t n_filepaths = 0;
	char** contents = NULL;
	char*** stringBuffers = NULL;
	
	
	DEBUG("~~ begin fuzzy opener\n");
	char* cmd = "/usr/bin/git";
	char* args[] = {cmd, "-C", NULL, "ls-files", "-co", "--exclude-standard", NULL};
	
	int i = 0;
	int j = 0;
	int n_paths = 0;
	
	if(!w->searchTerm) {
		goto CLEANUP;
	}
	
	while(w->gs->MainControl_searchPaths[n_paths]) {
		n_paths++;
	}
	if(n_paths == 0) return;

	candidates = malloc(max_candidates*sizeof(*candidates));
	contents = malloc(sizeof(*contents)*(n_paths+1));
	stringBuffers = malloc(sizeof(*stringBuffers)*(n_paths+1));
	
	i = 0;
	while(w->gs->MainControl_searchPaths[i]) {
		args[2] = w->gs->MainControl_searchPaths[i];
		contents[i] = execProcessPipe_charpp(args, &stringBuffers[i], &n_filepaths);
		DEBUG("result: %ld filepaths\n", n_filepaths);

		if(n_candidates+n_filepaths >= max_candidates) {
			DEBUG("fps: %lu, mc: %lu\n", n_filepaths, max_candidates);
			max_candidates = 2*MAX(n_filepaths, max_candidates);
			DEBUG("mc: %lu, size: %lu\n", max_candidates, sizeof(*candidates));
			candidates = realloc(candidates, max_candidates*sizeof(*candidates));
		}
		for(j=0;j<n_filepaths;j++) {
			DEBUG("got filepath: %s\n", stringBuffers[i][j]);
			candidates[n_candidates+j].basepath = w->gs->MainControl_searchPaths[i];
			candidates[n_candidates+j].filepath = stringBuffers[i][j];
		}

		i++;
		n_candidates += n_filepaths;
	}
	contents[i] = NULL;
	stringBuffers[i] = NULL;

CLEANUP:
	if(w->stringBuffers) {
		i = 0;
		while(w->stringBuffers[i]) {
			free(w->stringBuffers[i]);
			i++;
		}
		free(w->stringBuffers);
	}
	w->stringBuffers = stringBuffers;

	if(w->contents) {
		i = 0;
		while(w->contents[i]) {
			free(w->contents[i]);
			i++;
		}
		free(w->contents);
	}
	w->contents = contents;

	if(w->candidates) {
		free(w->candidates);
	}
	w->candidates = candidates;

	char* input = w->searchTerm;

	fcandidate* matches = NULL;
	int n_matches = 0;
	int err = -1;

	if(input) {
		err = fuzzy_match_fcandidate(candidates, n_candidates, &matches, &n_matches, input, 0);
		// printf("fuzzy match exit code: %d\n", err);
	}

	if(w->matches) free(w->matches);
	if(!err) {
		w->matches = matches;
		w->matchCnt = n_matches;
		
	//	for(i=0;i<n_matches;i++) {
	//		printf("ordered match [%s]\n", matches[i]);
	//	}
	}
	else {
		w->matches = NULL;
		w->matchCnt = 0;
	}
	
	
	//free(matches);
	// free(filepaths);

}

#undef DEBUG

