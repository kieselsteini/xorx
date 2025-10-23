/*
========================================================================================================================

	Kingdom of Xorx
	A little action/adventure game like ZZT, Kingdom of Kroz or Dungeons of Grimlor.
	Written by Sebastian Steinhauer <s.steinhauer@yahoo.de> '2025

========================================================================================================================
*/
//==[[ Headers / Defines / Types ]]=====================================================================================

// standard C headers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

// SDL3 headers
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// various defines for engine + game
enum {
	TICK_RATE = 30, // game ticks per second
	TICK_TIME = 1000 / TICK_RATE, // time for a single game tick

	TILE_WIDTH = 8, // tile width in pixels
	TILE_HEIGHT = 8, // tile height in pixels
	VIDEO_COLS = 16 * 2, // video width in tiles
	VIDEO_ROWS = 9 * 2, // video height in tiles
	VIDEO_WIDTH = VIDEO_COLS * TILE_WIDTH, // video width in pixels
	VIDEO_HEIGHT = VIDEO_ROWS * TILE_HEIGHT, // video height in pixels

	AUDIO_RATE = 22050, // audio mixing rate
	AUDIO_BUFFER = 1024 * 2, // audio mixer buffer in samples
	AUDIO_VOICES = 4, // amount of parallel sound effects
	AUDIO_SOUNDS = 32, // number of sound effects

	MAP_COLS = 512, // map width in tiles
	MAP_ROWS = 256, // map height in tiles
	VIEW_COLS = 32, // view width in tiles
	VIEW_ROWS = 16, // view height in tiles
};

#define WINDOW_TITLE "Kingdom of Xorx" // title of the window
#define WINDOW_SCALE 0.8f // scale window to the desktop size

// define the tileset
enum {
	TILE_EMPTY = 0, // empty cell
	// ui stuff
	TILE_BORDER_CORNER, TILE_BORDER_LR, TILE_BORDER_UD,
	TILE_MAP_0, TILE_MAP_1, TILE_MAP_2, TILE_MAP_3, TILE_MAP_4,
	// pickups
	TILE_LIFE = 16, TILE_AMMO, TILE_FLASK, TILE_KEY,
	// walls
	TILE_WALL_0 = 128, TILE_WALL_1, TILE_WALL_2, TILE_WALL_3,
	TILE_RUIN_0, TILE_RUIN_1,
	TILE_TREE_0, TILE_TREE_1, TILE_TREE_2, TILE_TREE_3,
	TILE_GRASS_0, TILE_GRASS_1,
	TILE_WATER_0, TILE_WATER_1, TILE_LAVA_0, TILE_LAVA_1,
	// effects
	TILE_EXPLOSION_0, TILE_EXPLOSION_1, TILE_EXPLOSION_2, TILE_EXPLOSION_3,
	TILE_SPAWN_0, TILE_SPAWN_1, TILE_SPAWN_2, TILE_SPAWN_3,
	// arrows
	TILE_ARROW_N, TILE_ARROW_E, TILE_ARROW_S, TILE_ARROW_W,
	TILE_WARROW_N, TILE_WARROW_E, TILE_WARROW_S, TILE_WARROW_W,
	// monsters
	TILE_MONSTER_0, TILE_MONSTER_1, TILE_MONSTER_2, TILE_MONSTER_3,
	// player sprites
	TILE_PLAYER_STAND, TILE_PLAYER_SHOOT, TILE_PLAYER_MAGIC, TILE_PLAYER_DEFEND,
	// bolts
	TILE_BOLT_N, TILE_BOLT_E, TILE_BOLT_S, TILE_BOLT_W,
	TILE_WBOLT_N, TILE_WBOLT_E, TILE_WBOLT_S, TILE_WBOLT_W,
	// bolt - trap
	TILE_BOLT_TRAP_0, TILE_BOLT_TRAP_1,
	// monster shrine
	TILE_SHRINE_0, TILE_SHRINE_1, TILE_SHRINE_2, TILE_SHRINE_3,
	// teleporter
	TILE_TELEPORT,
	TILE_PSPAWN_0, TILE_PSPAWN_1, TILE_PSPAWN_2, TILE_PSPAWN_3,
	// assorted stuff
	TILE_BOULDER,
	// total solid wall
	TILE_WALL_X = 255,
};

// define sound effects
enum {
	SOUND_EXPLODE,
	SOUND_SPAWN,
	SOUND_SHOOT,
	SOUND_PICKUP,
	SOUND_BOULDER,
	SOUND_TELEPORT,
	SOUND_WON,
	SOUND_PLAYER_MOVED,
	SOUND_PLAYER_BLOCKED,
	SOUND_PLAYER_HURT,
	SOUND_PLAYER_DIED,
	SOUND_MONSTER_HURT,
	SOUND_MONSTER_DIED,
};

// define button bit-masks
typedef enum btn_t {
	BUTTON_NONE = 0, BUTTON_ANY = 255,
	BUTTON_A = 1, BUTTON_B = 2, BUTTON_X = 4, BUTTON_Y = 8,
	BUTTON_UP = 16, BUTTON_DOWN = 32, BUTTON_LEFT = 64, BUTTON_RIGHT = 128
} btn_t;

// define directions (clock-wise)
typedef enum dir_t {
	DIR_NONE, DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST
} dir_t;

// simple 2D vector
typedef struct vec_t {
	int x, y;
} vec_t;

// simple cell on the world
typedef struct cell_t {
	uint8_t tile; // active visible tile on this spot
	uint8_t tick; // when will this cell be active again
} cell_t;

// a sound effect loaded to memory
typedef struct sound_t {
	const int16_t *samples; // decoded 16-bit PCM sample data
	unsigned int length; // length of sound in samples
} sound_t;

// audio mixer channel
typedef struct voice_t {
	sound_t sound; // sound effect to play
	unsigned int position; // current playback position
} voice_t;

