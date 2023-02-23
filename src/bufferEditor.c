#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "common_math.h"


#include "buffer.h"
#include "commands.h"
#include "ui/gui.h"
#include "ui/gui_internal.h"
#include "clipboard.h"





extern int g_DisableSave;


#include "ui/macros_on.h"

void GUIBufferEditor_Render(GUIBufferEditor* w, GUIManager* gm, Vector2 tl, Vector2 sz, PassFrameParams* pfp) {
	// GUI_BeginWindow()
	float top = 0;
	w->trayHeight = 60;
	
	if(gm->activeID == w) {
		ACTIVE(w->ec);
	}
	
	if(w->gotoLineTrayOpen) top += 40;
	
	GUI_BeginWindow(w, tl, sz, gm->curZ, 0);
	
	float sbh = w->statusBarHeight * w->showStatusBar;
	Vector2 ecsz = V(sz.x, sz.y - sbh - w->trayOpen * 60 - top);
		

	if(w->showStatusBar) {
		StatusBar_Render(w->statusBar, gm, V(0, sz.y - sbh), V(sz.x, sbh), pfp);
	}
	
	if(w->gotoLineTrayOpen) {
		gm->curZ += 20;
		GUI_Rect(V(0, 0), V(sz.x, 40), &gm->defaults.trayBgColor);
		
		DEFAULTS(GUIEditOpts, eo);
		
		ACTIVE(&w->gotoLineNum);
		if(GUI_IntEdit_(gm, &w->gotoLineNum, V(10,10), sz.x - 20, &w->gotoLineNum, &eo)) {
			BufferLine* bl = Buffer_raw_GetLineByNum(w->ec->b, w->gotoLineNum);
				
			if(bl) {
				GBEC_MoveCursorTo(w->ec, bl, 0);
				GBEC_scrollToCursorOpt(w->ec, 1);
			}
		
		}
		gm->curZ -= 20;
		
//		if(gm->activeID == &w->gotoLineNum) GUI_CancelInput();
	}
	
	
	if(w->trayOpen) {
		gm->curZ += 10;
		int update = 0;
		
		GUI_Rect(V(0, sz.y - sbh - 60), V(sz.x, 60), &gm->defaults.trayBgColor);
		
		DEFAULTS(GUIEditOpts, eopts);
		eopts.selectAll = 1;
		if(GUI_Edit_(gm, &w->findQuery, V(10, sz.y - sbh - 55), sz.x - 10 - 200, &w->findQuery, &eopts)){
			update = 1;
		}
		if(GUI_Edit_(gm, &w->replaceText, V(10, sz.y - sbh - 25), sz.x - 10 - 200, &w->replaceText, &eopts)) {
			
		}
		

		
		DEFAULTS(GUIButtonOpts, bo)
		bo.size = V(90, 20);
		bo.fontSize = 12;
		
		if(GUI_Button_(gm, ID(&w->findQuery)+1, V(sz.x - 200 + 10, sz.y - sbh - 55), "Find Next", &bo)) {
			update = 1;
		}
		if(GUI_Button_(gm, ID(&w->replaceText)+1, V(sz.x - 200 + 10, sz.y - sbh - 25), "Replace", &bo)) {
		
		}
		if(GUI_Button_(gm, ID(&w->replaceText)+2, V(sz.x - 200 + 10 + 95, sz.y - sbh - 25), "Replace All", &bo)) {
		
		}

		gm->curZ -= 10;
		
		
		if(update) {
			GUIBufferEditor_StopFind(w);
				
				//  w->findQuery.data, &w->find_opt
			BufferFindState_FindAll(w->findState); // needs a _create?
			w->ec->findSet = w->findState->findSet;
			
			GUIBufferEditor_RelativeFindMatch(w, 0, 1, w->findState);
			GUIBufferEditor_scrollToCursor(w);
		}
		
		if(!gm->drawMode && 
			gm->curEvent.type == GUIEVENT_KeyDown && gm->curEvent.modifiers == 0 && 
			(gm->curEvent.keycode == XK_ISO_Left_Tab || gm->curEvent.keycode == XK_Tab)
		) {
			if(gm->activeID == &w->findQuery) {
				ACTIVE(&w->replaceText);
				GUI_CancelInput();
			}
			else if(gm->activeID == &w->replaceText) {
				ACTIVE(&w->findQuery);
				GUI_CancelInput();
			}
		}
	}
	
	if(w->saveTrayOpen) {
		// render the save tray
		// above the find tray
	}
	
	// forward activeness to the edit control
	if(gm->activeID == w) ACTIVE(w->ec);

	GUI_PushClip(V(0,top), ecsz);
	GBEC_Render(w->ec, gm, V(0,top), ecsz, pfp);
	GUI_PopClip();
	
	// command processing
	if(!gm->drawMode && GUI_InputAvailable()) {
		
		int mode = w->inputMode;
		
		
		uint64_t overlays = w->overlays;
		int foundCommand = 0; 
		
		for(int i = 0; i < 63 && !foundCommand; i++) {
			if(!((1ul << i) & overlays)) continue;
			
			GUI_CmdModeInfo* cmi = Commands_GetOverlay(gm, i);
			if(!cmi) continue;
			
			size_t numCmds;
			
			GUI_Cmd* cmd = Commands_ProbeCommand(gm, GUIELEMENT_Buffer, &gm->curEvent, cmi->id, &numCmds);
			int needRehighlight = 0;
			for(;cmd && numCmds > 0; numCmds--, cmd++) { 
				if(!GUIBufferEditor_ProcessCommand(w, cmd, &needRehighlight)) {
					GUI_CancelInput();
					
					if(needRehighlight || cmd->flags & GUICMD_FLAG_rehighlight) {
						GUIBufferEditControl_MarkRefreshHighlight(w->ec);
					}
					
					foundCommand = 1;
				}
			}	
		}	
		
		
		if(!foundCommand) {
			size_t numCmds;
			GUI_Cmd* cmd = Commands_ProbeCommand(gm, GUIELEMENT_Buffer, &gm->curEvent, mode, &numCmds);
			int needRehighlight = 0;
			for(;cmd && numCmds > 0; numCmds--, cmd++) { 
				if(!GUIBufferEditor_ProcessCommand(w, cmd, &needRehighlight)) {
					GUI_CancelInput();
					
					if(needRehighlight || cmd->flags & GUICMD_FLAG_rehighlight) {
						GUIBufferEditControl_MarkRefreshHighlight(w->ec);
					}
				}
			}
		}
	}
	
	// --------- drawing code ---------
	if(gm->drawMode) {	
		
		if(w->ec->needsRehighlight) {
			GUIBufferEditControl_RefreshHighlight(w->ec);
			w->ec->needsRehighlight = 0;
		}
		
		// the little red recording circle
		if(w->isRecording) {
			gm->curZ += 100;
			GUI_CircleFilled(V(sz.x - 30, sz.y - (sbh) + 2), (sbh - 4), 0, C4(1,0,0,1), C4(1,0,0,1));
			gm->curZ -= 100;
		}
	}
	
	
	GUI_EndWindow();
}

