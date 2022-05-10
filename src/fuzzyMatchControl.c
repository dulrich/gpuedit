
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>


//#include <sys/types.h> // opendir
//#include <dirent.h> // readdir
//#include <unistd.h> // pathconf
//#include <sys/stat.h>




#include "fuzzyMatch.h"
#include "fuzzyMatchControl.h"
#include "app.h" // for execProcess*

#include "ui/gui_internal.h"



// #define DEBUG printf
#define DEBUG(...) 


#include "ui/macros_on.h"

void GUIFuzzyMatchControl_Render(GUIFuzzyMatchControl* w, GUIManager* gm, Vector2 tl, Vector2 sz, PassFrameParams* pfp) {

	gm->curZ += 10;
	if(GUI_Edit(&w->searchTerm, tl, V(sz.x, 20), &w->searchTerm)) {
		GUIFuzzyMatchControl_Refresh(w);
	}
	gm->curZ -= 10;


	if(GUI_InputAvailable()) {
		GUI_Cmd* cmd = Commands_ProbeCommand(gm, GUIELEMENT_FuzzyMatcher, &gm->curEvent);
		
		if(cmd) {
			GUIFuzzyMatchControl_ProcessCommand(w, cmd);
			GUI_CancelInput();
		}
	}


	if(!gm->drawMode) return;


	// draw general background
	GUI_Rect(tl, sz, &gm->defaults.windowBgColor);
	
	// 	drawTextLine();
	float lh = w->lineHeight;
	float gutter = w->leftMargin + 20;
	
	int linesDrawn = 0;
	
	gm->curZ++;
	
	for(intptr_t i = 0; w->matches && i < w->matchCnt; i++) {
		DEBUG("rendering match: %ld\n", i);
	
		if(lh * linesDrawn > sz.y) break; // stop at the bottom of the window
		
		Vector2 btl = {tl.x + gutter, tl.y + 20 + (lh * linesDrawn)};
		Vector2 bsz = {sz.x - gutter, (lh)};
		
		if(w->cursorIndex == i) { // backgrounds for selected items
			struct Color4* color = &gm->defaults.selectedItemBgColor;
			GUI_Rect(btl, bsz, color);
		}

		gm->curZ++;
		// the file name
		GUI_TextLine(w->matches[i].filepath, strlen(w->matches[i].filepath), btl, "Arial",  16, &gm->defaults.selectedItemTextColor);
		gm->curZ--;
		
		linesDrawn++;
	}

}

#include "ui/macros_off.h"




/*

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



*/

void GUIFuzzyMatchControl_ProcessCommand(GUIFuzzyMatchControl* w, GUI_Cmd* cmd) {
	long amt;

	switch(cmd->cmd) {
		case GUICMD_FuzzyMatcher_Exit:
//			GUIManager_BubbleUserEvent(w->header.gm, &w->header, "closeMe");			
			break;
			
		case GUICMD_FuzzyMatcher_CursorMove:
			if(w->matchCnt == 0) break;
			w->cursorIndex = (cmd->amt + w->cursorIndex + w->matchCnt) % w->matchCnt;
			break;
			
		case GUICMD_FuzzyMatcher_OpenFile: {
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
			
			MessagePipe_Send(w->upstream, MSG_OpenFileOpt, &opt, NULL);

			free(path_raw);
			free(path);

			if(w->gs->MainControl_openInPlace) {
				MessagePipe_Send(w->upstream, MSG_CloseMe, w, NULL);
			}

			break;
		}
		
	}
	
}


GUIFuzzyMatchControl* GUIFuzzyMatchControl_New(GUIManager* gm, MessagePipe* mp, char* path, char* searchTerm) {

	
	
	GUIFuzzyMatchControl* w = pcalloc(w);
	w->upstream = mp;

//	w->header.flags = GUI_MAXIMIZE_X | GUI_MAXIMIZE_Y;
//	w->header.cmdElementType = CUSTOM_ELEM_TYPE_FuzzyMatcher;
	
	w->lineHeight = 25;
	w->leftMargin = 20;
	
	if(searchTerm) {
//		w->searchTerm = strdup(searchTerm);
//		w->searchBox = GUIEdit_New(gm, searchTerm);
	} else {
//		w->searchBox = GUIEdit_New(gm, "");
	}
	
	
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
	
	if(!w->searchTerm.data) {
		goto CLEANUP;
	}
	
	while(w->gs->MainControl_searchPaths[n_paths]) {
		n_paths++;
	}
	if(n_paths == 0) return;

	candidates = malloc(max_candidates * sizeof(*candidates));
	contents = malloc(sizeof(*contents) * (n_paths + 1));
	stringBuffers = malloc(sizeof(*stringBuffers) * (n_paths + 1));
	
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
		
		for(j = 0; j < n_filepaths; j++) {
			DEBUG("got filepath: %s\n", stringBuffers[i][j]);
			candidates[n_candidates + j].basepath = w->gs->MainControl_searchPaths[i];
			candidates[n_candidates + j].filepath = stringBuffers[i][j];
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

	char* input = w->searchTerm.data;

	fcandidate* matches = NULL;
	int n_matches = 0;
	int err = -1;

	if(input) {
		err = fuzzy_match_fcandidate(candidates, n_candidates, &matches, &n_matches, input, 0);
		DEBUG("fuzzy match exit code: %d\n", err);
	}

	if(w->matches) free(w->matches);
	if(!err) {
		w->matches = matches;
		w->matchCnt = n_matches;
		DEBUG("match count at end: %ld\n", w->matchCnt);
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

