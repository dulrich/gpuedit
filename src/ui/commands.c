#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <X11/keysymdef.h>


#include "gui.h"



static struct {
	char* ename;
	char* name;
	int id;
} enum_table[] = { 
#define X(a, b) {#a, #b, GUICMD_##a##_##b},
	GUI_COMMAND_LIST
#undef X
	{NULL, NULL, 0}
};



static struct {
	char* name;
	uint64_t val;
} keysym_lookup[] = {
	#include "cmd_keysym_lookup.c"
	
	{"VK_Print", GUI_CMD_EXTSYM(1)},
	{NULL, NULL},
};



static struct { 
	char* name;
	uint64_t key;
} raw_flags[] = {
#define X(a) {#a, GUICMD_FLAG_##a },
	GUI_COMMAND_FLAG_LIST
#undef X
	{NULL, 0},
};

#define X(a) { #a, GUIELEMENT_##a},
static struct{ char* n; int id;} elemList[] = {
	GUI_ELEMENT_LIST
	{NULL, 0},
};
#undef X


static HT(uint64_t) syms;
static HT(uint64_t) flag_lookup;
//static HT(uint64_t) cmd_enums;

static void init_words() {
	HT_init(&flag_lookup, 64);
	HT_init(&syms, 2100);
//	HT_init(&cmd_enums, 120);
	

	for(int i = 0; keysym_lookup[i].name != 0; i++) {
		HT_set(&syms, keysym_lookup[i].name, keysym_lookup[i].val);
	}
	
//	for(int i = 0; enum_table[i].name != 0; i++) {
//		HT_set(&cmd_enums, enum_table[i].name, enum_table[i].val);
//	}
	
	for(int i = 0; raw_flags[i].name != 0; i++) {
		HT_set(&flag_lookup, raw_flags[i].name, raw_flags[i].key);
	}
}


void GUIManager_InitCommands(GUIManager* gm) {
	
	static int is_init = 0;
	if(!is_init) {
		is_init = 1;
		init_words();
	}
	
	HT_init(&gm->cmdElementLookup, 2048);
	
	for(int i = 0; elemList[i].n; i++) {
		GUIManager_AddCommandElement(gm, elemList[i].n, elemList[i].id);
//		HT_set(&gm->cmdElementLookup, elemList[i].n, elemList[i].id);
	}
	
	
	for(int i = 0; enum_table[i].name; i++) {
		GUIManager_AddCommand(gm, enum_table[i].ename, enum_table[i].name, enum_table[i].id);
	}
}


static int event_src_cat(GUIEvent* gev) {
	switch(gev->type) {
		case GUIEVENT_Click:
			return GUI_CMD_SRC_CLICK;
			
		case GUIEVENT_KeyDown:
			return GUI_CMD_SRC_KEY;
			
		default:
			return GUI_CMD_SRC_NONE;
	}
}


enum {
	has_ctl = 1<<0,
	has_alt = 1<<1,
	has_shift = 1<<2,
	has_tux = 1<<3,
};


static unsigned int get_index(unsigned int mods) {
	unsigned int o = 0;
	if(mods & GUIMODKEY_CTRL) o |= has_ctl;
	if(mods & GUIMODKEY_ALT) o |= has_alt;
	if(mods & GUIMODKEY_SHIFT) o |= has_shift;
	if(mods & GUIMODKEY_TUX) o |= has_tux;
	return o;
}

