#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "common_math.h"


#include "buffer.h"
#include "gui.h"
#include "gui_internal.h"
#include "clipboard.h"

#include "highlighters/c.h"




extern int g_DisableSave;


    //////////////////////////////////
   //                              //
  //       GUIBufferEditor        //
 //                              //
//////////////////////////////////

static void render(GUIBufferEditor* w, PassFrameParams* pfp) {
// HACK
// 	GUIBufferEditor_Draw(w, w->header.gm, w->scrollLines, + w->scrollLines + w->linesOnScreen + 2, 0, 100);
	
	if(w->lineNumTypingMode) {
		GUIHeader_render(&w->lineNumEntryBox->header, pfp); 
	}
	
	GUIHeader_renderChildren(&w->header, pfp);
}


static void keyDown(GUIObject* w_, GUIEvent* gev) {
	GUIBufferEditor* w = (GUIBufferEditor*)w_;
	int needRehighlight = 0;
	
	if(isprint(gev->character) && (gev->modifiers & (~(GUIMODKEY_SHIFT | GUIMODKEY_LSHIFT | GUIMODKEY_RSHIFT))) == 0) {
		GUIBufferEditControl_ProcessCommand(w->ec, &(BufferCmd){
			BufferCmd_InsertChar, gev->character
		}, &needRehighlight);
		
		GUIBufferEditControl_RefreshHighlight(w->ec);
	}
	else {
		// special commands
		unsigned int S = GUIMODKEY_SHIFT;
		unsigned int C = GUIMODKEY_CTRL;
		unsigned int A = GUIMODKEY_ALT;
		unsigned int T = GUIMODKEY_TUX;
		
		unsigned int scrollToCursor   = 1 << 0;
		unsigned int rehighlight      = 1 << 1;
		unsigned int resetCursorBlink = 1 << 2;
		unsigned int undoSeqBreak     = 1 << 3;
		unsigned int hideMouse        = 1 << 4;
		
		
		Cmd found;
		unsigned int iter = 0;
		while(Commands_ProbeCommand(gev, w->commands, w->inputMode, &found, &iter)) {
			// GUIBufferEditor will pass on commands to the buffer
			GUIBufferEditor_ProcessCommand(w, &(BufferCmd){
				.type = found.cmd, .str = found.str 
			}, &needRehighlight);
			
			
			if(found.flags & scrollToCursor) {
				GUIBufferEditControl_scrollToCursor(w->ec);
			}
			
			if(found.flags & rehighlight) {
				GUIBufferEditControl_RefreshHighlight(w->ec);
			}
			
			if(found.flags & resetCursorBlink) {
				w->ec->cursorBlinkTimer = 0;
			}
			
			if(found.flags & undoSeqBreak) {
				if(!w->ec->sel) {
					printf("seq break without selection\n");
					Buffer_UndoSequenceBreak(
						w->buffer, 0, 
						w->ec->current->lineNum, w->ec->curCol,
						0, 0, 0
					);
				}
				else {
					printf("seq break with selection\n");
					Buffer_UndoSequenceBreak(
						w->buffer, 0, 
						w->ec->sel->startLine->lineNum, w->ec->sel->startCol,
						w->ec->sel->endLine->lineNum, w->ec->sel->endCol,
						1 // TODO check pivot locations
					);
				}
			}
		}
		
	}
	
}

static void parentResize(GUIBufferEditor* w, GUIEvent* gev) {
	w->header.size = gev->size;
	if(w->trayRoot) {
		w->trayRoot->header.size.x = w->header.size.x;
	}
}

static void updatePos(GUIBufferEditor* w, GUIRenderParams* grp, PassFrameParams* pfp) {
	Vector2 wsz = w->header.size; 
	
	float sbHeight = w->statusBarHeight;
	if(!w->showStatusBar) {
		sbHeight = 0;
	}
	
	
	if(w->trayOpen) {
		w->trayRoot->header.size.x = w->header.size.x;
		w->trayRoot->header.z = 500;
		sbHeight += w->trayRoot->header.size.y;
		w->trayRoot->header.topleft.y = wsz.y - sbHeight;
		
		w->trayRoot->color = (Color4){.4,.4,.4,1};
		w->trayRoot->padding = (AABB2){{5,5}, {5,5}};
	}
	
	w->statusBar->header.hidden = !w->showStatusBar;
	w->statusBar->header.size = (Vector2){wsz.x, w->statusBarHeight};
	w->statusBar->header.topleft = (Vector2){0, 0};
	
	w->ec->header.size = (Vector2){wsz.x, wsz.y - sbHeight};
	w->ec->header.topleft = (Vector2){0, 0};
	
	
	
	gui_defaultUpdatePos(&w->header, grp, pfp);
}


