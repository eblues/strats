#include <conio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

enum tiles_e {
	EMPTY,
	GROUND,
	GRASS,
	O_COVER,
	I_COVER,
	H_COVER,
	SOLDIER,
	ALIEN,
	MAX_TILE
};

#define MIN_COVER_TYPE O_COVER
#define MAX_COVER_TYPE (H_COVER + 1)

char tiles_glyphs[] = {
	' ', /* EMPTY */
	'.', /* GROUND */
	',', /* GRASS */
	219, /* O_COVER */
	221, /* I_COVER */
	192, /* H_COVER */
	'S', /* SOLDIER */
	'X', /* ALIEN */
};

enum directions_e {
	NORTH,
	SOUTH,
	EAST,
	WEST,
	MAX_DIR
};

enum soldier_skill_e {
	RUN_N_GUN,
	CURE,
	GRENADE,
	RANGE,
	MAX_SOLDIER_SKILL
};

enum alien_skill_e {
	MIND_CONTROL,
	JUMP,
	STEALTH,
	POISON,
	MAX_ALIEN_SKILL
};

#define MAX_X		40
#define MAX_Y		25

#define SCREEN_X 	(MAX_X-2)
#define SCREEN_Y 	(MAX_Y-2)

#define CHUNK_SIZE_COLS SCREEN_X
#define CHUNK_SIZE_ROWS (SCREEN_Y-1)

struct entity {
	enum tiles_e type;
	char ref_count;
};

struct soldier {
	struct entity _e;
	char hitpoints;
	char mobility;
	enum soldier_skill_e skill;
};

#define MAX_SOLDIER_HITPOINTS	5
#define MAX_SOLDIER_MOBILITY	((CHUNK_SIZE_COLS + CHUNK_SIZE_ROWS) / 10)

struct cover {
	struct entity _e;
	char high;
	char hitpoints;
};

#define MAX_COVER_HITPOINTS	2

struct alien {
	struct entity _e;
	char hitpoints;
	char mobility;
	enum alien_skill_e skill;
};

#define MAX_ALIEN_HITPOINTS	4
#define MAX_ALIEN_MOBILITY	MAX_SOLDIER_MOBILITY

struct grass {
	struct entity _e;
	char fire;
};

struct ground {
	struct entity _e;
};

struct playfield_chunk {
	struct playfield_chunk *neighs[MAX_DIR];
	struct entity *field[CHUNK_SIZE_COLS][CHUNK_SIZE_ROWS];
};

struct cursor {
	char x, y, enabled;
};

enum game_state_e {
	IDLE,
	FIRING,
};

struct game {
	struct playfield_chunk *pc;
	struct cursor crs;
	struct cursor old_crs;
	unsigned turn;
	const char *status;
	enum game_state_e state;
	struct entity *actor;
	char actor_x, actor_y;
};

void draw(struct game *g);
void draw_board(const struct game *g);
struct playfield_chunk *generate_pc(void);
struct entity *make_basic(enum tiles_e type, size_t size);
struct grass *make_grass(void);
struct ground *make_ground(void);
struct soldier *make_soldier(void);
struct alien *make_alien(void);
struct cover *make_cover(enum tiles_e cover_type);
struct cover *make_random_cover(void);
struct entity *release(struct entity *e);
struct entity *hold(struct entity *e);
enum directions_e opposite(enum directions_e d);
struct playfield_chunk *generate_pc(void);
void free_pc(struct playfield_chunk *c);
char is_full(char x, char y, struct playfield_chunk *c);
void place(struct playfield_chunk *c, struct entity *(*make_f)(void));

enum directions_e opposite(enum directions_e d) {
	switch(d) {
		case NORTH:
			return SOUTH;
		case SOUTH:
			return NORTH;
		case EAST:
			return WEST;
		case WEST:
			return EAST;
	}
	return MAX_DIR;
}


static void _draw_cell(char x, char y, const struct game *g);

static void _draw_cell(char x, char y, const struct game *g) {
	cputcxy(x+1, y+1, tiles_glyphs[g->pc->field[x][y]->type]);
}

void draw_board(const struct game *g) {
	char i, j;

	// Draw the playing field
	for(i = 0; i < CHUNK_SIZE_ROWS; ++i)
		for(j = 0; j < CHUNK_SIZE_COLS; ++j)
			_draw_cell(j, i, g);
}