GUI_Cmd* Commands_ProbeCommand(GUIManager* gm, int elemType, GUIEvent* gev, int mode) {
//	printf("probing\n");
	
	
	unsigned int ANY = (GUIMODKEY_SHIFT | GUIMODKEY_CTRL | GUIMODKEY_ALT | GUIMODKEY_TUX);
	unsigned int ANY_MASK = ~ANY;
	
	uint16_t cat = event_src_cat(gev);
	
	unsigned int i = 0;

	VEC_EACHP(&gm->cmdList, i, cp) {
 			//printf("%d, '%c', %x \n", gev->keycode, gev->keycode, gev->modifiers);
		int extMode = 0;
		int32_t c = gev->keycode;
		
		if(cat == GUI_CMD_SRC_NONE) {
			return NULL;
		}
		if(cat == GUI_CMD_SRC_KEY) {
			if(cp->keysym & (1 << 29)) {
				extMode = 1;
			}
			else 
				c = tolower(c);
		}
		else {
			c = GUI_CMD_RATSYM(gev->button, gev->multiClick);
		}

		
		if(cp->src_type != cat) { continue; }
		if(cp->element != elemType) { continue; }
		if(cp->mode != mode) { continue; }
		
		if(extMode) {
			if((cp->keysym & ~(7u << 29u)) == 1) { // regular print chars
				if(isprint(gev->character) && (gev->modifiers & (~(GUIMODKEY_SHIFT | GUIMODKEY_LSHIFT | GUIMODKEY_RSHIFT))) == 0) {
				
					gm->tmpCmd = *cp;
					gm->tmpCmd.amt = gev->character;
					
					return &gm->tmpCmd;
				}
			}
		
			continue;
		}
		
		if(cp->keysym != c) { continue; }
		if((cp->mods & ANY) != (gev->modifiers & ANY)) { continue; }
		// TODO: specific mods
		
		// found
		//printf("found command: %d, %d, %x\n", cp->element, cp->mode, cp->mods);
		return cp;
		
	}
//	printf("^^^^\n");


	GUI_CmdModeInfo* cmi = Commands_GetModeInfo(gm, mode);
	if(cmi && cmi->parent != -1) {
		return Commands_ProbeCommand(gm, elemType, gev, cmi->parent);
	}


	return NULL; // no match
}
/*
GUI_Cmd* Commands_ProbeSubCommand(GUIHeader* gh, int sub_elem, GUIEvent* gev) { 
	GUIManager* gm = gh->gm;
//	printf("probing\n");
	unsigned int ANY = (GUIMODKEY_SHIFT | GUIMODKEY_CTRL | GUIMODKEY_ALT | GUIMODKEY_TUX);
	unsigned int ANY_MASK = ~ANY;
		
	uint16_t cat = event_src_cat(gev);
	
	unsigned int i = 0;
	
	VEC_EACHP(&gm->cmdListSubElems, i, cp) {
	
		int c = gev->keycode;
		if(cat == GUI_CMD_SRC_KEY) {
			c = tolower(c);
		}
		else {
			c = GUI_CMD_RATSYM(gev->button, gev->multiClick);
		}
	
 			//printf("%d, '%c', %x \n", gev->keycode, gev->keycode, gev->modifiers);
		if(cp->sub_elem != sub_elem) { continue; }
		if(cp->src_type != cat) { continue; }
		if(cp->element != gh->cmdElementType) { continue; }
		if(cp->mode != gh->cmdMode) { continue; }
		if(cp->keysym != c) { continue; }
		if((cp->mods & ANY) != (gev->modifiers & ANY)) { continue; }
		// TODO: specific mods
		
		// found
		//printf("found sub-command: %d, %d, %x\n", cp->element, cp->mode, cp->mods);
		return cp;
		
	}

	return NULL; // no match
}
*/

int GUIManager_AddCommand(GUIManager* gm, char* elemname, char* name, uint32_t id) {
	GUI_CmdElementInfo* inf;
	int infIndex;
	
	if(HT_get(&gm->cmdElementLookup, elemname, &infIndex)) {
		L1("Unknown command element name: '%s' trying to add '%s'\n", elemname, name);
		return 1;
	}
	inf = &VEC_ITEM(&gm->cmdElements, infIndex);
	
	HT_set(&inf->nameLookup, name, id);
	
	return 0;
}


// returns zero on success
int GUIManager_AddCommandElement(GUIManager* gm, char* name, uint16_t id) {
	GUI_CmdElementInfo inf;
	int infIndex;
	
	if(id > 0xfff0) {
		L1("GUI: Too many command elements trying to add '%s'\n", name);
		return 0;
	}
	
	//printf("adding %s to command elements\n", name);
	inf.id = id;
	HT_init(&inf.nameLookup, 16);
	
	infIndex = VEC_LEN(&gm->cmdElements);
	HT_set(&gm->cmdElementLookup, name, infIndex);
	VEC_PUSH(&gm->cmdElements, inf);
	
	return 0; 
}