void GUIBufferEditor_SetBuffer(GUIBufferEditor* w, Buffer* b) {
	w->buffer = b;
	GUIBufferEditControl_SetBuffer(w->ec, b);
}

static void userEvent(GUIObject* w_, GUIEvent* gev) {
	GUIBufferEditor* w = (GUIBufferEditor*)w_;
	
	if(w->trayOpen && (GUIEdit*)gev->originalTarget == w->findBox) {
		if(0 == strcmp(gev->userType, "change")) {
			// becaus userData is not null terminated from the Edit
			if(w->findQuery) free(w->findQuery);
			w->findQuery = strndup(gev->userData, gev->userSize);
			
			GUIBufferEditor_StopFind(w);
			GUIBufferEditor_StartFind(w, w->findQuery);
			GUIBufferEditor_NextFindMatch(w);
			
		// 	GUIBufferEditor_FindWord(w, word);
			GUIBufferEditor_scrollToCursor(w);
		}
		else if(0 == strcmp(gev->userType, "enter")) {
			GUIBufferEditor_NextFindMatch(w);
			GUIBufferEditor_scrollToCursor(w);
		}
	}
}

static void gainedFocus(GUIObject* w_, GUIEvent* gev) {
	GUIBufferEditor* w = (GUIBufferEditor*)w_;
	
	if(w->findMode == 1 || w->replaceMode == 1) {
		GUIManager_pushFocusedObject(w->header.gm, w->findBox);
	}
}


GUIBufferEditor* GUIBufferEditor_New(GUIManager* gm) {
	
	static struct gui_vtbl static_vt = {
		.Render = (void*)render,
		.UpdatePos = (void*)updatePos,
	};
	
	static struct GUIEventHandler_vtbl event_vt = {
		.KeyDown = keyDown,
// 		.Click = click,
// 		.ScrollUp = scrollUp,
// 		.ScrollDown = scrollDown,
// 		.DragStart = dragStart,
// 		.DragStop = dragStop,
// 		.DragMove = dragMove,
		.ParentResize = (void*)parentResize,
		.User = userEvent,
		.GainedFocus = gainedFocus,
	};
	
	
	GUIBufferEditor* w = pcalloc(w);
	
	gui_headerInit(&w->header, gm, &static_vt, &event_vt);
	
	w->header.cursor = GUIMOUSECURSOR_TEXT;
	
	
	w->ec = GUIBufferEditControl_New(gm);
// 	w->ec->header.flags = GUI_MAXIMIZE_X | GUI_MAXIMIZE_Y;
	GUIRegisterObject(w, w->ec);

	w->showStatusBar = 1;
	w->statusBarHeight = gm->gs->Buffer_statusBarHeight;
	w->statusBar = GUIStatusBar_New(gm);
// 	w->statusBar->header.flags = GUI_MAXIMIZE_X;
	w->statusBar->header.gravity = GUI_GRAV_BOTTOM_LEFT;
	w->statusBar->header.size.y = 30; 
	w->statusBar->header.size.x = w->header.size.x;
	w->statusBar->header.z = 500;
	
	w->statusBar->bgColor = (Color4){.1,.1,.1,1};
	w->statusBar->padding = (AABB2){{5,5}, {5,5}};
	w->statusBar->spacing = 3;
	
	GUIRegisterObject(w, w->statusBar);
	
	
// 	GUITextF* textf;
	
// 	textf = GUITextF_new(w->header.gm);
// 	textf->header.topleft = (Vector2){4, 0};
// 	GUITextF_setString(textf, "Column %>ld", &w->ec->ec->curCol);
// 	GUIRegisterObject(w->header.parent, textf);
	
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
	Buffer_Delete(w->buffer);
	
	// TODO gui cleanup
// 	w->scrollbar;
// 	w->lineNumEntryBox;
// 	w->findBox;
// 	w->loadBox;
// 	w->trayRoot;
	
	// TODO more regex cleanup
	GUIBufferEditor_StopFind(w);
	
	VEC_FREE(&w->findRanges);
	
	
	// TODO: free gui stuff
	//
	//TODO: free edit control
	
	free(w);
}