void draw(struct game *g) {
	// Clear the status message
	gotoxy(0, SCREEN_Y+1);
	cclear(MAX_X);

	// Clear the cursor if it was there, and the current one is different
	if(g->old_crs.enabled && (g->crs.x != g->old_crs.x || g->crs.y != g->old_crs.y
				|| g->old_crs.enabled != g->crs.enabled)) {
		cputcxy(g->old_crs.x, g->old_crs.y+1, ' ');
		cputcxy(g->old_crs.x+2, g->old_crs.y+1, ' ');
		if(g->old_crs.x > 0)
			_draw_cell(g->old_crs.x-1, g->old_crs.y, g);
		if(g->old_crs.x < CHUNK_SIZE_COLS-1)
			_draw_cell(g->old_crs.x+1, g->old_crs.y, g);
	}

	// Draw the new cursor if needed
	if(g->crs.enabled) {
		cputcxy(g->crs.x, g->crs.y+1, '[');
		cputcxy(g->crs.x+2, g->crs.y+1, ']');
		gotoxy(SCREEN_X / 2 + 1, SCREEN_Y + 1);
		cprintf("(%d,%d)", g->crs.x, g->crs.y);
	}

	// Save the current cursor for the next redraw
	g->old_crs = g->crs;

	// Draw the status message
	if(g->status) {
		cputsxy(0, SCREEN_Y+1, g->status);
	}
}

struct entity *hold(struct entity *e) {
	++e->ref_count;
	return e;
}

struct entity *release(struct entity *e) {
	if(!--e->ref_count) {
		free(e);
		return NULL;
	}
	return e;
}

struct entity *make_basic(enum tiles_e type, size_t size) {
	struct entity *e;
	assert(size >= sizeof(*e));
        e = malloc(size);
	assert(e);
	e->type = type;
	e->ref_count = 0;
	return e;
}

struct grass *make_grass(void) {
	struct grass *g = (struct grass *)make_basic(GRASS, sizeof(*g));
	g->fire = 0;
	return g;
}

struct ground *make_ground(void) {
	struct ground *g = (struct ground *)make_basic(GROUND, sizeof(*g));
	return g;
}

struct soldier *make_soldier(void) {
	struct soldier *s = (struct soldier *)make_basic(SOLDIER, sizeof(*s));
	s->hitpoints = MAX_SOLDIER_HITPOINTS;
	s->mobility = MAX_SOLDIER_MOBILITY;
	s->skill = rand() % MAX_SOLDIER_SKILL;
	return s;
}

struct alien *make_alien(void) {
	struct alien *a = (struct alien *)make_basic(ALIEN, sizeof(*a));
	a->hitpoints = MAX_ALIEN_HITPOINTS;
	a->mobility = MAX_ALIEN_MOBILITY;
	a->skill = rand() % MAX_ALIEN_SKILL;
	return a;
}

struct cover *make_cover(enum tiles_e cover_type) {
	struct cover *c;
	assert(cover_type >= MIN_COVER_TYPE && cover_type < MAX_COVER_TYPE);
        c = (struct cover *)make_basic(cover_type, sizeof(*c));
	c->hitpoints = MAX_COVER_HITPOINTS;
	return c;
}

struct cover *make_random_cover(void) {
	enum tiles_e cover_type;
	cover_type = rand() % (MAX_COVER_TYPE - MIN_COVER_TYPE) + MIN_COVER_TYPE;
	return make_cover(cover_type);
}

char is_full(char r, char c, struct playfield_chunk *pc) {
	switch(pc->field[c][r]->type) {
		case GRASS:
		case GROUND:
			return 0;
		default:
			return 1;
	}
}

void place(struct playfield_chunk *c, struct entity *(*make_f)(void)) {
	char j, k;
	struct entity *e;

	j = rand() % CHUNK_SIZE_ROWS;
	k = rand() % CHUNK_SIZE_COLS;
	while(is_full(j, k, c)) {
		if(++k == CHUNK_SIZE_COLS) {
			++j;
			k = 0;
		}
		if(j == CHUNK_SIZE_ROWS)
			j = 0;
	}
	e = make_f();
	hold(e);
	c->field[k][j] = e;
}

#define MAX_SOLDIERS 	2
#define MAX_ALIENS	5
#define MAX_COVER	((CHUNK_SIZE_ROWS * CHUNK_SIZE_COLS - MAX_SOLDIERS - MAX_ALIENS) / 10)

struct playfield_chunk *generate_pc(void) {
	char i, j;
	struct grass *grass = NULL;
	struct ground *ground = NULL;
	struct playfield_chunk *c;
       
	c = malloc(sizeof(*c));

	for(i = 0; i < MAX_DIR; ++i) {
		c->neighs[i] = NULL;
	}
	
