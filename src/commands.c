#include <stdio.h>
#include <string.h>

#include <X11/keysymdef.h>

#include "sti/sti.h"

#include "commands.h"
#include "gui.h"



static struct { 
	char* name,
	unsigned int key;
} raw_keys[] = {
	{"down", XK_Down},
	{"up", XK_Up},
	{"tab", XK_Tab},
	{"enter", XK_Return},
	{"return", XK_Return},
	(NULL, 0},
};


static HashTable words;

static void init_words() {
	HT_init(&words);
	
	for(int i = 0; raw_words[i].name != 0; i++) {
		HT_set(&words, raw_words[i].name, raw_words[i].key);
	}
}

static int get_word(char* w) {
	int64_t n;
	if(HT_get(&words, w, &n)) return -1;
	return n;
} 


void CommandList_loadFile(char* path) {
	
	size_t len;
	char* src = readWhileFile(path, &len);
	char** olines = strsplit_inplace(src, '\n');
	char** lines = olines; // keep original for freeing
	
	
	while(*lines) {
		char* s = *lines;
		
		
		// first, modifiers
		unsigned int m = 0;
		unsigned int key = 0;
		
		for(*s; s++) {
			if(*s == ' ') { 
				s++;
				break;
			}
			else if(*s == 'L') {
				s++;
				     if(*s == 'C') m |= GUIMODKEY_LCTRL | GUIMODKEY_CTRL;
				else if(*s == 'A') m |= GUIMODKEY_LALT | GUIMODKEY_ALT;
				else if(*s == 'T') m |= GUIMODKEY_LTUX | GUIMODKEY_TUX;
				else if(*s == 'W') m |= GUIMODKEY_LTUX | GUIMODKEY_TUX;
			}
			else if(*s == 'R') {
				s++;
				     if(*s == 'C') m |= GUIMODKEY_RCTRL | GUIMODKEY_CTRL;
				else if(*s == 'A') m |= GUIMODKEY_RALT | GUIMODKEY_ALT;
				else if(*s == 'T') m |= GUIMODKEY_RTUX | GUIMODKEY_TUX;
				else if(*s == 'W') m |= GUIMODKEY_RTUX | GUIMODKEY_TUX;
			}
			else if(*s == 'C') m |= GUIMODKEY_CTRL;
			else if(*s == 'A') m |= GUIMODKEY_ALT;
			else if(*s == 'T') m |= GUIMODKEY_TUX;
			else if(*s == 'W') m |= GUIMODKEY_TUX;
			else {
				printf("Unknown character looking for command modifiers: '%c'\n", *s);
				s++;
			}
		}
		
		// next the main key
		for(*s; s++) {
			if(*s == '\'') { // a key literal
				s++;
				
				if(*s == '\\') { // escape sequence
					s++;
					     if(*s == 'n') key = '\n'; 
					else if(*s == 'r') key = '\r'; 
					else if(*s == 'v') key = '\v'; 
					else if(*s == 't') key = '\t'; 
					
					continue;
				}
				
				key = *s;
				s += 2; // skip the closing quote too
				
				break;
			}
			
			
			if(*s == 'X' && *(s+1) == 'K' && *(s+2) == '_') {
				// X11 key macro
				
// 					cat keysymdef.h | grep '#define' | egrep -o 'XK_[^ ]* *[x0-9a-f]*' | sed 's/  */", /g;s/^/{"/;s/$/},/'
				
				break;
			}
			
			// normal words:
			
			
		}
	}
}