void GUIBufferEditor_UpdateSettings(GUIBufferEditor* w, GlobalSettings* s) {
	w->ec->linesPerScrollWheel = s->Buffer_linesPerScrollWheel;
	w->ec->cursorBlinkOnTime = s->Buffer_cursorBlinkOnTime;
	w->ec->cursorBlinkOffTime = s->Buffer_cursorBlinkOffTime;
	w->ec->outlineCurLine = s->Buffer_outlineCurrentLine;
	
	GUIBufferEditControl_UpdateSettings(w->ec, s);
}


// makes sure the cursor is on screen, with minimal necessary movement
void GUIBufferEditor_scrollToCursor(GUIBufferEditor* gbe) {
	GUIBufferEditControl_scrollToCursor(gbe->ec);
}







int GUIBufferEditor_StartFind(GUIBufferEditor* w, char* pattern) {
	
	int errno;
	PCRE2_SIZE erroff;
	PCRE2_UCHAR errbuf[256];
	//printf("starting RE find: '%s'\n", pattern);
	
	// free previous regex 
	if(w->findRE) {
		pcre2_code_free(w->findRE);
		pcre2_match_data_free(w->findMatch);
		w->findMatch = 0;
	}

	uint32_t options = PCRE2_CASELESS;
	
	w->findRE = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, options, &errno, &erroff, NULL);
	if(!w->findRE) {
		pcre2_get_error_message(errno, errbuf, sizeof(errbuf));
		w->findREError = strdup(errbuf);
		w->findREErrorChar = erroff;
		printf("PCRE find error #1: '%s' \n", errbuf);
		
		return 1;
	}
	
	// compilation was successful. clear any old errors.
	if(w->findREError) {
		free(w->findREError);
		w->findREError = 0;
		w->findREErrorChar = -1;
		printf("PCRE find error #2\n");
	}
	
	w->findMatch = pcre2_match_data_create_from_pattern(w->findRE, NULL);
	
	w->nextFindLine = w->ec->current;
	w->nextFindChar = w->ec->curCol;
	
	
	return 0;
}


int GUIBufferEditor_NextFindMatch(GUIBufferEditor* w) {
	
	BufferLine* bl = w->nextFindLine;
	int off = 0; // this is for partial matches
	uint32_t opts = PCRE2_NOTEMPTY | PCRE2_NOTEMPTY_ATSTART;
	int res;
	int wraps = 0;
	
	while(bl) {
		res = pcre2_match(w->findRE, bl->buf ? bl->buf + w->nextFindChar : "", bl->length, off, opts, w->findMatch, NULL);
		
		if(res > 0) break;
		if(res != PCRE2_ERROR_NOMATCH) {
			// real error of some sort
			
			char errbuf[256];
			pcre2_get_error_message(res, errbuf, sizeof(errbuf));
			
			printf("PCRE real error: %p %p %ld '%s'\n", w->findRE, bl->buf, bl->lineNum, errbuf);
			
			return 1;
		}
		
		bl = bl->next;
		w->nextFindChar = 0;
		
		if(!bl) { // end of file
			bl = w->buffer->first;
// 			printf("find wrapped\n");
			wraps++;
			
			if(wraps > 1) return 1;
		}
	}
	

	PCRE2_SIZE* ovec = pcre2_get_ovector_pointer(w->findMatch);
	w->findCharS = (int)ovec[0] + w->nextFindChar;
	w->findCharE = (int)ovec[1] + w->nextFindChar;
	w->findLine = bl;
	w->nextFindLine = bl;
	w->nextFindChar = w->findCharE;
	
	GBEC_MoveCursorTo(w->ec, bl, w->findCharS);
	GBEC_SetCurrentSelection(w->ec, bl, w->findCharS, bl, w->findCharE); 
	
// 	printf("match found at: %d:%d\n", bl->lineNum, w->findCharS);
	
	return 0;
}



void GUIBufferEditor_StopFind(GUIBufferEditor* w) {
	
	// clear errors
	if(w->findREError) {
		free(w->findREError);
		w->findREError = NULL;
		w->findREErrorChar = -1;
	}
	
	// clean up regex structures
	if(w->findRE) {
		pcre2_code_free(w->findRE);
		pcre2_match_data_free(w->findMatch);
		w->findMatch = NULL;
		w->findRE = NULL;
	}
	
	VEC_FREE(&w->findRanges);
}