	for(i = 0; i < CHUNK_SIZE_ROWS; ++i) {
		for(j = 0; j < CHUNK_SIZE_COLS; ++j) {
			char kind = rand() % 2;
			switch(kind) {
				case 1:
					if(!grass) {
						grass = make_grass();
					}
					hold((struct entity *)grass);
					c->field[j][i] = (struct entity *)grass;
					break;
				default:
					if(!ground) {
						ground = make_ground();
					}
					hold((struct entity *)ground);
					c->field[j][i] = (struct entity *)ground;
			}
		}
	}

	assert(CHUNK_SIZE_ROWS * CHUNK_SIZE_COLS >= MAX_SOLDIERS + MAX_ALIENS + MAX_COVER);

	/* generate soldiers */
	i = rand() % MAX_SOLDIERS + 1;
	while(i--)
		place(c, (struct entity *(*)(void))make_soldier);

	/* generate aliens */
	i = rand() % MAX_ALIENS + 1;
	while(i--)
		place(c, (struct entity *(*)(void))make_alien);

	/* generate cover */
	i = rand() % MAX_COVER + 1;
	while(i--)
		place(c, (struct entity *(*)(void))make_random_cover);

	return c;
}

void free_pc(struct playfield_chunk *c) {
	char i, j;
	for(i = 0; i < CHUNK_SIZE_ROWS; ++i) {
		for(j = 0; j < CHUNK_SIZE_COLS; ++j) {
			release(c->field[j][i]);
		}
	}
	for(i = 0; i < MAX_DIR; ++i) {
		struct playfield_chunk *o = c->neighs[i];
		if(o) {
			o->neighs[opposite(i)] = NULL;
		}
	}
	free(c);
}


void init_game(struct game *g);
void destroy_game(struct game *g);


void init_game(struct game *g) {
	g->pc = generate_pc();
	g->crs.x = g->crs.y = g->crs.enabled = 0;
	g->old_crs.x = g->old_crs.y = g->old_crs.enabled = 0;
	g->turn = 0;
	g->status = "ready";
	g->state = IDLE;
}

void destroy_game(struct game *g) {
	free_pc(g->pc);
}

void select_target(struct game *g);
void crs_move(struct game *g, enum directions_e dir);
void toggle_crs(struct game *g, char change_status);

void crs_move(struct game *g, enum directions_e dir) {
	if(!g->crs.enabled) {
		g->status = "ready";
		return;
	}

	switch(dir) {
		case SOUTH:
			if(g->crs.y < CHUNK_SIZE_ROWS-1) 
				++g->crs.y;
			return;
		case NORTH:
			if(g->crs.y) 
				--g->crs.y;
			return;
		case EAST:
			if(g->crs.x < CHUNK_SIZE_COLS-1)
				++g->crs.x;
			return;
		case WEST:
			if(g->crs.x)
				--g->crs.x;
			return;
	}

	return;
}

void toggle_crs(struct game *g, char change_status) {
	g->crs.enabled = !g->crs.enabled;
	if(!g->crs.enabled)
		g->state = IDLE;
	if(change_status) {
		if(g->crs.enabled) {
			g->status = "select actor";
		} else {
			g->status = "ready";
		}
	}
	return;
}

void select_target(struct game *g) {
       if(g->state == IDLE) {
		g->actor = g->pc->field[g->crs.x][g->crs.y];
		g->actor_x = g->crs.x;
		g->actor_y = g->crs.y;
		switch(g->actor->type) {
			case ALIEN:
			case SOLDIER:
				break;
			default:
				g->status = "cannot fire";
				return;
		}
		g->state = FIRING;
		g->status = "select target";
	} else if(g->state == FIRING) {
		g->status = "FIRE!";
		g->state = IDLE;
		toggle_crs(g, 0);
	}
}

void act(struct game *g, char in) {
	++g->turn;
	g->status = "ready";
	switch(in) {
		case ' ':
			toggle_crs(g, 1);
			break;
		case 'w':
			crs_move(g, NORTH);
			break;
		case 's':
			crs_move(g, SOUTH);
			break;
		case 'd':
			crs_move(g, EAST);
			break;
		case 'a':
			crs_move(g, WEST);
			break;
		case 'f':
			select_target(g);
			break;
		default:
			g->status = "unknown command";
	}
}
				

int main(void) {
	struct game g;
	char in;
	init_game(&g);

	cursor(0);
	clrscr();
	draw_board(&g);

	do {
		draw(&g);

	        while(!kbhit())
	                ;
	        while(kbhit()) {
	                in = cgetc();
		}

		act(&g, in);
	} while(1);

	destroy_game(&g);
	return EXIT_SUCCESS;
}

