#include <conio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <limits.h>

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
	char quit;
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
char is_cover(const struct entity *t);
void place(struct playfield_chunk *c, struct entity *(*make_f)(void));

char is_cover(const struct entity *t) {
	switch(t->type) {
		case O_COVER:
		case I_COVER:
		case H_COVER:
			return 1;
		default:
			return 0;
	}
}

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

void _place(struct playfield_chunk *c, struct entity *(*make_f)(void), char x, char y);

void _place(struct playfield_chunk *c, struct entity *(*make_f)(void), char x, char y) {
	struct entity *e;

	if(is_full(y, x, c))
		return;
	e = make_f();
	hold(e);
	c->field[x][y] = e;
}

void place(struct playfield_chunk *c, struct entity *(*make_f)(void)) {
	char j, k;

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
	_place(c, make_f, k, j);
}

#define MAX_SOLDIERS 	2
#define MAX_ALIENS	5
#define MAX_COVER	((CHUNK_SIZE_ROWS * CHUNK_SIZE_COLS - MAX_SOLDIERS - MAX_ALIENS) / 10)

#define MAKER(a) ((struct entity *(*)(void))(a))

#define NEEDS_REFRESH(a) (!(a) || (a)->_e.ref_count == UCHAR_MAX)

void _draw_line(signed char src_x, signed char src_y, signed char dst_x, signed char dst_y, struct playfield_chunk *c);
void _draw_shadow_line(signed char src_x, signed char src_y, signed char dst_x, signed char dst_y, 
		signed char o_dst_x, signed char o_dst_y, struct playfield_chunk *c);

void _draw_line(signed char src_x, signed char src_y, signed char dst_x, signed char dst_y, struct playfield_chunk *c) {
	struct grass *grass = NULL;
	signed char dx, dy, sx, sy, err, e2;
	signed char x0, y0;

	dx = abs(dst_x - src_x);
	sx = src_x < dst_x ? 1 : -1;
	dy = abs(dst_y - src_y);
	sy = src_y < dst_y ? 1 : -1; 
	err = (dx > dy ? dx : -dy)/2;
	x0 = src_x;
	y0 = src_y;
 
	for(;;) {
		if(y0 >= 0 && y0 < CHUNK_SIZE_ROWS &&
				x0 >= 0 && x0 < CHUNK_SIZE_COLS &&
				!is_full(y0, x0, c)) {
			if(NEEDS_REFRESH(grass))
				grass = make_grass();
			hold((struct entity *)grass);
			c->field[x0][y0] = (struct entity *)grass;
		}

		if (x0 == dst_x && y0 == dst_y)
			break;

		e2 = err;
    		if (e2 >-dx) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dy) {
			err += dx;
			y0 += sy;
		}
	}


}

#define INSIDE(x, y) ((x) >= 0 && (x) < CHUNK_SIZE_COLS && \
		(y) >= 0 && (y) < CHUNK_SIZE_ROWS)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define BETWEEN(x, x0, x1) ((x) < MAX((x0), (x1)) && (x) > MIN((x0), (x1)))

void _draw_shadow_line(signed char src_x, signed char src_y, signed char dst_x, signed char dst_y, 
		signed char o_dst_x, signed char o_dst_y, struct playfield_chunk *c) {
	struct grass *grass = NULL;
	signed char dx, dy, sx, sy, err, e2;
	signed char x0, y0;

	dx = abs(dst_x - src_x);
	sx = src_x < dst_x ? 1 : -1;
	dy = abs(dst_y - src_y);
	sy = src_y < dst_y ? 1 : -1; 
	err = (dx > dy ? dx : -dy)/2;
	x0 = src_x;
	y0 = src_y;
 
	for(;;) {
		if(INSIDE(x0, y0)) {
			if(!BETWEEN(x0, src_x, o_dst_x) &&
				!BETWEEN(y0, src_y, o_dst_y)
				&& !is_full(y0, x0, c)) {
				if(NEEDS_REFRESH(grass))
					grass = make_grass();
				hold((struct entity *)grass);
				c->field[x0][y0] = (struct entity *)grass;
			}
		} else {
			break;
		}

		e2 = err;
    		if (e2 >-dx) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dy) {
			err += dx;
			y0 += sy;
		}
	}


}

struct playfield_chunk *generate_pc(void) {
	char i, j;
	struct ground *ground = NULL;
	struct playfield_chunk *c;
	signed char src_x, src_y, dst_x, dst_y;
       
	c = malloc(sizeof(*c));

	for(i = 0; i < MAX_DIR; ++i) {
		c->neighs[i] = NULL;
	}
	