int GUIBufferEditor_FindWord(GUIBufferEditor* w, char* word) {
	Buffer* b = w->buffer;
	
	if(!b->first) return 1;
	
	BufferLine* bl = b->first;
	while(bl) {
		if(bl->buf) {
			char* r = strstr(bl->buf, word);//, bl->length); 
			if(r != NULL) {
				intptr_t dist = r - bl->buf;
				
// 				printf("found: %d, %d\n", bl->lineNum, dist);
				
				w->ec->current = bl;
				w->ec->curCol = dist;
				
				if(!w->ec->sel) w->ec->sel = calloc(1, sizeof(*w->ec->sel));
				w->ec->sel->startLine = bl; 
				w->ec->sel->endLine = bl; 
				w->ec->sel->startCol = dist;
				w->ec->sel->endCol = dist + strlen(word);
				
				w->ec->selectPivotLine = bl;
				w->ec->selectPivotCol = w->ec->sel->endCol;
				
				return 0;
			}
		}
		bl = bl->next;
	}
	
	return 1;
// 	printf("not found: '%s'\n", word);
}



void GUIBufferEditor_MoveCursorTo(GUIBufferEditor* gbe, intptr_t line, intptr_t col) {
	if(line < 1) line = 1;
	if(col < 0) col = 0;
	gbe->ec->current = Buffer_raw_GetLine(gbe->buffer, line);
	gbe->ec->curCol = MIN(col, gbe->ec->current->length); // TODO: check for bounds
}


// event callbacks for the Go To Line edit box
static void gotoline_onchange(GUIEdit* ed, void* gbe_) {
	GUIBufferEditor* w = (GUIBufferEditor*)gbe_;
	
	intptr_t line = GUIEdit_GetDouble(ed);
	if(line <= 0) return; 
	
	GUIBufferEditor_MoveCursorTo(w, line, 0);
	GUIBufferEditor_scrollToCursor(w);
}

static void gotoline_onenter(GUIEdit* ed, void* gbe_) {
	GUIBufferEditor* w = (GUIBufferEditor*)gbe_;
	
	GUIBufferEditor_CloseTray(w);
	w->lineNumTypingMode = 0;
	
	GUIManager_popFocusedObject(w->header.gm);
	
}


static void loadfile_onenter(GUIEdit* ed, void* gbe_) {
	GUIBufferEditor* w = (GUIBufferEditor*)gbe_;
	
	char* word = GUIEdit_GetText(ed);
	if(word == 0 || strlen(word) == 0) return; 
	
	GUIBufferEditor_CloseTray(w);
	w->loadTypingMode = 0;
	GUIManager_popFocusedObject(w->header.gm);
	
}