// all global state lives in this nested structure
static struct state_t {
	// core system
	struct {
		bool running; // keep the engine running
		jmp_buf error; // error handling routine
	} core;
	// time system
	struct {
		uint64_t tick; // current global engine tick
		uint64_t last; // last measured SDL time
		uint64_t accu; // accumulated delta time between frames
	} time;
	// input system
	struct {
		btn_t down; // buttons which are currently down
		btn_t prev; // buttons which were down last tick
	} input;
	// audio system
	struct {
		SDL_AudioDeviceID device; // SDL audio device object
		SDL_AudioStream *stream; // SDL audio stream object
		voice_t voices[AUDIO_VOICES]; // audio mixer channels
		sound_t sounds[AUDIO_SOUNDS]; // sound effects
		uint32_t playing; // bit-mask of sounds to play
	} audio;
	// video system
	struct {
		SDL_Window *window; // SDL window object
		SDL_Renderer *renderer; // SDL renderer object
		SDL_Texture *texture; // tileset atlas texture
		uint8_t data[VIDEO_ROWS][VIDEO_COLS]; // screen content data
	} video;
	// game system
	struct game_t {
		bool paused; // flag if we are paused
		bool dead; // flag if we are dead
		uint8_t rand; // current game random "seed"
		uint8_t tick; // 8-bit game tick we use for cells
		vec_t player; // current player position
		vec_t view; // current view position
		int life; // current hitpoints
		int ammo; // current ammunition / swords
		int flasks; // current amount of flasks
		int keys; // current amount of keys
		int gold; // current amount of gold
		cell_t cells[MAP_ROWS][MAP_COLS]; // cells of our game world
	} game;
} state;

// define invalid position vector
static const vec_t invalid_position = {-1, -1};


//==[[ Various Routines ]]==============================================================================================

// return minimum integer
static int mini(const int a, const int b) {
	return a < b ? a : b;
}

// return maxium integer
static int maxi(const int a, const int b) {
	return a > b ? a : b;
}

// return clamped integer
static int clampi(const int x, const int min, const int max) {
	return maxi(mini(x, max), min);
}

// return random number for gameplay
static unsigned int rnd(void) {
	// we use a static array of 256 random bytes ... it works for DOOM it works for us :)
	static const uint8_t data[256] = { 87, 31, 118, 249, 64, 152, 247, 255, 254, 202, 250, 123, 39, 194, 240, 135, 117, 130, 66, 219, 48, 225, 37, 237, 105, 176, 78, 198, 99, 85, 3, 34, 61, 96, 50, 45, 43, 136, 203, 23, 119, 132, 175, 131, 178, 19, 36, 70, 241, 183, 140, 161, 199, 67, 155, 86, 220, 223, 65, 233, 71, 2, 192, 35, 244, 134, 166, 141, 236, 186, 46, 116, 184, 195, 205, 179, 181, 30, 109, 215, 245, 206, 228, 191, 187, 15, 115, 20, 93, 145, 113, 60, 151, 231, 137, 83, 209, 174, 59, 62, 89, 22, 51, 177, 114, 129, 7, 169, 171, 126, 18, 79, 160, 16, 180, 163, 232, 207, 144, 1, 246, 230, 94, 122, 167, 172, 104, 0, 128, 72, 90, 12, 76, 196, 41, 190, 193, 52, 149, 68, 189, 73, 100, 95, 218, 121, 156, 33, 108, 8, 157, 63, 77, 150, 139, 138, 162, 107, 82, 88, 200, 234, 74, 28, 110, 54, 229, 4, 84, 133, 239, 103, 125, 211, 153, 159, 197, 29, 102, 27, 142, 24, 158, 253, 222, 217, 204, 148, 147, 170, 213, 111, 226, 208, 56, 168, 143, 6, 165, 201, 47, 112, 92, 251, 13, 212, 55, 242, 188, 91, 80, 146, 210, 243, 235, 81, 124, 252, 14, 238, 221, 127, 5, 53, 106, 214, 227, 42, 101, 57, 38, 21, 9, 97, 40, 44, 248, 164, 98, 75, 32, 154, 11, 10, 182, 224, 173, 17, 185, 25, 58, 26, 216, 120, 69, 49 };
	return data[state.game.rand++];
}

// shortcut to create 2D vector
static vec_t vec2(const int x, const int y) {
	return (vec_t){ .x = x, .y = y };
}

// check if two vectors are equal
static bool veq(const vec_t a, const vec_t b) {
	return (a.x == b.x) && (a.y == b.y);
}

// add two vectors
static vec_t vadd(const vec_t a, const vec_t b) {
	return (vec_t){ .x = a.x + b.x, .y = a.y + b.y };
}

// subtract two vectors
static vec_t vsub(const vec_t a, const vec_t b) {
	return (vec_t){ .x = a.x - b.x, .y = a.y - b.y };
}

