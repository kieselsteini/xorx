/*
========================================================================================================================

	Kingdom of Xorx
	A little action/adventure game like ZZT or Kroz.
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

	MAP_COLS = 256, // map width in tiles
	MAP_ROWS = 256, // map height in tiles
};

#define WINDOW_TITLE "Kingdom of Xorx" // title of the window
#define WINDOW_SCALE 0.8f // scale window to the desktop size

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
	struct {
		uint8_t rand; // current game random "seed"
	} game;
} state;


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


//==[[ Gameplay Routines ]]=============================================================================================

// initialize the game
static void on_init(void) {
}

// run single game tick
static void on_tick(void) {
}


//==[[ Core Engine Routines ]]==========================================================================================

// press will set/unset a button
static void press(const btn_t mask, const bool down) {
	if (down) state.input.down |= mask; else state.input.down &= ~mask;
}

// handle keyboard keys
static void handle_keyboard(const SDL_Keycode key, const bool down) {
	switch (key) {
		case SDLK_ESCAPE: if (down) state.core.running = false; break;
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