uint16_t GUIManager_AddCommandMode(GUIManager* gm, char* name) {
//	uint16_t mode = gm->cmdModeLookup.fill + 1;
	
//	if(mode > 0xfff0) {
//		printf("GUI: Too many command modes trying to add '%s'\n", name);
//		return 0;
//	}
//	
//	HT_set(&gm->cmdModeLookup, name, mode);  // TODO IMGUI
	
//	return mode;
return 0;  // TODO IMGUI
}

uint32_t GUIManager_AddCommandFlag(GUIManager* gm, char* name) {
	/*  // TODO IMGUI
	uint32_t flag = 1 << gm->nextCmdFlagBit;
	gm->nextCmdFlagBit++;
	
	if(gm->nextCmdFlagBit > 31) {
		printf("GUI: Too many command flags trying to add '%s'\n", name);
		return 0;
	}
	
//	HT_set(&gm->cmdFlagLookup, name, flag);  // TODO IMGUI
	
	return flag;
	*/ return 0;
}

GUI_CmdModeInfo* Commands_GetModeInfo(GUIManager* gm, int id) {
	// TODO: better data structure
	VEC_EACHP(&gm->commandModes, i, mp) {
		if(mp->id == id) return mp;
	}

	return NULL;
}


void CommandList_loadJSONFile(GUIManager* gm, char* path) {
	json_file_t* jsf;

	jsf = json_load_path(path);
	CommandList_loadJSON(gm, jsf->root);
	json_file_free(jsf);
}