	for(i = 0; i < CHUNK_SIZE_ROWS; ++i) {
		for(j = 0; j < CHUNK_SIZE_COLS; ++j) {
			if(NEEDS_REFRESH(ground))
				ground = make_ground();
			hold((struct entity *)ground);
			c->field[j][i] = (struct entity *)ground;
		}
	}

	/* draw a line between 2 random coordinates */
	do {
		src_x = rand() % CHUNK_SIZE_COLS;
		dst_x = rand() % CHUNK_SIZE_COLS;
		src_y = rand() % CHUNK_SIZE_ROWS;
		dst_y = rand() % CHUNK_SIZE_ROWS;
	} while(src_x == dst_x && src_y == dst_y);

	_place(c, MAKER(make_soldier), src_x, src_y);
	_place(c, MAKER(make_alien), dst_x, dst_y);

	/* FIXME: depending on relative positions, we don't want to 
	 * look at all 4 possibilities. If the 2 positions are aligned then
	 * we're looking at one only? And if they're not, at 3?
	 * Can we make the light cone 'diverge' a bit if 2 objects
	 * are aligned but pretty close?
	 */

	_draw_shadow_line(src_x, src_y, dst_x-1, dst_y, dst_x, dst_y, c);
	_draw_shadow_line(src_x, src_y, dst_x+1, dst_y, dst_x, dst_y, c);
	_draw_shadow_line(src_x, src_y, dst_x, dst_y-1, dst_x, dst_y, c);
	_draw_shadow_line(src_x, src_y, dst_x, dst_y+1, dst_x, dst_y, c);

	return c;
}

#if 0
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
#endif

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
	g->quit = 0;
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

char to_hit(struct game *g);
void fire(struct game *g);

#define MAX_DISTANCE_TO_HIT 	20
#define MIN_DAMAGE		1
#define MAX_DAMAGE		5

struct entity *inside(char x, char y, signed char off_x, signed char off_y, struct game *g) {
	if((off_x < 0 && !x) || (off_y < 0 && !y))
		return NULL;
	if(off_x > 0 && (x == CHUNK_SIZE_COLS || off_y && y == CHUNK_SIZE_ROWS))
		return NULL;
	if(!off_x && x > CHUNK_SIZE_COLS)
		return NULL;
	if(!off_y && y > CHUNK_SIZE_ROWS)
		return NULL;
	return g->pc->field[x+off_x][y+off_y];
}

char to_hit(struct game *g) {
	struct entity *t = NULL;
	struct cover *c = NULL;
	char cover = 0;
	signed char off_x = g->crs.x - g->actor_x;
	signed char off_y = g->crs.y - g->actor_y;
	char distance = abs(off_x) + abs(off_y);

	if(distance > MAX_DISTANCE_TO_HIT)
		return 1;

	if(off_x && (t = inside(g->crs.x, g->crs.y, off_x, 0, g)) && is_cover(t)) {
		c = (struct cover *)t;
		cover += 1 + c->high;
	}

	if(off_y && (t = inside(g->crs.x, g->crs.y, 0, off_y, g)) && is_cover(t)) {
		c = (struct cover *)t;
		cover += 1 + c->high;
	}

	// TODO: apply cover

	return 255 / ((distance + 1) * (cover + 1)) * MAX_DISTANCE_TO_HIT;
}
	

void fire(struct game *g) {
	char roll = rand();
	char tohit = to_hit(g);
	char hit = roll > tohit;
	char crit = hit && roll - tohit > roll;
	if(hit) {
		if(crit)
			g->status = "crit";
		else
			g->status = "hit";
	} else
		g->status = "missed";
}

void select_target(struct game *g) {
       if(g->state == IDLE) {
		g->actor = g->pc->field[g->crs.x][g->crs.y];
		g->actor_x = g->crs.x;
		g->actor_y = g->crs.y;
		switch(g->actor->type) {
			case SOLDIER:
				break;
			default:
				g->status = "cannot fire";
				return;
		}
		g->state = FIRING;
		g->status = "select target";
	} else if(g->state == FIRING) {
		struct entity *t = g->pc->field[g->crs.x][g->crs.y];
		switch(t->type) {
			case ALIEN:
				break;
			default:
				g->status = "cannot target";
				return;
		}
		fire(g);
		g->state = IDLE;
		toggle_crs(g, 0);
		g->crs.x = g->actor_x;
		g->crs.y = g->actor_y;
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
		case 'Q':
			g->quit = 1;
			break;
		default:
			g->status = "unknown command";
	}
}
				

int main(void) {
	while(1) {
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
		} while(!g.quit);

		destroy_game(&g);
	}
	return EXIT_SUCCESS;
}