// return vector for given direction
static vec_t vdir(const dir_t dir) {
	static const vec_t v[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
	return (dir >= DIR_NORTH) && (dir <= DIR_WEST) ? v[dir - 1] : (vec_t){};
}

// move vector one step in direction
static vec_t vmove(const vec_t v, const dir_t d) {
	return vadd(v, vdir(d));
}

// return the base vector for the given vector (normalized to VIEW_COLS/VIEW_ROWS)
static vec_t vbase(const vec_t v) {
	return (vec_t){ .x = (v.x / VIEW_COLS) * VIEW_COLS, .y = (v.y / VIEW_ROWS) * VIEW_ROWS };
}

// check if button is down
static bool btn(const btn_t mask) {
	return state.input.down & mask;
}

// check if button was just pressed
static bool btnp(const btn_t mask) {
	return (state.input.down & (~state.input.prev)) & mask;
}

// clear the whole screen
static void cls(void) {
	memset(state.video.data, 0, sizeof(state.video.data));
}

// draw single tile to screen
static void draw(const int x, const int y, const uint8_t tile) {
	if ((x >= 0) && (x < VIDEO_COLS) && (y >= 0) && (y < VIDEO_ROWS)) state.video.data[y][x] = tile;
}

// print text to screen
static void print(int x, const int y, const char *text) {
	for (; *text; ++text, ++x) draw(x, y, (uint8_t)*text);
}

// play sound effect
static void sound(const int id) {
	if ((id >= 0) && (id < AUDIO_SOUNDS)) state.audio.playing |= 1 << id;
}

// center text on screen
static void center(const int y, const char *text) {
	print((VIDEO_COLS - strlen(text)) / 2, y, text);
}

// draw a border
static void border(const int x0, const int y0, const int x1, const int y1) {
	for (int x = x0 + 1; x < x1; ++x) { draw(x, y0, TILE_BORDER_LR); draw(x, y1, TILE_BORDER_LR); }
	for (int y = y0 + 1; y < y1; ++y) { draw(x0, y, TILE_BORDER_UD); draw(x1, y, TILE_BORDER_UD); }
	draw(x0, y0, TILE_BORDER_CORNER); draw(x1, y0, TILE_BORDER_CORNER);
	draw(x0, y1, TILE_BORDER_CORNER); draw(x1, y1, TILE_BORDER_CORNER);
}

// return formatted string printf-style
static const char *strf(const char *fmt, ...) {
	static char buffer[1024]; va_list va;
	va_start(va, fmt); vsnprintf(buffer, sizeof(buffer), fmt, va); va_end(va);
	return buffer;
}

// show error message and force the program to shutdown
static _Noreturn void fail(const char *fmt, ...) {
	char message[1024]; va_list va;
	va_start(va, fmt); vsnprintf(message, sizeof(message), fmt, va); va_end(va);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error!", message, state.video.window);
	longjmp(state.core.error, 1);
}

// check if vector is inside the world
static bool inside(const vec_t v) {
	return (v.x >= 0) && (v.x < MAP_COLS) && (v.y >= 0) && (v.y < MAP_ROWS);
}

// visible checks if the current vector is visible
static bool visible(const vec_t v) {
	return veq(vbase(v), state.game.view);
}

// get a cell from the world
static cell_t get(const vec_t v) {
	return inside(v) ? state.game.cells[v.y][v.x] : (cell_t){ .tile = TILE_WALL_0 };
}

// put a cell to world
static void put(const vec_t v, const cell_t c) {
	if (inside(v)) state.game.cells[v.y][v.x] = c;
}

// clear will clear a cell
static void clear(const vec_t v) {
	put(v, (cell_t){});
}

// shape will shape a cell with tile and given ticks to activate again
static void shape(const vec_t v, const uint8_t tile, const uint8_t ticks) {
	put(v, (cell_t){ .tile = tile, .tick = state.game.tick + ticks });
}

// hibernate cell
static void hibernate(const vec_t v) {
	const cell_t cell = get(v);
	put(v, (cell_t){ .tile = cell.tile, .tick = (cell.tick + 256 - state.game.tick) % 256 });
}

// explode a cell
static void explode(const vec_t v) {
	shape(v, TILE_EXPLOSION_0, 2);
	sound(SOUND_EXPLODE);
}

// return random direction
static dir_t random_dir(void) {
	return DIR_NORTH+rnd()%4;
}

// return direction based on player input
static dir_t input_dir(void) {
	if (btn(BUTTON_UP)) return DIR_NORTH;
	if (btn(BUTTON_DOWN)) return DIR_SOUTH;
	if (btn(BUTTON_LEFT)) return DIR_WEST;
	if (btn(BUTTON_RIGHT)) return DIR_EAST;
	return DIR_NONE;
}

// return direction to walk from src -> dst
static dir_t chase_dir(const vec_t src, const vec_t dst) {
	vec_t delta = vsub(dst, src);
	if (delta.x && delta.y) { if (rnd()%2) delta.x = 0; else delta.y = 0; }
	if (delta.x < 0) return DIR_WEST;
	if (delta.x > 0) return DIR_EAST;
	if (delta.y < 0) return DIR_NORTH;
	if (delta.y > 0) return DIR_SOUTH;
	return DIR_NONE;
}


//==[[ Gameplay Routines ]]=============================================================================================

// returns true if there is a wall
static bool iswall(const vec_t v) {
	switch (get(v).tile) {
		case TILE_WALL_0: case TILE_WALL_1: case TILE_WALL_2: case TILE_WALL_3: case TILE_WALL_X:
			return true;
		default:
			return false;
	}
}

// returns true if tile is completely surrounded by walls
static bool enclosed(const vec_t src) {
	for (int y = -1; y <= 1; ++y) {
		for (int x = -1; x <= 1; ++x) {
			if (!iswall(vadd(src, vec2(x, y)))) return false;
		}
	}
	return true;
}

// start new game will start a completely new game
static void start_game(void) {
	// setup new game state
	state.game = (struct game_t){
		.player = invalid_position,
		.life = 10,
		.ammo = 5,
	};
	// load "world.bmp" and map the colors
	SDL_Surface *surface = SDL_LoadBMP("world.bmp");
	if (!surface) return;
	if ((surface->w != MAP_COLS) || (surface->h != MAP_ROWS)) {
		SDL_DestroySurface(surface);
		fail("Level has invalid size");
	}
	for (int y = 0; y < MAP_ROWS; ++y) {
		for (int x = 0; x < MAP_COLS; ++x) {
			Uint8 r, g, b;
			const vec_t v = vec2(x, y);
			SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, NULL);
			switch ((r << 16) | (g << 8) | b) {
				default: /* floor */ clear(v); break;
				case 0x4e4a4e: /* walls */ shape(v, TILE_WALL_0 + rnd()%4, 0); break;
				case 0x8595a1: /* boulder */ shape(v, TILE_BOULDER, 0); break;
				case 0x70402a: /* ruin */ shape(v, TILE_RUIN_0 + rnd()%2, 0); break;
				case 0x004000: /* tree */ shape(v, TILE_TREE_0 + rnd()%2, 0); break;
				case 0x4a2a1b: /* dead tree */ shape(v, TILE_TREE_2 + rnd()%2, 0); break;
				case 0x008000: /* grass*/ shape(v, TILE_GRASS_0 + rnd()%2, 0); break;
				case 0x000096: /* water */ shape(v, TILE_WATER_0 + rnd()%2, 16); break;
				case 0xffffff: /* player */ shape(v, TILE_PLAYER_STAND, 1); state.game.player = v; break;
				case 0x400000: /* monster 0 */ shape(v, TILE_MONSTER_0, 1); break;
				case 0x800000: /* monster 1 */ shape(v, TILE_MONSTER_1, 1); break;
				case 0xc00000: /* monster 2 */ shape(v, TILE_MONSTER_2, 1); break;
				case 0xff0000: /* monster 3 */ shape(v, TILE_MONSTER_3, 1); break;
				case 0xff8000: /* bolt trap */ shape(v, TILE_BOLT_TRAP_0, rnd()%16); break;
				case 0xff6400: /* shrine */ shape(v, TILE_SHRINE_0, 30+rnd()%16); break;
				case 0x6dc2ca: /* teleport */ shape(v, TILE_TELEPORT, 0); break;
			}
		}
	}
	SDL_DestroySurface(surface);
	state.game.view = vbase(state.game.player);
	// place solid walls (wall x)
	for (int y = 0; y < MAP_ROWS; ++y) {
		for (int x = 0; x < MAP_COLS; ++x) {
			const vec_t v = vec2(x, y);
			if (enclosed(v)) shape(v, TILE_WALL_X, 0);
		}
	}
}