void CommandList_loadJSON(GUIManager* gm, json_value_t* root) {
	json_value_t* cmds_v, *elems_v, *modes_v, *keys_v;
	
	cmds_v = json_obj_get_val(root, "commands");
	if(!cmds_v) return;
	
	
	// element metadata
	elems_v = json_obj_get_val(cmds_v, "elements");
	if(elems_v && elems_v->type == JSON_TYPE_ARRAY) {
		
		json_link_t* link = elems_v->arr.head;
		json_value_t* v;
		for(;link; link = link->next) {
			char* ename, *defkey;
			GUI_CmdElementInfo* inf;
			int infIndex;
			uint32_t n32;
			
			ename = json_obj_get_str(link->v, "elem");
			if(!ename) continue;
			
			if(HT_get(&gm->cmdElementLookup, ename, &infIndex)) {
				L1("Unknown element name: '%s'\n", ename);
				continue;
			}
			inf = &VEC_ITEM(&gm->cmdElements, infIndex); // TODO IMGUI
			
		}
	}
	
	
	// modes metadata
	modes_v = json_obj_get_val(cmds_v, "modes");
	if(modes_v && modes_v->type == JSON_TYPE_OBJ) {
		json_value_t* v;
		
		char* key;
		void* iter = NULL;
		while(json_obj_next(modes_v, &iter, &key, &v)) {
		
			int id = strtol(key, NULL, 10);
			
			GUI_CmdModeInfo* cmi = Commands_GetModeInfo(gm, id);
			if(!cmi) {
				VEC_PUSH(&gm->commandModes, (GUI_CmdModeInfo){0});
				cmi = &VEC_TAIL(&gm->commandModes);
				cmi->id = id;
				cmi->parent = -1;
			}
		
			
			// look for a mode name string
			char* mname = json_obj_get_str(v, "name");
			if(mname) {
				if(cmi->name) {
					if(strcmp(mname, cmi->name)) {
						L2("Overwriting command mode %d name '%s' with '%s'\n", id, cmi->name, mname);
						free(cmi->name);
						cmi->name = strdup(mname);
					}
				}
				else {
					cmi->name = strdup(mname);
				}
			}
			
			// check for a parent node to fall back to
			cmi->parent = json_obj_get_int(v, "parent", -1);
		}
	}


	// keystroke config
	keys_v = json_obj_get_val(cmds_v, "keyConfig");
	if(keys_v) {
		CommandList_loadKeyConfigJSON(gm, keys_v);
	}
	
}




	
static int read_command_entry(GUIManager* gm, json_value_t* entry, GUI_Cmd* cmd, int validate) {
	char* s;
	json_value_t* v;
	GUI_CmdElementInfo* inf, *inf2;
	int infIndex, infIndex2;
	
	uint16_t n16;
	uint32_t n32;
	
	// element name
	s = json_obj_get_str(entry, "elem");
	if(s == NULL && validate && cmd->element == 0) {
		L1("Command List entry missing element name\n");
		return 1;
	}
	else if(s != NULL) {
		char* s1 = strdup(s);
		
		if(HT_get(&gm->cmdElementLookup, s, &infIndex)) {
			L1("Unknown element name: '%s'\n", s);
			return 1;
		}
		inf = &VEC_ITEM(&gm->cmdElements, infIndex);  // TODO IMGUI
		cmd->element = inf->id;
	}
	
	// sub-element name
	s = json_obj_get_str(entry, "sub_elem");
	if(s != NULL) {
		char* s1 = strdup(s);
		
		if(HT_get(&gm->cmdElementLookup, s, &infIndex2)) {
			L1("Unknown sub-element name: '%s'\n", s);
			return 1;
		}
		inf2 = &VEC_ITEM(&gm->cmdElements, infIndex2); // TODO IMGUI
		cmd->sub_elem = inf2->id;
	}
	
	// command enum
	s = json_obj_get_str(entry, "cmd");
	if(s == NULL && validate && cmd->cmd == 0) {
		L1("Command List entry missing cmd name\n");
		return 1;
	}
	else if(s != NULL) {
		char* s2 = strdup(s);
		if(HT_get(&inf->nameLookup, s, &n32)) {
			L1("Unknown command enum: '%s'\n", s);
			return 1;
		}
		cmd->cmd = n32;
	}
	
	// key
	cmd->src_type = GUI_CMD_SRC_KEY;
	
	s = json_obj_get_str(entry, "key");
	if(s == NULL && validate && cmd->keysym == 0) {
		L1("Command List entry missing key\n");
		return 1;
	}
	else if(s != NULL) {
		char* s3 = strdup(s);
		if(*s == 'X' && *(s+1) == 'K') {
			// X11 key macro
			// cat keysymdef.h | grep '#define' | egrep -o 'XK_[^ ]* *[x0-9a-f]*' | sed 's/  */", /g;s/^/{"/;s/$/},/'
			uint64_t n;
			if(HT_get(&syms, s, &n)) {
				L1("Invalid X11 keysym name: '%s'\n", s);
				return 1;
			}
			
			cmd->keysym = n;
		}
		else if(*s == 'V' && *(s+1) == 'K') {
			// virtual key macro
			uint64_t n;
			if(HT_get(&syms, s, &n)) {
				L1("Invalid virtual keysym name: '%s'\n", s);
				return 1;
			}
			
			cmd->keysym = n;
		}
		else if(*s == 'R' && *(s+1) == 'A' && *(s+2) == 'T' && *(s+3) == '_')  {
			// Mouse button
			char* end = NULL;
			int reps = 1;
			int btn_num = strtol(s+4, &end, 10);
			
			if(*end == 'x') {
				reps = strtol(end+1, NULL, 10);
			}
			
		//	printf("RAT_%d\n", btn_num);
			cmd->src_type = GUI_CMD_SRC_CLICK;
			cmd->keysym = GUI_CMD_RATSYM(btn_num, reps);
		//	printf("keysym: %x = %dx%d\n", cmd.keysym, btn_num, reps);
		}
		else { // regular character literal
			cmd->keysym = s[0];
		}
	}
	
	// optional modifiers
	s = json_obj_get_str(entry, "mods");
	if(s) {
		unsigned int m = 0;
		
		for(; *s; s++) {
			if(*s == ' ') { 
				s++;
				break;
			}
			else if(*s == 'L') {
				s++;
					if(*s == 'C') m |= GUIMODKEY_LCTRL | GUIMODKEY_CTRL;
				else if(*s == 'A') m |= GUIMODKEY_LALT | GUIMODKEY_ALT;
				else if(*s == 'S') m |= GUIMODKEY_LSHIFT | GUIMODKEY_SHIFT;
				else if(*s == 'T') m |= GUIMODKEY_LTUX | GUIMODKEY_TUX;
				else if(*s == 'W') m |= GUIMODKEY_LTUX | GUIMODKEY_TUX;
			}
			else if(*s == 'R') {
				s++;
					if(*s == 'C') m |= GUIMODKEY_RCTRL | GUIMODKEY_CTRL;
				else if(*s == 'A') m |= GUIMODKEY_RALT | GUIMODKEY_ALT;
				else if(*s == 'S') m |= GUIMODKEY_RSHIFT | GUIMODKEY_SHIFT;
				else if(*s == 'T') m |= GUIMODKEY_RTUX | GUIMODKEY_TUX;
				else if(*s == 'W') m |= GUIMODKEY_RTUX | GUIMODKEY_TUX;
			}
			else if(*s == 'C') m |= GUIMODKEY_CTRL;
			else if(*s == 'A') m |= GUIMODKEY_ALT;
			else if(*s == 'S') m |= GUIMODKEY_SHIFT;
			else if(*s == 'T') m |= GUIMODKEY_TUX;
			else if(*s == 'W') m |= GUIMODKEY_TUX;
			else {
				L1("Unknown character looking for command modifiers: \"%c\"\n", *s);
				s++;
			}
		}
		
		cmd->mods = m;
	}
	
	// optional amt value (default 0)
	if(!json_obj_get_key(entry, "amt", &v)) {
		if(v->type == JSON_TYPE_STRING) {
			cmd->str = strdup(v->s);
		}
		else if(v->type == JSON_TYPE_ARRAY) {
			// ONLY supports array of strings
			char** z = malloc(sizeof(*z) * (v->len + 1));
			
			int j = 0;
			json_link_t* link = v->arr.head;
			while(link) {
				z[j++] = strdup(link->v->s);
				
				link = link->next;
			}
			z[j] = NULL;
			
			cmd->pstr = z;
		}
		else {
			cmd->amt = json_as_int(v);
		}
	}
	
	// optional mode value (default 0)
	if(!json_obj_get_key(entry, "mode", &v)) {
		cmd->mode = json_as_int(v);
		/*
		s = v->s;
		
		if(HT_get(&gm->cmdModeLookup, s, &n16)) {
			fprintf(stderr, "Unknown mode name: '%s'\n", s);
			continue;
		}*/
		cmd->mode = v->n; //n16;
	}
	
	// optional flag list
	if(!json_obj_get_key(entry, "flags", &v)) {
		unsigned int flags = 0;
		
		if(v->type == JSON_TYPE_ARRAY) {
		
			json_link_t* l2 = v->arr.head;
			for(;l2; l2 = l2->next) {
				
				if(l2->v->type == JSON_TYPE_STRING) {
					uint64_t x;
					if(!HT_get(&flag_lookup, l2->v->s, &x)) {
						flags |= x;
					}
				}
				else {
					L1("Invalid flag format in command list.\n");
				}
			}
			
			cmd->flags = flags;
		}
	}
	
	return 0;
}