void GUIBufferEditor_ProcessCommand(GUIBufferEditor* w, BufferCmd* cmd, int* needRehighlight) {
	GUIEdit* e;
	struct json_file* jsf;
	
	switch(cmd->type){
		case BufferCmd_ToggleMenu:
				
			jsf = json_load_path("/etc/gpuedit/buffer_menu.json");
			w->menu = (GUISimpleWindow*)GUICL_CreateFromConfig(w->header.gm, jsf->root);
			
			// TODO: free json 
			
			GUIRegisterObject(w, w->menu);
			
			
			GUISelectBox* hlsel = (GUISelectBox*)GUIObject_FindChild(w->menu, "highlighter");
			GUISelectBoxOption opts[] = {
				{.label = "C", .data = "c"},
				{.label = "C++", .data = "cpp"},
				{.label = "JavaScript", .data = "javascript"},
			};
			
			if(hlsel) GUISelectBox_SetOptions(hlsel, opts, 3);
			
			
			
			
			break;

		case BufferCmd_ToggleGDBBreakpoint: {
			w->ec->current->flags ^= BL_BREAKPOINT_FLAG;
			
			//if(w->ec->current->flags & BL_BREAKPOINT_FLAG) {
			// actually toggles breakpoint
				if(w->setBreakpoint) 
					w->setBreakpoint(w->sourceFile, w->ec->current->lineNum, w->setBreakpointData);
			//}
			
			break;
		}
		case BufferCmd_MovePage:
			GBEC_MoveCursorV(w->ec, cmd->amt * w->ec->linesOnScreen);
			
			w->ec->scrollLines = MAX(0, MIN(w->ec->scrollLines + cmd->amt * w->ec->linesOnScreen, w->buffer->numLines - 1));
			break;
		
			
			// TODO: init selectoin and pivots if no selection active
		case BufferCmd_GrowSelectionH:
// 			if(!w->selectPivotLine) {
			if(!w->ec->sel) {
				w->ec->selectPivotLine = w->ec->current;
				w->ec->selectPivotCol = w->ec->curCol;
			}
// 			printf("pivot: %d, %d\n", w->selectPivotLine->lineNum, w->selectPivotCol);
			GBEC_MoveCursorH(w->ec, cmd->amt);
			GUIBufferEditControl_SetSelectionFromPivot(w->ec);
			break;
		
		case BufferCmd_GrowSelectionV:
			if(!w->ec->sel) {
				w->ec->selectPivotLine = w->ec->current;
				w->ec->selectPivotCol = w->ec->curCol;
			}
// 			printf("pivot: %d, %d\n", w->selectPivotLine->lineNum, w->selectPivotCol);
			GBEC_MoveCursorV(w->ec, cmd->amt);
			GUIBufferEditControl_SetSelectionFromPivot(w->ec);
			break;
			
		case BufferCmd_SelectSequenceUnder:
			GBEC_SelectSequenceUnder(w->ec, w->ec->current, w->ec->curCol, cmd->str); 
			w->ec->selectPivotLine = w->ec->sel->startLine;
			w->ec->selectPivotCol = w->ec->sel->startCol;
			break;
			
		case BufferCmd_MoveToNextSequence:
			GBEC_MoveToNextSequence(w->ec, w->ec->current, w->ec->curCol, cmd->str);
			break;
			
		case BufferCmd_MoveToPrevSequence:
			GBEC_MoveToPrevSequence(w->ec, w->ec->current, w->ec->curCol, cmd->str);
			break;
			
		case BufferCmd_DeleteToNextSequence:
			GBEC_DeleteToNextSequence(w->ec, w->ec->current, w->ec->curCol, cmd->str);
			break;
			
		case BufferCmd_DeleteToPrevSequence:
			GBEC_DeleteToPrevSequence(w->ec, w->ec->current, w->ec->curCol, cmd->str);
			break;
			
		case BufferCmd_GoToEOL:
			if(w->ec->sel) GBEC_ClearAllSelections(w->ec);
			w->ec->curCol = w->ec->current->length;
			break;
			
		case BufferCmd_GoToSOL:
			if(w->ec->sel) GBEC_ClearAllSelections(w->ec);
			w->ec->curCol = 0;
			break;
		
		case BufferCmd_GoToAfterIndent:
			if(w->ec->sel) GBEC_ClearAllSelections(w->ec);
			GBEC_MoveCursorTo(w->ec, w->ec->current, BufferLine_GetIndentCol(w->ec->current));
			break;
			
		case BufferCmd_GoToLine:
			GUIBufferEditor_ToggleTray(w, 50);
		
			if(!w->lineNumTypingMode) {
				w->lineNumTypingMode = 1;
				// activate the line number entry box
				w->lineNumEntryBox = GUIEdit_New(w->header.gm, "");
				GUIResize(&w->lineNumEntryBox->header, (Vector2){200, 20});
				w->lineNumEntryBox->header.topleft = (Vector2){0,5};
				w->lineNumEntryBox->header.gravity = GUI_GRAV_TOP_CENTER;
				w->lineNumEntryBox->header.z = 600;
				w->lineNumEntryBox->numType = 1; // integers
				
// 				w->lineNumEntryBox->onChange = gotoline_onchange;
// 				w->lineNumEntryBox->onChangeData = w;
				
// 				w->lineNumEntryBox->onEnter = gotoline_onenter;
// 				w->lineNumEntryBox->onEnterData = w;
				
				GUIRegisterObject(w->trayRoot, w->lineNumEntryBox);
				
				w->ec->cursorBlinkPaused = 1;
				GUIManager_pushFocusedObject(w->header.gm, w->lineNumEntryBox);
			}
			else {
				GUIBufferEditor_CloseTray(w);
				w->lineNumTypingMode = 0;
				w->ec->cursorBlinkPaused = 0;
// 				guiDelete(w->lineNumEntryBox);
				GUIManager_popFocusedObject(w->header.gm);
			}
		// TODO: change hooks
			break;
		
		case BufferCmd_ReplaceNext: { // TODO put this all in a better spot
			Buffer* b = w->ec->buffer;
			GUIBufferEditControl* ec = w->ec;
			if(ec->sel) {
				ec->current = ec->sel->startLine;
				ec->curCol = ec->sel->startCol;
				Buffer_DeleteSelectionContents(b, ec->sel);
				
				char* rtext = GUIEdit_GetText(w->replaceBox);
				size_t len = strlen(rtext);
				
				Buffer_LineInsertChars(b, ec->current, rtext, ec->curCol, len);
				GBEC_MoveCursorH(ec, len);
			}
			
			GUIBufferEditor_NextFindMatch(w);
			break;
		}
		case BufferCmd_ReplaceAll:
			printf("ReplaceAll NYI\n");
			break;
			
		case BufferCmd_ReplaceStart:
			if(!w->replaceMode) {
				char* preserved = NULL;
				if(w->trayOpen) {
					GUIBufferEditor_CloseTray(w);
				}
				
				w->replaceMode = 1;
				w->trayOpen = 1;
				w->inputMode = 2;
		
				
				w->trayRoot = (GUIWindow*)GUIManager_SpawnTemplate(w->header.gm, "replace_tray");
				GUIRegisterObject(w, w->trayRoot);
				w->findBox = (GUIEdit*)GUIObject_FindChild(w->trayRoot, "find");
				w->replaceBox = (GUIEdit*)GUIObject_FindChild(w->trayRoot, "replace");
				if(w->findQuery) {
					GUIEdit_SetText(w->findBox, w->findQuery);
				}
				
				w->ec->cursorBlinkPaused = 1;
				GUIManager_pushFocusedObject(w->header.gm, w->findBox);
			}
			else {
				GUIBufferEditor_CloseTray(w);
				w->ec->cursorBlinkPaused = 0;
				GUIManager_popFocusedObject(w->header.gm);
			}
			break;
			
		case BufferCmd_FindStartSequenceUnderCursor:
			if(!w->findMode) {
				char* preserved = NULL;
				if(w->trayOpen) {
					GUIBufferEditor_CloseTray(w);
				}
				
				w->findMode = 1;
				w->trayOpen = 1;
				w->inputMode = 1;
				
				w->trayRoot = (GUIWindow*)GUIManager_SpawnTemplate(w->header.gm, "find_tray");
				GUIRegisterObject(w, w->trayRoot);
				w->findBox = (GUIEdit*)GUIObject_FindChild(w->trayRoot, "find");
				
				BufferRange sel;
				Buffer* b = w->ec->buffer;
				Buffer_GetSequenceUnder(b, w->ec->current, w->ec->curCol, cmd->str, &sel);
				char* str = Buffer_StringFromSelection(b, &sel, NULL);
				GUIEdit_SetText(w->findBox, str);
				
				GUIBufferEditor_StopFind(w);
				GUIBufferEditor_StartFind(w, str);					
				GUIBufferEditor_NextFindMatch(w);
				
				if(w->findQuery) free(w->findQuery);
				w->findQuery = str;

				GUIBufferEditor_scrollToCursor(w);
				
				w->ec->cursorBlinkPaused = 1;
				GUIManager_pushFocusedObject(w->header.gm, w->findBox);
			}
			else {
				GUIBufferEditor_CloseTray(w);
				w->ec->cursorBlinkPaused = 0;
				GUIManager_popFocusedObject(w->header.gm);
			}
			
		
			break;
		
		case BufferCmd_FindStart:
			if(w->findQuery) {
				free(w->findQuery);
				w->findQuery = NULL;
			}
			// inetnational fallthrough
			
		case BufferCmd_FindResume:
			if(!w->findMode) {
				char* preserved = NULL;
				if(w->trayOpen) {
					GUIBufferEditor_CloseTray(w);
				}
				
				w->findMode = 1;
				w->trayOpen = 1;
				w->inputMode = 1;
				
				w->trayRoot = (GUIWindow*)GUIManager_SpawnTemplate(w->header.gm, "find_tray");
				GUIRegisterObject(w, w->trayRoot);
				w->findBox = (GUIEdit*)GUIObject_FindChild(w->trayRoot, "find");
				if(w->findQuery) {
					GUIEdit_SetText(w->findBox, w->findQuery);
				}
				
				w->ec->cursorBlinkPaused = 1;
				GUIManager_pushFocusedObject(w->header.gm, w->findBox);
			}
			else {
				GUIBufferEditor_CloseTray(w);
				w->ec->cursorBlinkPaused = 0;
				GUIManager_popFocusedObject(w->header.gm);
			}
			
			break;
			
		case BufferCmd_FindNext:
			GUIBufferEditor_NextFindMatch(w);
			break;
			
		case BufferCmd_PromptLoad:
			GUIBufferEditor_ToggleTray(w, 50);
			if(!w->loadTypingMode) {
				w->loadTypingMode = 1;
				
				e = GUIEdit_New(w->header.gm, "");
				GUIResize(&e->header, (Vector2){400, 20});
				e->header.topleft = (Vector2){0,5};
				e->header.gravity = GUI_GRAV_TOP_CENTER;
				e->header.z = 600;
				
// 				e->onEnter = find_onenter;
// 				e->onEnterData = w;
				
				w->ec->cursorBlinkPaused = 1;
				GUIRegisterObject(w->trayRoot, e);
				GUIManager_pushFocusedObject(w->header.gm, e);
				
				w->loadBox = e;
			}
			else {
				GUIBufferEditor_CloseTray(w);
				w->loadTypingMode = 0;
				w->ec->cursorBlinkPaused = 0;
// 				guiDelete(w->findBox);
				GUIManager_popFocusedObject(w->header.gm);
			}
			
			break;
			
		case BufferCmd_CollapseWhitespace:
			Buffer_CollapseWhitespace(w->buffer, w->ec->current, w->ec->curCol);
			break;
			
		case BufferCmd_CloseTray:
			if(w->trayOpen) {
				GUIBufferEditor_CloseTray(w);
				w->lineNumTypingMode = 0;
				w->findMode = 0;
				w->loadTypingMode = 0;
				w->ec->cursorBlinkPaused = 0;
				GUIManager_popFocusedObject(w->header.gm);
			}
			break;
			
// 		case BufferCmd_CloseBuffer: {
// 			GUIObject* oo = GUIManager_SpawnTemplate(w->header.gm, "save_changes");
// 			GUIRegisterObject(NULL, oo); // register to root window
			
			
// 			break;
// 		}
		case BufferCmd_Save: 
			if(!g_DisableSave) {
				Buffer_SaveToFile(w->buffer, w->sourceFile);
			}
			else {
				printf("Buffer saving disabled.\n");
			}
			break;
		
		case BufferCmd_Reload:
		{
			struct hlinfo* hl = w->buffer->hl; // preserve the meta info
			EditorParams* ep = w->buffer->ep;
			
			Buffer_Delete(w->buffer);
			w->ec->selectPivotLine = NULL;
			w->ec->selectPivotCol = 0;
			// keep the scroll lines
			w->buffer = Buffer_New();
			w->ec->buffer = w->buffer;
			Buffer_LoadFromFile(w->buffer, w->sourceFile);
			
			w->buffer->hl = hl;
			w->buffer->ep = ep;
			w->ec->scrollLines = MIN(w->ec->scrollLines, w->buffer->numLines);
		}
		break;
		
		default:
			GUIBufferEditControl_ProcessCommand(w->ec, cmd, needRehighlight);
			
	}
	
// 	printf("line/col %d:%d %d\n", b->current->lineNum, b->curCol, b->current->length);
}




void GUIBufferEditor_CloseTray(GUIBufferEditor* w) {
	if(!w->trayOpen) return;
// 	
	w->trayOpen = 0;
	w->findMode = 0;
	w->replaceMode = 0;
	
	w->inputMode = 0;
	
	GUIObject_Delete(w->trayRoot);
	w->trayRoot = NULL;
	w->findBox = NULL;
	w->replaceBox = NULL;
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
	
	GUIRegisterObject(w, w->trayRoot);
	*/
}


void GUIBufferEditor_ProbeHighlighter(GUIBufferEditor* w) {
	Highlighter* h;
	
	h = HighlighterManager_ProbeExt(w->hm, w->sourceFile);
	
	if(h) {
		w->ec->h = h;
		
		GUIBufferEditControl_RefreshHighlight(w->ec);
	}

}