// hurt the player
static void hurt(const int damage) {
	if (damage < state.game.life) {
		state.game.life -= damage;
		sound(SOUND_PLAYER_HURT);
		shape(state.game.player, TILE_PLAYER_DEFEND, 5);
	} else {
		state.game.life = 0;
		state.game.dead = true;
		sound(SOUND_PLAYER_DIED);
		explode(state.game.player);
	}
}

// teleport the player
static bool teleport(const vec_t src, const dir_t dir) {
	for (vec_t dst = vmove(src, dir); inside(dst); dst = vmove(dst, dir)) {
		if (get(dst).tile == TILE_TELEPORT) {
			sound(SOUND_TELEPORT);
			clear(state.game.player);
			state.game.player = vmove(dst, dir);
			state.game.view = vbase(state.game.player);
			shape(state.game.player, TILE_PSPAWN_0, 2);
			return true;
		}
	}
	return false;
}

// push a boulder
static bool push(const vec_t src, const dir_t dir) {
	const vec_t dst = vmove(src, dir);
	switch (get(dst).tile) {
		default:
			return false;
		case TILE_EMPTY:
			clear(src);
			shape(dst, TILE_BOULDER, 0);
			sound(SOUND_BOULDER);
			return true;
		case TILE_MONSTER_0:
		case TILE_MONSTER_1:
		case TILE_MONSTER_2:
		case TILE_MONSTER_3:
			sound(SOUND_BOULDER);
			sound(SOUND_MONSTER_DIED);
			explode(dst);
			return false;
		case TILE_WATER_0:
		case TILE_WATER_1:
			clear(src);
			explode(dst);
			sound(SOUND_BOULDER);
			return false;
	}
}

// update arrows
static void update_arrow(const vec_t src, const dir_t dir, const bool water) {
	if (water) shape(src, TILE_WATER_0+rnd()%2, 16+rnd()%8); else clear(src);
	const vec_t dst = vmove(src, dir);
	if (!visible(dst)) return;
	const cell_t cell = get(dst);
	switch (cell.tile) {
		case TILE_EMPTY:
		case TILE_EXPLOSION_0:
		case TILE_EXPLOSION_1:
		case TILE_EXPLOSION_2:
		case TILE_EXPLOSION_3:
			shape(dst, TILE_ARROW_N + dir - 1, 2);
			break;
		case TILE_WATER_0:
		case TILE_WATER_1:
			shape(dst, TILE_WARROW_N + dir - 1, 2);
			break;
		case TILE_MONSTER_0:
			sound(SOUND_MONSTER_DIED);
			explode(dst);
			break;
		case TILE_MONSTER_1:
		case TILE_MONSTER_2:
		case TILE_MONSTER_3:
			sound(SOUND_MONSTER_HURT);
			put(dst, (cell_t){ .tile = cell.tile - 1, .tick = cell.tick });
			break;
		case TILE_GRASS_0:
		case TILE_GRASS_1:
		case TILE_RUIN_0:
		case TILE_RUIN_1:
			explode(dst);
			break;
	}
}

// update bolts
static void update_bolt(const vec_t src, const dir_t dir, const bool water) {
	if (water) shape(src, TILE_WATER_0+rnd()%2, 16+rnd()%8); else clear(src);
	const vec_t dst = vmove(src, dir);
	if (!visible(dst)) return;
	const cell_t cell = get(dst);
	switch (cell.tile) {
		case TILE_EMPTY:
		case TILE_EXPLOSION_0:
		case TILE_EXPLOSION_1:
		case TILE_EXPLOSION_2:
		case TILE_EXPLOSION_3:
			shape(dst, TILE_BOLT_N + dir - 1, 2);
			break;
		case TILE_WATER_0:
		case TILE_WATER_1:
			shape(dst, TILE_WBOLT_N + dir - 1, 2);
			break;
		case TILE_MONSTER_0:
			sound(SOUND_MONSTER_DIED);
			explode(dst);
			break;
		case TILE_MONSTER_1:
		case TILE_MONSTER_2:
		case TILE_MONSTER_3:
			sound(SOUND_MONSTER_HURT);
			put(dst, (cell_t){ .tile = cell.tile - 1, .tick = cell.tick });
			break;
		case TILE_PLAYER_STAND:
		case TILE_PLAYER_SHOOT:
		case TILE_PLAYER_MAGIC:
		case TILE_PLAYER_DEFEND:
			hurt(5);
			break;
	}
}