static void read_command_list_defaults(GUIManager* gm, json_value_t* list, GUI_Cmd* defaults) {
	json_link_t* link = list->arr.head;
	for(;link; link = link->next) {
		GUI_Cmd cmd = *defaults;
		
		if(read_command_entry(gm, link->v, &cmd, 1)) continue;
		
		// TODO: add the command into the hash table 
		//printf("pushing cmd: %s %s %s %d %x\n", s1, s2, s3, cmd.mode, cmd.mods);
		if(cmd.sub_elem == 0) {
			VEC_PUSH(&gm->cmdList, cmd);
		}
		else {
//			VEC_PUSH(&gm->cmdListSubElems, cmd);
		}
	
	}
}


void CommandList_loadKeyConfigJSON(GUIManager* gm, json_value_t* root) {
	

	
	if(root->type != JSON_TYPE_ARRAY) {
		L1("Command List json root must be an array.\n");
		return;
	}
	
	int i = 0;

	json_link_t* link = root->arr.head;
	json_value_t* v;
	for(;link; link = link->next) {
		GUI_Cmd cmd = {0};
		
		
		
		if(!json_obj_get_key(link->v, "cmd_list", &v) && v->type == JSON_TYPE_ARRAY) {
			if(read_command_entry(gm, link->v, &cmd, 0)) continue;
			read_command_list_defaults(gm, v, &cmd);
		}
		else {
			if(read_command_entry(gm, link->v, &cmd, 1)) continue;
			// TODO: add the command into the hash table 
			//printf("pushing cmd: %s %s %s %d %x\n", s1, s2, s3, cmd.mode, cmd.mods);
			if(cmd.sub_elem == 0) {
				VEC_PUSH(&gm->cmdList, cmd);
			}
			else {
	//			VEC_PUSH(&gm->cmdListSubElems, cmd);
			}
		}
	}
}