#include "ui/macros_off.h"






void GUIBufferEditor_SetBuffer(GUIBufferEditor* w, Buffer* b) {
	w->b = b;
	GUIBufferEditControl_SetBuffer(w->ec, b);
}


GUIBufferEditor* GUIBufferEditor_New(GUIManager* gm, MessagePipe* tx) {

	
	GUIBufferEditor* w = pcalloc(w);
	w->gm = gm;	GUIString_Init(&w->findQuery);
	GUIString_Init(&w->replaceText);
	
	w->ec = GUIBufferEditControl_New(gm);
	
	w->statusBar = StatusBar_New(gm, w);
	w->showStatusBar = !gm->gs->hideStatusBar;
	
	w->tx = tx;
	
//	pcalloc(w->findSet);
//	w->ec->findSet = w->findSet;
	
	RING_INIT(&w->macros, 12);

	
	return w;
}



void GUIBufferEditor_Destroy(GUIBufferEditor* w) {
#define SFREE(x) \
do { \
	if(x) { \
		free(x); \
		x = NULL; \
	} \
} while(0)
	
	
	SFREE(w->sourceFile);
	
	// this is internally reference counted
	Buffer_Delete(w->b);

	
	// TODO more regex cleanup
	GUIBufferEditor_StopFind(w);
	
//	VEC_FREE(&w->findRanges);
	
	
	free(w);
}

void GUIBufferEditor_SaveSessionState(GUIBufferEditor* w, json_value_t* out) {
	
	json_obj_set_key(out, "path", json_new_str(w->sourceFile));
	json_obj_set_key(out, "line", json_new_int(CURSOR_LINE(w->ec->sel)->lineNum));
	json_obj_set_key(out, "col", json_new_int(CURSOR_COL(w->ec->sel)));
}


void GUIBufferEditor_LoadSessionState(GUIBufferEditor* w, json_value_t* state) {
	int line = json_obj_get_int(state, "line", 1);
	int col = json_obj_get_int(state, "col", 0);
	
//	GUIBufferEditControl_SetScroll(w->ec, line, col);
	GBEC_MoveCursorToNum(w->ec, line, col);
	GBEC_SetScrollCentered(w->ec, line, col);
}


void GUIBufferEditor_UpdateSettings(GUIBufferEditor* w, Settings* s) {
	w->s = s;
	w->gs = Settings_GetSection(s, SETTINGS_General);
	w->bs = Settings_GetSection(s, SETTINGS_Buffer);
	w->ts = Settings_GetSection(s, SETTINGS_Theme);
	
	w->statusBarHeight = w->bs->statusBarHeight;
	
	GUIBufferEditControl_UpdateSettings(w->ec, s);
}


// makes sure the cursor is on screen, with minimal necessary movement
void GUIBufferEditor_scrollToCursor(GUIBufferEditor* gbe) {
	GBEC_scrollToCursor(gbe->ec);
}



BufferFindState* BufferFindState_Create(Buffer* b, char* pattern, GUIFindOpt* opt, BufferRange* searchSpace) {
	BufferFindState* st = pcalloc(st);
	st->b = b;
	
	// TODO: findmask
	if(pattern) st->pattern = strdup(pattern);
	if(opt) st->opts = *opt;
	
	if(searchSpace) {
		pcalloc(st->searchSpace);
		VEC_PUSH(&st->searchSpace->ranges, BufferRange_Copy(searchSpace));
	}
	else {
		pcalloc(st->searchSpace);
		BufferRange* r = pcalloc(r);
		r->line[0] = b->first;
		r->col[0] = 0;
		r->line[1] = b->last;
		r->col[1] = b->last->length;
		VEC_PUSH(&st->searchSpace->ranges, r);	
	}
	
	return st;
}


int BufferFindState_CompileRE(BufferFindState* st) {
	int errno;
	PCRE2_SIZE erroff;
	PCRE2_UCHAR errbuf[256];
	//printf("starting RE find: '%s'\n", pattern);
	
	// free previous regex 
	if(st->findRE) {
		pcre2_code_free(st->findRE);
		pcre2_match_data_free(st->findMatch);
		st->findMatch = 0;
	}
	
		
	uint32_t options = 0;
	if(!st->opts.case_cmp) {
		options |= PCRE2_CASELESS;
	}
	
	if(st->opts.match_mode == GFMM_PLAIN) {
		options |= PCRE2_LITERAL;
	}

	st->findRE = pcre2_compile((PCRE2_SPTR)st->pattern, PCRE2_ZERO_TERMINATED, options, &errno, &erroff, NULL);
	if(!st->findRE) {
		pcre2_get_error_message(errno, errbuf, sizeof(errbuf));
		st->findREError = strdup(errbuf);
		st->findREErrorChar = erroff;
		printf("PCRE find error #1: '%s' \n", errbuf);
		
		return 1;
	}
	
	// compilation was successful. clear any old errors.
	if(st->findREError) {
		free(st->findREError);
		st->findREError = 0;
		st->findREErrorChar = -1;
		printf("PCRE find error #2\n");
	}
	
	st->findMatch = pcre2_match_data_create_from_pattern(st->findRE, NULL);
		
	return 0;
}


