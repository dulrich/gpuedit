#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <dlfcn.h>

#include "highlight.h"
#include "common_math.h"
#include "common_gl.h"


static void* a_malloc(Allocator* a, size_t sz) {
	return malloc(sz);
}
static void* a_calloc(Allocator* a, size_t sz) {
	return calloc(1, sz);
}
static void* a_realloc(Allocator* a, void* p, size_t sz) {
	return realloc(p, sz);
}
static void a_free(Allocator* a, void* p) {
	free(p);
}


typedef void (*entryFn)(Allocator*, HighlighterPluginInfo** /*list*/, uint64_t* /*count*/);


HighlighterModule* Highlighter_LoadModule(HighlighterManager* hm, char* path) {
	
	int flags = RTLD_LAZY;
	
	void* lib = dlopen(path, flags);
	if(!lib) {
		printf("Failed to open highlighter library: '%s'\n", path);
		return NULL;
	}
	
	entryFn getList = dlsym(lib, "gpuedit_list_highlighters");
	if(!getList) {
		printf("Invalid highlighter library: '%s'\n", path);
		dlclose(lib);
		return NULL;
	}
	
	Allocator al = {
		.malloc = a_malloc,
		.calloc = a_calloc,
		.realloc = a_realloc,
		.free = a_free,
	};
	
	HighlighterPluginInfo* list;
	uint64_t cnt;
	
	getList(&al, &list, &cnt);
	
	for(int i = 0; i < cnt; i++) {
		HighlighterPluginInfo* hpi = list + i;
		Highlighter* h = pcalloc(h);
		h->plugin = hpi;
		h->numStyles = hpi->getStyleCount();
		h->styles = calloc(1, sizeof(*h->styles) * h->numStyles);
		
		hpi->getStyleDefaults(h->styles, h->numStyles);
		
		VEC_PUSH(&hm->plugins, h);
	}
	
	HighlighterModule* mod = pcalloc(mod);
	mod->numHighlighters = cnt;
	mod->highlighters = (void*)list;
	mod->path = strdup(path);
	mod->libHandle = lib;
	
	VEC_PUSH(&hm->modules, mod);
	
	return mod;
}












 
#define CHECK_ALLOC(buf, allocsz, len, inc, extra) \
do { \
	if((buf) == NULL) { \
		(allocsz) = MAX(32, nextPOT((inc) + (extra))); \
		(buf) = malloc(sizeof(*(buf)) * (allocsz)); \
	} \
	else if((allocsz) < (len) + (inc) + (extra)) { \
		(allocsz) = nextPOT((len) + (inc) + (extra));  \
		(buf) = realloc((buf), sizeof(*(buf)) * (allocsz)); \
	} \
} while(0);



static StyleInfo* get_style(Highlighter* h, char* name) {
	for(int i = 0; i < h->numStyles; i++) {
		if(0 == strcmp(h->styles[i].name, name)) {
			return &h->styles[i];
		}
	}
	
	return NULL;
}



void Highlighter_PrintStyles(Highlighter* h) {
	
	for(int i = 0; i < h->numStyles; i++) {
		printf("%d: %s\n", i, h->styles[i].name);
	}
	
}



void Highlighter_LoadStyles(Highlighter* h, char* path) {
	size_t len;
	
	char* src = readWholeFile(path, &len);
	
	
	char** lines = strsplit_inplace(src, '\n', NULL);
	
	char** lines2 = lines;
	for(int ln = 1; *lines2; lines2++, ln++) {
		char name[128];
		char value[128];
		StyleInfo* style;
		
		if(2 != sscanf(*lines2, " %127[_a-zA-Z0-9] = %127s ", name, value)) {
			printf("Invalid highlighting color line %s:%d: '%s'\n", path, ln, *lines2);
			continue;
		}
		
// 		printf("line: '%s' = '%s'\n", name, value);
		
		style = get_style(h, name);
		if(!style) {
			fprintf(stderr, "Unknown style name '%s' in %s:%d\n", name, path, ln);
			continue;
		}
		
		
		if(value[0] == '#') { // hex code
			decodeHexColorNorm(value, (float*)&style->fgColor);
		}
		
		// TODO: rgba()
		// TODO: backgrounds, formats, fonts, etc
		
	}
	
	
	free(lines);
	free(src);
	
	
}








