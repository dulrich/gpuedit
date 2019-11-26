#ifndef __gpuedit_buffer_h__
#define __gpuedit_buffer_h__



#include "gui.h"
#include "font.h"

typedef struct BufferLine {
	
	int lineNum;
	
	size_t allocSz;
	size_t length; // of the text itself
	char* buf;
	
	char* style;
	
	struct BufferLine* prev, *next;
	
} BufferLine;


typedef struct Buffer {
	GUIHeader header;
	
	BufferLine* first, *last, *current; 
	
	int numLines;
	int curCol;
	
	// TODO: move elsewhere
	GUIFont* font;
	
} Buffer;

typedef struct TextDrawParams {
	GUIFont* font;
	float fontSize;
	float lineHeight;
	float charWidth;
	int tabWidth;
	
	// colors
} TextDrawParams;


typedef struct ThemeDrawParams {
	struct Color4 bgColor; 
	struct Color4 textColor; 
	struct Color4 cursorColor; 
	struct Color4 hl_bgColor; 
	struct Color4 hl_textColor; 
} ThemeDrawParams;


typedef struct BufferDrawParams {
	char showLineNums;
	float lineNumWidth;
	
	TextDrawParams* tdp;
	ThemeDrawParams* theme;
} BufferDrawParams;


enum BufferCmdType {
	BufferCmd_MoveCursorV,
	BufferCmd_MoveCursorH,
	BufferCmd_InsertChar,
	BufferCmd_SplitLine,
	BufferCmd_Backspace,
};

typedef struct BufferCmd {
	enum BufferCmdType type;
	int amt;
} BufferCmd;



struct GUIManager;
typedef struct GUIManager GUIManager;




BufferLine* BufferLine_New();
void BufferLine_Delete(BufferLine* l);
BufferLine* BufferLine_FromStr(char* text, size_t len);
BufferLine* BufferLine_Copy(BufferLine* orig);
void BufferLine_EnsureAlloc(BufferLine* l, size_t len);
void BufferLine_InsertChar(BufferLine* l, char c, size_t col);
void BufferLine_DeleteChar(BufferLine* l, size_t col);
void BufferLine_SetText(BufferLine* l, char* text, size_t len);
void BufferLine_InsertText(BufferLine* l, char* text, size_t len, size_t col);
void BufferLine_AppendText(BufferLine* l, char* text, size_t len);


void Buffer_RenumberLines(BufferLine* start, size_t num);
void Buffer_ProcessCommand(Buffer* b, BufferCmd* cmd);

// these functions operate independently of the cursor
BufferLine* Buffer_InsertLineBefore(Buffer* b, BufferLine* after);
BufferLine* Buffer_InsertLineAfter(Buffer* b, BufferLine* before);
void Buffer_DeleteLine(Buffer* b, BufferLine* l);
void Buffer_BackspaceAt(Buffer* b, BufferLine* l, size_t col);

// these functions operate on absolute positions
void Buffer_AppendRawText(Buffer* b, char* source, size_t len);
BufferLine* Buffer_AppendLine(Buffer* b, char* text, size_t len);
BufferLine* Buffer_PrependLine(Buffer* b, char* text, size_t len);


Buffer* Buffer_New(GUIManager* gm);

void Buffer_Draw(Buffer* b, GUIManager* gm, int lineFrom, int lineTo, int colFrom, int colTo);
static void drawTextLine(GUIManager* gm, TextDrawParams* tdp, char* txt, int charCount, Vector2 tl);


// These functions operate on and with the cursor
BufferLine* Buffer_AdvanceLines(Buffer* b, int n);
void Buffer_InsertLinebreak(Buffer* b);



void test(Buffer* b);






#endif // __gpuedit_buffer_h__