int GUIBufferEditor_StartFind(GUIBufferEditor* w, BufferFindState* st) {
	
	BufferFindState_CompileRE(st);
	w->findState = st;
	
	st->nextFindLine = CURSOR_LINE(w->ec->sel);
	st->nextFindChar = CURSOR_COL(w->ec->sel);
	
	return 0;
}


int GUIBufferEditor_SmartFind(GUIBufferEditor* w, char* charSet, FindMask_t mask, BufferFindState* st) {
	if(w->trayOpen) {
		GUIBufferEditor_CloseTray(w);
	}
	
	w->inputMode = BIM_Find;
	w->trayOpen = 1;
	
	BufferRange sel = {};
	Buffer* b = w->ec->b;
	char* str = NULL;
	if((mask & FM_SELECTION) && HAS_SELECTION(w->ec->sel)) {
		str = Buffer_StringFromSelection(b, w->ec->sel, NULL);
	}
	
	if(!str && (mask & FM_SEQUENCE) && charSet) {
		Buffer_GetSequenceUnder(b, CURSOR_LINE(w->ec->sel), CURSOR_COL(w->ec->sel), charSet, &sel);
		if((sel.line[0] == sel.line[1]) && (sel.col[1] - sel.col[0] > 0)) {
			str = Buffer_StringFromSelection(b, &sel, NULL);
		}
	}
	
	if(str) {
		GUIString_Set(&w->findQuery, str);
		st->pattern = strdup(str);
	}
	
	st->findIndex = -1;
	
	st->opts.match_mode = GFMM_PCRE;
	BufferFindState_FindAll(st);
	w->ec->findSet = st->findSet;
	
	w->findState = st;

	// locate the match at/after the cursor
	GUIBufferEditor_RelativeFindMatch(w, 1, 1, st);
		
//	GUIBufferEditor_scrollToCursor(w);
	
//	GUIManager_pushFocusedObject(w->header.gm, &w->findBox->header);
	
	
	if(str) free(str);
	
	return 0;
}


int GUIBufferEditor_RelativeFindMatch(GUIBufferEditor* w, int offset, int continueFromCursor, BufferFindState* st) {
	if(!st->findSet || (st->findSet && !VEC_LEN(&st->findSet->ranges))) {
//		GUIText_setString(w->findResultsText, "No results");
		printf("no results\n");
		return 1;
	}
	
	if(st->findSet->changeCounter != w->b->changeCounter) {
		BufferFindState_FindAll(st); // used to pass in the find query box text
		st->findIndex = -1;
//		printf("reset find index\n");
	}
	
	BufferRange* r = NULL;
//	GUIBufferControl* ec = w->ec;
	intptr_t line = CURSOR_LINE(w->ec->sel)->lineNum;
	intptr_t col = CURSOR_COL(w->ec->sel);
	
	if(st->findIndex == -1) {
		if(continueFromCursor && (offset > 0)) {
			VEC_EACH(&st->findSet->ranges, i, range) {
				if(range->line[0]->lineNum < line) {
					continue;
				} 
				else if((range->line[0]->lineNum == line) && (range->col[0] < col)) {
					continue;
				}
				
				st->findIndex = i;
				r = range;
				break;
			}
		} 
		else if(continueFromCursor && (offset < 0)) {
			VEC_R_EACH(&st->findSet->ranges, i, range) {
				if(range->line[0]->lineNum > line) {
					continue;
				} 
				else if((range->line[0]->lineNum == line) && (range->col[0] >= col)) {
					continue;
				}
				
				st->findIndex = i;
				r = range;
				break;
			}
		}
	}
	
	if(!r && VEC_LEN(&st->findSet->ranges)) {
		st->findIndex = (st->findIndex + offset + VEC_LEN(&st->findSet->ranges)) % VEC_LEN(&st->findSet->ranges);
		
		r = VEC_ITEM(&st->findSet->ranges, st->findIndex);
	}
	char fmt_buffer[420];
	snprintf(fmt_buffer, 420, "%ld of %ld", st->findIndex + 1, VEC_LEN(&st->findSet->ranges));
//	GUIText_setString(w->findResultsText, fmt_buffer);
	
	GBEC_MoveCursorTo(w->ec, r->line[0], r->col[0]);
	GBEC_SetCurrentSelection(w->ec, r->line[0], r->col[0], r->line[1], r->col[1]);
	
	
	return 0;
};


void GUIBufferEditor_StopFind(GUIBufferEditor* w) {
	
	BufferFindState_FreeAll(w->findState);
	free(w->findState);
	w->findState = NULL;
}

void BufferFindState_FreeAll(BufferFindState* st) {
	
	// clean up errors
	if(st->findREError) {
		free(st->findREError);
		st->findREError = NULL;
		st->findREErrorChar = -1;
	}

	// clean up the find set
	BufferRangeSet_FreeAll(st->findSet);
	st->findIndex = -1;
	
	// cached strings
	if(st->pattern) free(st->pattern), st->pattern = NULL;
	if(st->replaceText) free(st->replaceText), st->replaceText = NULL;
}