// update the bolt trap
static void update_bolt_trap(const vec_t src, const cell_t cell) {
	if (cell.tile == TILE_BOLT_TRAP_0) {
		shape(src, TILE_BOLT_TRAP_1, 4);
	} else {
		for (dir_t dir = DIR_NORTH; dir <= DIR_WEST; ++dir) update_bolt(src, dir, false);
		shape(src, TILE_BOLT_TRAP_0, 30-4);
	}
}

// update monster shrine
static void update_shrine(const vec_t src, const cell_t cell) {
	if (cell.tile == TILE_SHRINE_3) {
		shape(src, TILE_SHRINE_0, 60);
		const vec_t dst = vmove(src, random_dir());
		if (get(dst).tile == TILE_EMPTY) {
			sound(SOUND_SPAWN);
			shape(dst, TILE_SPAWN_0, 2);
		}
	} else {
		shape(src, cell.tile + 1, 60);
	}
}

// update monster
static void update_monster(const vec_t src, const cell_t cell) {
	const vec_t dst = vmove(src, chase_dir(src, state.game.player));
	switch (get(dst).tile) {
		case TILE_EMPTY:
			clear(src);
			shape(dst, cell.tile, 16);
			break;
		case TILE_PLAYER_STAND:
		case TILE_PLAYER_SHOOT:
		case TILE_PLAYER_MAGIC:
		case TILE_PLAYER_DEFEND:
			hurt(cell.tile - TILE_MONSTER_0 + 1);
			explode(src);
			shape(dst, TILE_PLAYER_DEFEND, 5);
			break;
		default:
			shape(src, cell.tile, 16);
			break;
	}
}

// update player
static void update_player(const vec_t src) {
	const dir_t dir = input_dir();
	const vec_t dst = vmove(src, dir);

	// shoot
	if (btn(BUTTON_A)) {
		if (dir != DIR_NONE) {
			sound(SOUND_SHOOT);
			update_arrow(src, dir, false);
			shape(src, TILE_PLAYER_SHOOT, 20);
			return;
		}
		shape(src, TILE_PLAYER_SHOOT, 1);
		return;
	}

	// walk
	if (dir == DIR_NONE) {
		shape(src, TILE_PLAYER_STAND, 1);
		state.game.player = src;
		return;
	}
	const cell_t cell = get(dst);
	switch (cell.tile) {
		default:
			goto blocked;
		case TILE_EMPTY:
			break;
		case TILE_GRASS_0:
		case TILE_GRASS_1:
			explode(dst);
			goto blocked;
		case TILE_MONSTER_0:
		case TILE_MONSTER_1:
		case TILE_MONSTER_2:
		case TILE_MONSTER_3:
			hurt(cell.tile - TILE_MONSTER_0 + 1);
			sound(SOUND_MONSTER_DIED);
			explode(dst);
			goto blocked;
		case TILE_TELEPORT:
			if (teleport(dst, dir)) return;
			goto blocked;
		case TILE_BOULDER:
			if (!push(dst, dir)) goto blocked;
			clear(src);
			shape(dst, TILE_PLAYER_STAND, 10);
			sound(SOUND_PLAYER_MOVED);
			return;
		case TILE_LIFE:
			state.game.life = mini(999, state.game.life + 5);
			sound(SOUND_PICKUP);
			break;
		case TILE_AMMO:
			state.game.ammo = mini(999, state.game.ammo + 5);
			sound(SOUND_PICKUP);
			break;
		case TILE_FLASK:
			state.game.flasks = mini(999, state.game.flasks + 1);
			sound(SOUND_PICKUP);
			break;
	}

	// move player to new position
	clear(src);
	shape(dst, TILE_PLAYER_STAND, 5);
	sound(SOUND_PLAYER_MOVED);
	state.game.player = dst;
	return;

blocked:
	// movement was blocked
	shape(src, TILE_PLAYER_STAND, 1);
	sound(SOUND_PLAYER_BLOCKED);
	state.game.player = src;
	return;
}