// very broken after multicursor
int GUIBufferEditor_FindWord(GUIBufferEditor* w, char* word) {
	Buffer* b = w->b;
	
	fprintf(stderr, "%s is very broken", __func__);
	
	if(!b->first) return 1;
	
	BufferLine* bl = b->first;
	while(bl) {
		if(bl->buf) {
			char* r = strstr(bl->buf, word);//, bl->length); 
			if(r != NULL) {
				intptr_t dist = r - bl->buf;
				
// 				printf("found: %d, %d\n", bl->lineNum, dist);
				
				
				if(!w->ec->sel->line[1]) w->ec->sel = calloc(1, sizeof(*w->ec->sel));
				w->ec->sel->selecting = 1; 
				w->ec->sel->line[0] = bl; 
				w->ec->sel->line[1] = bl; 
				w->ec->sel->col[0] = dist;
				w->ec->sel->col[1] = dist + strlen(word);
				
				return 0;
			}
		}
		bl = bl->next;
	}
	
	return 1;
// 	printf("not found: '%s'\n", word);
}



int BufferFindState_FindAll(BufferFindState* st) {

	switch(st->opts.match_mode) {
		case GFMM_PLAIN:
		case GFMM_PCRE:
			return BufferFindState_FindAll_PCRE(st);
			break;
		case GFMM_FUZZY:
			return BufferFindState_FindAll_Fuzzy(st);
			break;
		default:
	}

	return 1;
}


int BufferFindState_FindAll_Fuzzy(BufferFindState* st) {
	return 1;
}


int BufferFindState_FindAll_PCRE(BufferFindState* st) {
	
	
	
	if(!st->findSet) {
		st->findSet = pcalloc(st->findSet);
		st->findSet->changeCounter = st->b->changeCounter;
	}
	else {
		// TODO: free ranges internally
		VEC_TRUNC(&st->findSet->ranges);
	}
	

	BufferLine* findLine;
	intptr_t findCharS;
	intptr_t findCharE;
	intptr_t findLen;
	char* findREError;
	int findREErrorChar;
	
	if(BufferFindState_CompileRE(st)) {
		return 1;
	}
		

	BufferLine* nextFindLine;
	intptr_t nextFindChar = 0;
	
	
	VEC_EACH(&st->searchSpace->ranges, ssri, ssr) {
		BufferLine* bl = ssr->line[0];
		int off = 0; // this is for partial matches
		uint32_t opts = PCRE2_NOTEMPTY | PCRE2_NOTEMPTY_ATSTART;
		int res;
		int wraps = 0;
		
		while(bl && bl->lineNum <= ssr->line[1]->lineNum) {
			
			colnum_t slen = bl->length;
			if(bl == ssr->line[1]) slen = MIN(ssr->col[1], slen);
			if(bl == ssr->line[0]) slen -= ssr->col[0];
			
			res = pcre2_match(st->findRE, bl->buf + nextFindChar, slen, off, opts, st->findMatch, NULL);
			
			if(res > 0) {
				// found a match
			
				PCRE2_SIZE* ovec = pcre2_get_ovector_pointer(st->findMatch);
				
				BufferRange* r  = pcalloc(r);
				r->line[0] = bl;
				r->col[0] = (int)ovec[0] + nextFindChar;
				r->line[1] = bl;
				r->col[1] = (int)ovec[1] + nextFindChar;
				
				VEC_PUSH(&st->findSet->ranges, r);
				
				nextFindChar += (int)ovec[1];  
				
//				printf("match found at: %d:%ld\n", bl->lineNum, (int)ovec[0] + nextFindChar);
				
				if(nextFindChar >= bl->length) {
					bl = bl->next;
					nextFindChar = 0;
				}
			
			}
			else {
				// no match
				
				if(res != PCRE2_ERROR_NOMATCH) {
					// real error of some sort	
					char errbuf[256];
					pcre2_get_error_message(res, errbuf, sizeof(errbuf));
					
					printf("PCRE real error: %p %p %ld '%s'\n", st->findRE, bl->buf, (uint64_t)bl->lineNum, errbuf);
					
					return NULL;
				}
				
				bl = bl->next;
				nextFindChar = 0;
			}
			
			if(!bl) { // end of file
				break;
			}
		}
	}

		
	return 0;
}




void GUIBufferEditor_ReplaceAll(GUIBufferEditor* w, BufferRangeSet* rset, char* text) {
	if(!VEC_LEN(&rset->ranges)) return;
	
	Buffer* b = w->ec->b;
	GUIBufferEditControl* ec = w->ec;
	size_t len = strlen(text);
			
	VEC_R_EACH(&rset->ranges, i, r) {
		if(r) {
			Buffer_DeleteSelectionContents(b, r);
			Buffer_LineInsertChars(b, r->line[0], text, r->col[0], len);
		}
	}
	
}


void GUIBufferEditor_MoveCursorTo(GUIBufferEditor* gbe, intptr_t line, intptr_t col) {
	if(line < 1) line = 1;
	if(col < 0) col = 0;
	CURSOR_LINE(gbe->ec->sel) = Buffer_raw_GetLineByNum(gbe->b, line);
	CURSOR_COL(gbe->ec->sel) = MIN(col, CURSOR_LINE(gbe->ec->sel)->length); // TODO: check for bounds
}



void GUIBufferEditor_ReplayMacro(GUIBufferEditor* w, int index) {
	int needRehighlight;
	
	
	if(index >= RING_LEN(&w->macros)) return;
	
	BufferEditorMacro* m = &RING_ITEM(&w->macros, index);
	
	VEC_EACHP(&m->cmds, i, cmd) {
		
		if(GUIBufferEditor_ProcessCommand(w, cmd, &needRehighlight)) {
			GBEC_ProcessCommand(w->ec, cmd, &needRehighlight);
		}
			
		if(needRehighlight || cmd->flags & GUICMD_FLAG_rehighlight) {
			GUIBufferEditControl_MarkRefreshHighlight(w->ec);
		}

		needRehighlight = 0;
	}
}