// handle single cell
static void update_cell(const vec_t v) {
	const cell_t cell = get(v);
	if (cell.tick != state.game.tick) return;
	switch (cell.tile) {
		// player
		case TILE_PLAYER_STAND:
		case TILE_PLAYER_SHOOT:
		case TILE_PLAYER_MAGIC:
		case TILE_PLAYER_DEFEND:
			update_player(v);
			break;
		// monsters
		case TILE_MONSTER_0:
		case TILE_MONSTER_1:
		case TILE_MONSTER_2:
		case TILE_MONSTER_3:
			update_monster(v, cell);
			break;
		// arrows
		case TILE_ARROW_N: update_arrow(v, DIR_NORTH, false); break;
		case TILE_ARROW_E: update_arrow(v, DIR_EAST, false); break;
		case TILE_ARROW_S: update_arrow(v, DIR_SOUTH, false); break;
		case TILE_ARROW_W: update_arrow(v, DIR_WEST, false); break;
		case TILE_WARROW_N: update_arrow(v, DIR_NORTH, true); break;
		case TILE_WARROW_E: update_arrow(v, DIR_EAST, true); break;
		case TILE_WARROW_S: update_arrow(v, DIR_SOUTH, true); break;
		case TILE_WARROW_W: update_arrow(v, DIR_WEST, true); break;
		// bolts
		case TILE_BOLT_N: update_bolt(v, DIR_NORTH, false); break;
		case TILE_BOLT_E: update_bolt(v, DIR_EAST, false); break;
		case TILE_BOLT_S: update_bolt(v, DIR_SOUTH, false); break;
		case TILE_BOLT_W: update_bolt(v, DIR_WEST, false); break;
		case TILE_WBOLT_N: update_bolt(v, DIR_NORTH, true); break;
		case TILE_WBOLT_E: update_bolt(v, DIR_EAST, true); break;
		case TILE_WBOLT_S: update_bolt(v, DIR_SOUTH, true); break;
		case TILE_WBOLT_W: update_bolt(v, DIR_WEST, true); break;
		// bolt trap
		case TILE_BOLT_TRAP_0:
		case TILE_BOLT_TRAP_1:
			update_bolt_trap(v, cell);
			break;
		// water
		case TILE_WATER_0: shape(v, TILE_WATER_1, 16 + rnd()%8); break;
		case TILE_WATER_1: shape(v, TILE_WATER_0, 16+rnd()%8); break;
		// explosions
		case TILE_EXPLOSION_0: shape(v, TILE_EXPLOSION_1, 2); break;
		case TILE_EXPLOSION_1: shape(v, TILE_EXPLOSION_2, 2); break;
		case TILE_EXPLOSION_2: shape(v, TILE_EXPLOSION_3, 2); break;
		case TILE_EXPLOSION_3: clear(v); break;
		// spawns
		case TILE_SPAWN_0: shape(v, TILE_SPAWN_1, 2); break;
		case TILE_SPAWN_1: shape(v, TILE_SPAWN_2, 2); break;
		case TILE_SPAWN_2: shape(v, TILE_SPAWN_3, 2); break;
		case TILE_SPAWN_3: shape(v, TILE_MONSTER_0+rnd()%4, 10); break;
		case TILE_PSPAWN_0: shape(v, TILE_PSPAWN_1, 2); break;
		case TILE_PSPAWN_1: shape(v, TILE_PSPAWN_2, 2); break;
		case TILE_PSPAWN_2: shape(v, TILE_PSPAWN_3, 2); break;
		case TILE_PSPAWN_3: shape(v, TILE_PLAYER_STAND, 1); break;
		// shrines
		case TILE_SHRINE_0:
		case TILE_SHRINE_1:
		case TILE_SHRINE_2:
		case TILE_SHRINE_3:
			update_shrine(v, cell);
			break;
	}
}

// update the whole game
static void update_game(void) {
	// check for dead
	if (state.game.dead && btnp(BUTTON_A)) start_game();

	// check for pause
	if (!state.game.dead && btnp(BUTTON_X)) state.game.paused = !state.game.paused;
	if (state.game.paused) return;

	// check if we have to move the screen
	const vec_t base = vbase(state.game.player);
	if (!veq(base, state.game.view)) {
		if (state.game.view.x < base.x) state.game.view.x += 2;
		if (state.game.view.x > base.x) state.game.view.x -= 2;
		if (state.game.view.y < base.y) state.game.view.y += 1;
		if (state.game.view.y > base.y) state.game.view.y -= 1;
		return;
	}

	// update the visible part of the map
	for (int y = 0; y < VIEW_ROWS; ++y) {
		for (int x = 0; x < VIEW_COLS; ++x) {
			update_cell(vadd(base, vec2(x, y)));
		}
	}

	// check if we have left the screen
	if (!veq(base, vbase(state.game.player))) {
		// hibernate the old screen
		for (int y = 0; y < VIEW_ROWS; ++y) {
			for (int x = 0; x < VIEW_COLS; ++x) {
				hibernate(vadd(base, vec2(x, y)));
			}
		}
		hibernate(state.game.player);
		state.game.tick = 0;
	} else {
		state.game.tick++;
	}
}

// draw the whole game
static void draw_game(void) {
	cls(); border(0, VIEW_ROWS, VIDEO_COLS - 1, VIEW_ROWS);
	// draw play screen
	for (int y = 0; y < VIEW_ROWS; ++y) {
		for (int x = 0; x < VIEW_COLS; ++x) {
			const cell_t cell = get(vadd(state.game.view, vec2(x, y)));
			draw(x, y, cell.tile);
		}
	}
	// draw hud
	center(VIDEO_ROWS - 1, strf("%c%-3d %c%-3d %c%-3d",
		TILE_LIFE, state.game.life,
		TILE_AMMO, state.game.ammo,
		TILE_FLASK, state.game.flasks
	));
	if (state.game.dead) {
		center(VIDEO_ROWS - 2, "\01 YOU DIED! \01");
	} else if (state.game.paused) {
		const int x0 = (VIDEO_COLS - (MAP_COLS / VIEW_COLS / 2)) / 2;
		const int y0 = (VIDEO_ROWS - (MAP_ROWS / VIEW_ROWS / 2)) / 2 - 1;
		border(x0 - 1, y0 - 1, x0 + (MAP_COLS / VIDEO_COLS / 2), y0 + (MAP_ROWS / VIEW_ROWS / 2));
		for (int y = 0; y < MAP_ROWS / VIEW_ROWS / 2; ++y) {
			for (int x = 0; x < MAP_COLS / VIEW_COLS / 2; ++x) {
				draw(x0 + x, y0 + y, TILE_MAP_0);
			}
		}
		const vec_t v = vec2(state.game.player.x / VIEW_COLS, state.game.player.y / VIEW_ROWS);
		draw(x0 + v.x / 2, y0 + v.y / 2, TILE_MAP_1 + (v.y % 2) * 2 + (v.x % 2));
	}
}

// initialize the game
static void on_init(void) {
	start_game();
}

// run single game tick
static void on_tick(void) {
	update_game();
	draw_game();
}


//==[[ Core Engine Routines ]]==========================================================================================