int GUIBufferEditor_ProcessCommand(GUIBufferEditor* w, GUI_Cmd* cmd, int* needRehighlight) {
//	GUIEdit* e;
	struct json_file* jsf;
	GUIManager* gm = w->gm;
	
	// keep this command around if a macro is being recorded
	if(w->isRecording && cmd->cmd != GUICMD_Buffer_MacroToggleRecording) {
		BufferEditorMacro* m = &RING_HEAD(&w->macros);
		VEC_PUSH(&m->cmds, *cmd);
	}
	
	switch(cmd->cmd){
		case GUICMD_Buffer_SetMode:
			w->inputMode = cmd->amt;
			break;
			
		case GUICMD_Buffer_PushMode:
			VEC_PUSH(&w->inputModeStack, w->inputMode);
			w->inputMode = cmd->amt;
			break;
			
		case GUICMD_Buffer_PopMode:
			if(VEC_LEN(&w->inputModeStack)) {
				VEC_POP(&w->inputModeStack, w->inputMode);
			}
			else
				w->inputMode = 0;
			break;

		case GUICMD_Buffer_MacroToggleRecording: 
			w->isRecording = !w->isRecording;
			if(w->isRecording) {
				RING_PUSH(&w->macros, (BufferEditorMacro){});
				BufferEditorMacro* m = &RING_HEAD(&w->macros);
				VEC_TRUNC(&m->cmds);
			}
			break;
			
		case GUICMD_Buffer_MacroReplay: 
			GUIBufferEditor_ReplayMacro(w, cmd->amt + w->isRecording);
			break;
			
		case GUICMD_Buffer_ToggleGDBBreakpoint: {
			CURSOR_LINE(w->ec->sel)->flags ^= BL_BREAKPOINT_FLAG;
			
			//if(w->ec->current->flags & BL_BREAKPOINT_FLAG) {
			// actually toggles breakpoint
				if(w->setBreakpoint) 
					w->setBreakpoint(w->sourceFile, CURSOR_LINE(w->ec->sel)->lineNum, w->setBreakpointData);
			//}
			
			break;
		}
		case GUICMD_Buffer_MovePage:
			GBEC_MoveCursorV(w->ec, w->ec->sel, cmd->amt * w->ec->linesOnScreen);
			w->ec->scrollLines = MAX(0, MIN(w->ec->scrollLines + cmd->amt * w->ec->linesOnScreen, w->b->numLines - 1));
			break;
		
			
			// TODO: init selectoin and pivots if no selection active
		case GUICMD_Buffer_GrowSelectionH:
// 			if(!w->selectPivotLine) {
			if(!HAS_SELECTION(w->ec->sel)) {
				w->ec->sel->selecting = 1;
				PIVOT_LINE(w->ec->sel) = CURSOR_LINE(w->ec->sel);
				PIVOT_COL(w->ec->sel) = CURSOR_COL(w->ec->sel);
			}
// 			printf("pivot: %d, %d\n", w->selectPivotLine->lineNum, w->selectPivotCol);
			GBEC_MoveCursorH(w->ec, w->ec->sel, cmd->amt);
//			GBEC_SetSelectionFromPivot(w->ec);
			break;
		
		case GUICMD_Buffer_GrowSelectionToNextSequence:
			if(!HAS_SELECTION(w->ec->sel)) {
				w->ec->sel->selecting = 1;
				PIVOT_LINE(w->ec->sel) = CURSOR_LINE(w->ec->sel);
				PIVOT_COL(w->ec->sel) = CURSOR_COL(w->ec->sel);
			}
			GBEC_MoveToNextSequence(w->ec, CURSOR_LINE(w->ec->sel), CURSOR_COL(w->ec->sel), cmd->str);
			GBEC_SetSelectionFromPivot(w->ec);
			break;
		
		case GUICMD_Buffer_GrowSelectionToPrevSequence:
			if(!HAS_SELECTION(w->ec->sel)) {
				w->ec->sel->selecting = 1;
				PIVOT_LINE(w->ec->sel) = CURSOR_LINE(w->ec->sel);
				PIVOT_COL(w->ec->sel) = CURSOR_COL(w->ec->sel);
			}
			GBEC_MoveToPrevSequence(w->ec, CURSOR_LINE(w->ec->sel), CURSOR_COL(w->ec->sel), cmd->str);
			GBEC_SetSelectionFromPivot(w->ec);
			break;
		
		case GUICMD_Buffer_GrowSelectionToSOL:
			if(!HAS_SELECTION(w->ec->sel)) {
				w->ec->sel->selecting = 1;
				PIVOT_LINE(w->ec->sel) = CURSOR_LINE(w->ec->sel);
				PIVOT_COL(w->ec->sel) = CURSOR_COL(w->ec->sel);
			}
			GBEC_MoveToFirstCharOrSOL(w->ec, CURSOR_LINE(w->ec->sel));
			GBEC_SetSelectionFromPivot(w->ec);
			break;
		
		case GUICMD_Buffer_GrowSelectionToEOL:
			if(!HAS_SELECTION(w->ec->sel)) {
				w->ec->sel->selecting = 1;
				PIVOT_LINE(w->ec->sel) = CURSOR_LINE(w->ec->sel);
				PIVOT_COL(w->ec->sel) = CURSOR_COL(w->ec->sel);
			}
			GBEC_MoveToLastCharOfLine(w->ec, CURSOR_LINE(w->ec->sel));
			GBEC_SetSelectionFromPivot(w->ec);
			break;
			
		
		case GUICMD_Buffer_GrowSelectionV:
			if(!HAS_SELECTION(w->ec->sel)) {
				w->ec->sel->selecting = 1;
				PIVOT_LINE(w->ec->sel) = CURSOR_LINE(w->ec->sel);
				PIVOT_COL(w->ec->sel) = CURSOR_COL(w->ec->sel);
			}
// 			printf("pivot: %d, %d\n", w->selectPivotLine->lineNum, w->selectPivotCol);
			GBEC_GrowSelectionV(w->ec, cmd->amt);
//			GBEC_SetSelectionFromPivot(w->ec);
			break;
		
		case GUICMD_Buffer_ClearSelection:
			if(HAS_SELECTION(w->ec->sel)) GBEC_ClearAllSelections(w->ec);
			break;
			
		case GUICMD_Buffer_SelectSequenceUnder:
			GBEC_SelectSequenceUnder(w->ec, CURSOR_LINE(w->ec->sel), CURSOR_COL(w->ec->sel), cmd->str); 
			break;
			
		case GUICMD_Buffer_MoveToNextSequence:
			GBEC_MoveToNextSequence(w->ec, CURSOR_LINE(w->ec->sel), CURSOR_COL(w->ec->sel), cmd->str);
			break;
			
		case GUICMD_Buffer_MoveToPrevSequence:
			GBEC_MoveToPrevSequence(w->ec, CURSOR_LINE(w->ec->sel), CURSOR_COL(w->ec->sel), cmd->str);
			break;
			
		case GUICMD_Buffer_DeleteToNextSequence:
			GBEC_DeleteToNextSequence(w->ec, CURSOR_LINE(w->ec->sel), CURSOR_COL(w->ec->sel), cmd->str);
			break;
			
		case GUICMD_Buffer_DeleteToPrevSequence:
			GBEC_DeleteToPrevSequence(w->ec, CURSOR_LINE(w->ec->sel), CURSOR_COL(w->ec->sel), cmd->str);
			break;
			
		case GUICMD_Buffer_GoToEOL:
			if(HAS_SELECTION(w->ec->sel)) GBEC_ClearAllSelections(w->ec);
			CURSOR_COL(w->ec->sel) = CURSOR_LINE(w->ec->sel)->length;
			break;
			
		case GUICMD_Buffer_GoToSOL:
			if(HAS_SELECTION(w->ec->sel)) GBEC_ClearAllSelections(w->ec);
			CURSOR_COL(w->ec->sel) = 0;
			break;
		
		case GUICMD_Buffer_GoToAfterIndent:
			if(HAS_SELECTION(w->ec->sel)) GBEC_ClearAllSelections(w->ec);
			GBEC_MoveCursorTo(w->ec, CURSOR_LINE(w->ec->sel), BufferLine_GetIndentCol(CURSOR_LINE(w->ec->sel)));
			break;
			
		case GUICMD_Buffer_GoToLineLaunch: 
//			w->gotoLineTrayOpen = 1;
//			w->inputMode = 3;
			GUI_SetActive(&w->gotoLineNum);
		/*
			if(w->inputMode != BIM_GoTo) {
				if(w->trayOpen) {
					GUIBufferEditor_CloseTray(w);
				}
				
				w->inputMode = BIM_GoTo;
				w->trayOpen = 1;
				w->header.cmdMode = BIM_GoTo;
				
				w->trayRoot = (GUIWindow*)GUIManager_SpawnTemplate(w->header.gm, "goto_tray");
				GUI_RegisterObject(w, w->trayRoot);
				w->lineNumEntryBox = (GUIEdit*)GUI_FindChild(w->trayRoot, "goto_line");
				
				GUIManager_pushFocusedObject(w->header.gm, &w->lineNumEntryBox->header);
			}
			else {
				GUIBufferEditor_CloseTray(w);
				GUIManager_popFocusedObject(w->header.gm);
			}*/
		// TODO: change hooks
			break;
		
		case GUICMD_Buffer_GoToLineCancel:
//			w->gotoLineTrayOpen = 0;
//			w->inputMode = 0;
			break;		
		
		case GUICMD_Buffer_GoToLineSubmit: 
//			w->gotoLineTrayOpen = 0;
//			w->inputMode = 0;
			break;
		
		case GUICMD_Buffer_ReplaceNext: { // TODO put this all in a better spot
			Buffer* b = w->ec->b;
			GUIBufferEditControl* ec = w->ec;
			
			if(!w->findState->findSet || w->findState->findSet && !VEC_LEN(&w->findState->findSet->ranges)) break;
			
			BufferRange* r = VEC_ITEM(&w->findState->findSet->ranges, w->findState->findIndex);
			
			if(r) {
				Buffer_DeleteSelectionContents(b, r);
				
				char* rtext = w->replaceText.data;
				size_t len = w->replaceText.len;
				
				Buffer_LineInsertChars(b, r->line[0], rtext, r->col[0], len);
				GBEC_MoveCursorTo(ec, r->line[0], r->col[0]);
				GBEC_MoveCursorH(ec, ec->sel, len);
			}
			
			GUIBufferEditor_RelativeFindMatch(w, 1, 1, w->findState);
			break;
		}
		
		case GUICMD_Buffer_ReplaceAll: {
			char* rtext = w->replaceText.data;
			size_t len = w->replaceText.len;
			
			GUIBufferEditor_ReplaceAll(w, w->findState->findSet, rtext);
		
			// HACK
			VEC_TRUNC(&w->findState->findSet->ranges);
			 
			break;
		}
		
		case GUICMD_Buffer_ReplaceStart:
		/*
			if(w->inputMode != BIM_Replace) {
				char* preserved = NULL;
				if(w->trayOpen) {
					GUIBufferEditor_CloseTray(w);
				}
				
				w->inputMode = BIM_Replace;
				w->trayOpen = 1;
				w->header.cmdMode = BIM_Replace;
		
				
				w->trayRoot = (GUIWindow*)GUIManager_SpawnTemplate(w->header.gm, "replace_tray");
				GUI_RegisterObject(w, w->trayRoot);
				w->findBox = (GUIEdit*)GUI_FindChild(w->trayRoot, "find");
				w->findResultsText = (GUIText*)GUI_FindChild(w->trayRoot, "results");
				w->replaceBox = (GUIEdit*)GUI_FindChild(w->trayRoot, "replace");
				if(w->findQuery) {
					GUIEdit_SetText(w->findBox, w->findQuery);
				}
				
				GUIManager_pushFocusedObject(w->header.gm, &w->findBox->header);

				GUIBufferEditor_RelativeFindMatch(w, 1, 1);
			}
			else {
				GUIBufferEditor_CloseTray(w);
				GUIManager_popFocusedObject(w->header.gm);
			}*/
			break;
			
		case GUICMD_Buffer_FindStartSequenceUnderCursor:
			GUIBufferEditor_SmartFind(w, cmd->str, FM_SEQUENCE, BufferFindState_Create(w->b, NULL, 0, NULL));
			
			break;
			
		case GUICMD_Buffer_FindStartFromSelection:
			GUIBufferEditor_SmartFind(w, cmd->str, FM_SEQUENCE, BufferFindState_Create(w->b, NULL, 0, NULL));
			break;
			
		case GUICMD_Buffer_FindStart:			
		case GUICMD_Buffer_FindResume:
			w->inputMode = BIM_Find;
			
			BufferFindState* st = BufferFindState_Create(w->b, NULL, 0, NULL);
			GUIBufferEditor_SmartFind(w, cmd->str, FM_SELECTION, st);
			
			GUI_SetActive(&w->findQuery);
			
			break;
		case GUICMD_Buffer_SmartFind:
			w->inputMode = BIM_Find;
			GUIBufferEditor_SmartFind(w, cmd->str, FM_SEQUENCE | FM_SELECTION, BufferFindState_Create(w->b, NULL, 0, NULL));
			GUI_SetActive(&w->findQuery);
			break;
		
		case GUICMD_Buffer_FindNext:
			GUIBufferEditor_RelativeFindMatch(w, 1, 1, w->findState);
			break;
			
		case GUICMD_Buffer_FindPrev:
			GUIBufferEditor_RelativeFindMatch(w, -1, 1, w->findState);
			break;
			
		case GUICMD_Buffer_CollapseWhitespace:
			Buffer_CollapseWhitespace(w->b, CURSOR_LINE(w->ec->sel), w->ec->sel->col[0]);
			break;
			
		case GUICMD_Buffer_CloseTray:
			w->inputMode = 0;
			if(w->trayOpen) {
				GUIBufferEditor_CloseTray(w);
				w->inputMode = BIM_Buffer;
//				GUIManager_popFocusedObject(w->header.gm);
				
				// TODO: findset: delete results
				// HACK
//				if(w->findSet) VEC_TRUNC(&w->findSet->ranges);
			}
			break;
			
// 		case GUICMD_Buffer_CloseBuffer: {
// 			GUIHeader* oo = GUIManager_SpawnTemplate(w->header.gm, "save_changes");
// 			GUI_RegisterObject(NULL, oo); // register to root window
			
			
// 			break;
// 		}
		case GUICMD_Buffer_SmartBubbleSelection: {
			BufferRange r = *w->ec->sel;
			
			if(!HAS_SELECTION(&r)) {
				Buffer_GetSequenceUnder(w->b, CURSOR_LINE(&r), CURSOR_COL(&r), cmd->str, &r);
			}
			
			char* s = Buffer_StringFromSelection(w->b, &r, NULL);
			MessagePipe_Send(w->tx, MSG_GrepOpener, s, NULL);
			break;
			
		}
			

		case GUICMD_Buffer_Save: 
			if(!g_DisableSave) {
				Buffer_SaveToFile(w->b, w->sourceFile);
			}
			else {
				printf("Buffer saving disabled.\n");
			}
			break;
		
		case GUICMD_Buffer_SaveAndClose:
			if(!g_DisableSave) {
				Buffer_SaveToFile(w->b, w->sourceFile);
			}
			else {
				printf("Buffer saving disabled.\n");
			}
			MessagePipe_Send(w->tx, MSG_CloseMe, w, NULL);
			
			break;
		
		case GUICMD_Buffer_PromptAndClose:
			// launch save_tray
			if(w->trayOpen) {
				GUIBufferEditor_CloseTray(w);
			}
			
			if(w->b->undoSaveIndex == w->b->undoCurrent) {
				// changes are saved, so just close
				MessagePipe_Send(w->tx, MSG_CloseMe, w, NULL);
				break;
			}
			
			w->trayOpen = 1;
			/*
			w->trayRoot = (GUIWindow*)GUIManager_SpawnTemplate(w->header.gm, "save_tray");
			GUI_RegisterObject(w, w->trayRoot);
			GUIText* saveTrayFilename = (GUIText*)GUI_FindChild(w->trayRoot, "filename");
			GUIText_setString(saveTrayFilename, w->sourceFile);
			
			GUIEdit* btn_save = (GUIEdit*)GUI_FindChild(w->trayRoot, "save");
			GUIHeader_AddHandler(&btn_save->header, GUIEVENT_Click, SaveTray_save_click);
			
			GUIEdit* btn_discard = (GUIEdit*)GUI_FindChild(w->trayRoot, "discard");
			GUIHeader_AddHandler(&btn_discard->header, GUIEVENT_Click, SaveTray_discard_click);
			
			GUIEdit* btn_cancel = (GUIEdit*)GUI_FindChild(w->trayRoot, "cancel");
			GUIHeader_AddHandler(&btn_cancel->header, GUIEVENT_Click, SaveTray_cancel_click);
			*/
			
			break;
		
		// currently crashes:
		// 0x00005555555a10f5 in getColOffset (txt=0x1 <error: Cannot access memory at address 0x1>, col=3, tabWidth=4) at src/buffer_drawing.c:24
		case GUICMD_Buffer_Reload:
		{
			struct hlinfo* hl = w->b->hl; // preserve the meta info
			
			Buffer_Delete(w->b);
//			w->ec->selectPivotLine = NULL;
//			w->ec->selectPivotCol = 0;

			// TODO: wipe out selections
			// keep the scroll lines
			w->b = Buffer_New();
			w->ec->b = w->b;
			Buffer_LoadFromFile(w->b, w->sourceFile);
			
			w->b->hl = hl;
			w->ec->scrollLines = MIN(w->ec->scrollLines, w->b->numLines);
		}
		break;
		
		
		default:
			return 1; // command not handled
	}
	
	

	
	if(cmd->flags & GUICMD_FLAG_scrollToCursor) {
		GBEC_scrollToCursor(w->ec);
	}
	
	if(cmd->flags & GUICMD_FLAG_rehighlight) {
		GUIBufferEditControl_MarkRefreshHighlight(w->ec);
	}
	
	if(cmd->flags & GUICMD_FLAG_resetCursorBlink) {
		w->ec->cursorBlinkTimer = 0;
	}
	
	if(cmd->flags & GUICMD_FLAG_undoSeqBreak) {
		if(!HAS_SELECTION(w->ec->sel)) {
//					printf("seq break without selection\n");
			Buffer_UndoSequenceBreak(
				w->b, 0, 
				CURSOR_LINE(w->ec->sel)->lineNum, CURSOR_COL(w->ec->sel),
				0, 0, 0
			);
		}
		else {
//					printf("seq break with selection\n");
			Buffer_UndoSequenceBreak(
				w->b, 0, 
				CURSOR_LINE(w->ec->sel)->lineNum, CURSOR_COL(w->ec->sel),
				PIVOT_LINE(w->ec->sel)->lineNum, PIVOT_COL(w->ec->sel),
				1 // TODO check pivot locations
			);
		}			
	}
	
	if(cmd->flags & GUICMD_FLAG_centerOnCursor) {
		GBEC_scrollToCursorOpt(w->ec, 1);
	}
	
	if(cmd->setMode >= 0) {
		GUI_CmdModeInfo* cmi = Commands_GetModeInfo(gm, cmd->setMode);
		if(cmi) {
			if(cmi->flags & GUICMD_MODE_FLAG_isOverlay) {
				w->overlays |= 1 << cmi->overlayBitIndex;
			}
			else {
				w->inputMode = cmd->setMode;
			}
		}
	}
	
	if(cmd->clearMode >= 0) {
		GUI_CmdModeInfo* cmi = Commands_GetModeInfo(gm, cmd->clearMode);
		if(cmi) {
			if(cmi->flags & GUICMD_MODE_FLAG_isOverlay) {
				w->overlays &= ~(1ul << cmi->overlayBitIndex);
			}
			else {
				if(w->inputMode == cmd->clearMode) {
					cmi = Commands_GetModeInfo(gm, w->inputMode);
					w->inputMode = (cmi && cmi->cascade >= 0) ? cmi->cascade : 0;
				}
			}
		}
	}
	
	int showGoto = 0;
	int showFind = 0;
	static int last_mode = -1;
	if(last_mode != w->inputMode) {
		last_mode = w->inputMode;
		
		GUI_CmdModeInfo* cmi = Commands_GetModeInfo(gm, w->inputMode);
		if(cmi) {
			showGoto |= !!(cmi->flags & GUICMD_MODE_FLAG_showGoToLineBar);
			showFind |= !!(cmi->flags & GUICMD_MODE_FLAG_showFindBar);
		}
	}
	
	static uint64_t last_overlays = 0;
	if(last_overlays != w->overlays) {
		last_overlays = w->overlays;
		
		for(int i = 0; i < 63; i++) {
			if(!(w->overlays & (1ul << i))) continue;
			
			GUI_CmdModeInfo* cmi = Commands_GetOverlay(gm, i);
			if(!cmi) continue;
			showGoto |= !!(cmi->flags & GUICMD_MODE_FLAG_showGoToLineBar);
			showFind |= !!(cmi->flags & GUICMD_MODE_FLAG_showFindBar);
		}
	}
	
	
	w->gotoLineTrayOpen = showGoto;
	w->trayOpen = showFind;
	
	return 0;
	
}