// screenshot will take a screenshot
static void screenshot(void) {
	SDL_Surface *tileset = SDL_LoadBMP("tiles.bmp");
	SDL_Surface *surface = SDL_CreateSurface(VIDEO_WIDTH * 3, VIDEO_HEIGHT * 3, SDL_PIXELFORMAT_RGB24);
	for (int y = 0; y < VIDEO_ROWS; ++y) {
		for (int x = 0; x < VIDEO_COLS; ++x) {
			const unsigned int tile = state.video.data[y][x];
			const SDL_Rect src = { .x = (tile % 16) * TILE_WIDTH, .y = (tile / 16) * TILE_HEIGHT, .w = TILE_WIDTH, .h = TILE_HEIGHT };
			const SDL_Rect dst = { .x = x * TILE_WIDTH * 3, .y = y * TILE_HEIGHT * 3, .w = TILE_WIDTH * 3, .h = TILE_HEIGHT * 3 };
			SDL_BlitSurfaceScaled(tileset, &src, surface, &dst, SDL_SCALEMODE_NEAREST);
		}
	}
	SDL_SaveBMP(surface, "screenshot.bmp");
	SDL_DestroySurface(surface);
	SDL_DestroySurface(tileset);
}

// press will set/unset a button
static void press(const btn_t mask, const bool down) {
	if (down) state.input.down |= mask; else state.input.down &= ~mask;
}

// handle keyboard keys
static void handle_keyboard(const SDL_Keycode key, const bool down) {
	switch (key) {
		case SDLK_ESCAPE: if (down) state.core.running = false; break;
		case SDLK_F12: if (down) screenshot(); break;
		case SDLK_W: case SDLK_8: case SDLK_KP_8: case SDLK_UP: press(BUTTON_UP, down); break;
		case SDLK_S: case SDLK_2: case SDLK_KP_2: case SDLK_DOWN: press(BUTTON_DOWN, down); break;
		case SDLK_A: case SDLK_4: case SDLK_KP_4: case SDLK_LEFT: press(BUTTON_LEFT, down); break;
		case SDLK_D: case SDLK_6: case SDLK_KP_6: case SDLK_RIGHT: press(BUTTON_RIGHT, down); break;
		case SDLK_I: case SDLK_RETURN: case SDLK_RETURN2: press(BUTTON_A, down); break;
		case SDLK_O: case SDLK_SPACE: press(BUTTON_B, down); break;
		case SDLK_K: press(BUTTON_X, down); break;
		case SDLK_L: press(BUTTON_Y, down); break;
	}
}

// handle gamepad buttons
static void handle_gamepad(const int button, const bool down) {
	switch (button) {
		case SDL_GAMEPAD_BUTTON_DPAD_UP: press(BUTTON_UP, down); break;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN: press(BUTTON_DOWN, down); break;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT: press(BUTTON_LEFT, down); break;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: press(BUTTON_RIGHT, down); break;
		case SDL_GAMEPAD_BUTTON_NORTH: press(BUTTON_Y, down); break;
		case SDL_GAMEPAD_BUTTON_SOUTH: press(BUTTON_A, down); break;
		case SDL_GAMEPAD_BUTTON_WEST: press(BUTTON_X, down); break;
		case SDL_GAMEPAD_BUTTON_EAST: press(BUTTON_Y, down); break;
	}
}

// update timer and ticks
static void update_ticks(void) {
	const uint64_t now = SDL_GetTicks();
	state.time.accu += now - state.time.last;
	state.time.last = now;
	for (; state.time.accu >= TICK_TIME; state.time.accu -= TICK_TIME) {
		on_tick();
		state.time.tick++;
		state.input.prev = state.input.down;
	}
}

// update the audio stream
static void update_audio(void) {
	// check the playing bit-mask and place sound effects into the mixer channels
	if (state.audio.playing) {
		for (int i = 0, j = 0; (i < AUDIO_SOUNDS) && (j < AUDIO_VOICES); ++i) {
			if ((state.audio.playing & (1 << i)) == 0) continue;
			const sound_t sound = state.audio.sounds[i];
			if (!sound.samples) continue;
			for (; j < AUDIO_VOICES; ++j) {
				voice_t *voice = &state.audio.voices[j];
				if (!voice->sound.samples) { *voice = (voice_t){ .sound = sound }; ++j; break; }
			}
		}
		state.audio.playing = 0;
	}
	// check if we have to "render" more audio data
	if (SDL_GetAudioStreamAvailable(state.audio.stream) < (int)(AUDIO_BUFFER * sizeof(int16_t))) {
		int16_t buffer[AUDIO_BUFFER];
		for (int i = 0; i < AUDIO_BUFFER; ++i) {
			int total = 0;
			for (int j = 0; j < AUDIO_VOICES; ++j) {
				voice_t *voice = &state.audio.voices[j];
				if (voice->sound.samples) {
					if (voice->position < voice->sound.length) {
						total += voice->sound.samples[voice->position++];
					} else {
						*voice = (voice_t){};
					}
				}
			}
			buffer[i] = (int16_t)clampi(total, -32768, 32767);
		}
		if (!SDL_PutAudioStreamData(state.audio.stream, buffer, sizeof(buffer))) fail("SDL_PutAudioStreamData() error: %s", SDL_GetError());
	}
}

// update video rendering
static void update_video(void) {
	if (!SDL_RenderClear(state.video.renderer)) fail("SDL_RenderClear() error: %s", SDL_GetError());
	for (int y = 0; y < VIDEO_ROWS; ++y) {
		for (int x = 0; x < VIDEO_COLS; ++x) {
			const unsigned int tile = state.video.data[y][x];
			const SDL_FRect src = { .x = (unsigned int)((tile % 16) * TILE_WIDTH), .y = (unsigned int)((tile / 16) * TILE_HEIGHT), .w = TILE_WIDTH, .h = TILE_HEIGHT };
			const SDL_FRect dst = { .x = x * TILE_WIDTH, .y = y * TILE_HEIGHT, .w = TILE_WIDTH, .h = TILE_HEIGHT };
			SDL_RenderTexture(state.video.renderer, state.video.texture, &src, &dst);
		}
	}
	if (!SDL_RenderPresent(state.video.renderer)) fail("SDL_RenderPresent() error: %s", SDL_GetError());
}