void GUIBufferEditor_CloseTray(GUIBufferEditor* w) {
	if(!w->trayOpen) return;
// 	
	w->trayOpen = 0;
	w->inputMode = BIM_Buffer;
	
	
//	w->header.cmdMode = BIM_Buffer;
	
//	GUI_Delete(w->trayRoot);
//	w->trayRoot = NULL;
//	w->findBox = NULL;
//	w->replaceBox = NULL;
}

void GUIBufferEditor_ToggleTray(GUIBufferEditor* w, float height) {
	if(w->trayOpen) GUIBufferEditor_CloseTray(w);
	else GUIBufferEditor_OpenTray(w, height);
}

void GUIBufferEditor_OpenTray(GUIBufferEditor* w, float height) {
	/*if(w->trayOpen) {
		if(w->trayHeight != height) {
			w->trayHeight = height;
			return;
		}
	}
	
	w->trayOpen = 1; 
	w->trayHeight = height;
	w->trayRoot = GUIWindow_New(w->header.gm);
	
	GUI_RegisterObject(w, w->trayRoot);
	*/
}


void GUIBufferEditor_ProbeHighlighter(GUIBufferEditor* w) {
	Highlighter* h;
	
	h = HighlighterManager_ProbeExt(w->hm, w->sourceFile);
	
	if(h) {
		w->ec->h = h;
		
		GUIBufferEditControl_MarkRefreshHighlight(w->ec);
	}

}