// load tileset
static SDL_Texture *load_tiles(const char *name) {
	SDL_Surface *surface = SDL_LoadBMP(name);
	if (!surface) return NULL;
	if ((surface->w != 16 * TILE_WIDTH) || (surface->h != 16 * TILE_HEIGHT)) {
		SDL_DestroySurface(surface);
		fail("Tiles(%s) has wrong size");
	}
	Uint8 r, g, b;
	if (SDL_ReadSurfacePixel(surface, 0, 0, &r, &g, &b, NULL)) SDL_SetRenderDrawColor(state.video.renderer, r, g, b, 255);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(state.video.renderer, surface);
	SDL_DestroySurface(surface);
	if (!texture) fail("SDL_CreateTextureFromSurface() error: %s", SDL_GetError());
	if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) fail("SDL_SetTextureScaleMode() error: %s", SDL_GetError());
	return texture;
}

// load sound effect
static sound_t load_sound(const char *name) {
	SDL_AudioSpec spec; Uint8 *data; Uint32 length;
	if (!SDL_LoadWAV(name, &spec, &data, &length)) return (sound_t){};
	if ((spec.format != SDL_AUDIO_S16) || (spec.freq != AUDIO_RATE) || (spec.channels != 1)) {
		SDL_free(data);
		fail("Sound(%s) has wrong format", name);
	}
	return (sound_t){ .samples = (int16_t*)data, .length = length / sizeof(int16_t) };
}

// callback to initialize the application
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
	(void)appstate; (void)argc; (void)argv;
	// init core system
	state = (struct state_t){ .core.running = true };
	if (setjmp(state.core.error)) return SDL_APP_FAILURE;
	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) fail("SDL_Init() error: %s", SDL_GetError());

	// init video system
	int w = VIDEO_WIDTH, h = VIDEO_HEIGHT;
	const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(1);
	if (dm) {
		const int factor = maxi(1, mini((dm->w * WINDOW_SCALE) / VIDEO_WIDTH, (dm->h * WINDOW_SCALE) / VIDEO_HEIGHT));
		w *= factor; h *= factor;
	}
	if (!(state.video.window = SDL_CreateWindow(WINDOW_TITLE, w, h, SDL_WINDOW_RESIZABLE))) fail("SDL_CreateWindow() error: %s", SDL_GetError());
	if (!(state.video.renderer = SDL_CreateRenderer(state.video.window, NULL))) fail("SDL_CreateRenderer() error: %s", SDL_GetError());
	if (!SDL_SetRenderVSync(state.video.renderer, 1)) fail("SDL_SetRenderVSync() error: %s", SDL_GetError());
	if (!SDL_SetRenderLogicalPresentation(state.video.renderer, VIDEO_WIDTH, VIDEO_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX)) fail("SDL_SetRenderLogicalPresentation() error: %s", SDL_GetError());

	// init audio system
	const SDL_AudioSpec want = { .format = SDL_AUDIO_S16, .freq = AUDIO_RATE, .channels = 1 };
	if (!(state.audio.device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL))) fail("SDL_OpenAudioDevice() error: %s", SDL_GetError());
	if (!(state.audio.stream = SDL_CreateAudioStream(&want, NULL))) fail("SDL_CreateAudioStream() error: %s", SDL_GetError());
	if (!SDL_BindAudioStream(state.audio.device, state.audio.stream)) fail("SDL_BindAudioStream() error: %s", SDL_GetError());

	// init assets
	state.video.texture = load_tiles("tiles.bmp");
	for (int i = 0; i < AUDIO_SOUNDS; ++i) state.audio.sounds[i] = load_sound(strf("sound%02d.wav", i));

	// init game + time system
	on_init();
	state.time.last = SDL_GetTicks();

	return SDL_APP_CONTINUE;
}

// callback to finalize the application
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
	(void)appstate; (void)result;
	// shutdown audio system
	for (int i = 0; i < AUDIO_SOUNDS; ++i) if (state.audio.sounds[i].samples) SDL_free((void*)state.audio.sounds[i].samples);
	if (state.audio.stream) SDL_DestroyAudioStream(state.audio.stream);
	if (state.audio.device) SDL_CloseAudioDevice(state.audio.device);

	// shutdown video system
	if (state.video.texture) SDL_DestroyTexture(state.video.texture);
	if (state.video.renderer) SDL_DestroyRenderer(state.video.renderer);
	if (state.video.window) SDL_DestroyWindow(state.video.window);

	// shutdown core system
	SDL_Quit();
}

// callback for every SDL event
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
	(void)appstate;
	switch (event->type) {
		case SDL_EVENT_QUIT:
			return SDL_APP_SUCCESS;
		case SDL_EVENT_KEY_DOWN:
			handle_keyboard(event->key.key, true);
			break;
		case SDL_EVENT_KEY_UP:
			handle_keyboard(event->key.key, false);
			break;
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			handle_gamepad(event->gbutton.button, true);
			break;
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
			handle_gamepad(event->gbutton.button, false);
			break;
		case SDL_EVENT_GAMEPAD_ADDED:
			SDL_OpenGamepad(event->gdevice.which);
			break;
	}
	return SDL_APP_CONTINUE;
}

// callback for every frame
SDL_AppResult SDL_AppIterate(void *appstate) {
	(void)appstate;
	if (!state.core.running) return SDL_APP_SUCCESS;
	if (setjmp(state.core.error)) return SDL_APP_FAILURE;
	update_ticks();
	update_audio();
	update_video();
	return SDL_APP_CONTINUE;
}
