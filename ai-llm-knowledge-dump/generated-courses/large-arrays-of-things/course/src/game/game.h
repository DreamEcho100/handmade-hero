/* ══════════════════════════════════════════════════════════════════════ */
/*               game.h — LOATs Game Demo                                */
/*                                                                       */
/*  11-scene demo: 9 lesson scenes + 2 labs (Particle + Data Layout).    */
/*  [1-9] lesson scenes  [P] Particle Lab  [L] Data Layout Lab           */
/*                                                                       */
/*  This file depends on things/things.h but NOT on any platform header. */
/* ══════════════════════════════════════════════════════════════════════ */
#ifndef GAME_H
#define GAME_H

#include "../things/things.h"
#include <stdint.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════ */
/*                      Constants                         */
/* ══════════════════════════════════════════════════════ */

#define SCREEN_W 800
#define SCREEN_H 600

#define PLAYER_SPEED    200.0f  /* pixels per second */
#define PLAYER_SIZE      20.0f
#define TROLL_SIZE       15.0f
#define SPAWN_INTERVAL    1.0f  /* seconds between troll spawns */
#define MAX_TROLLS        20
#define SP_MAX_ENTITIES   2000
#define MA_MAX_ENTITIES   2000
#define MA_ARENA_SIZE     (512 * 1024) /* 512 KB arena */

#define NUM_SCENES 14

/* ══════════════════════════════════════════════════════ */
/*                      Input                             */
/* ══════════════════════════════════════════════════════ */

typedef struct game_input {
    bool left;
    bool right;
    bool up;
    bool down;
    bool space;       /* used by inventory scenes (drop item) */
    bool tab_pressed; /* used by labs to cycle modes (Data Layout: layout, Spatial: partition) */
    int scene_key;    /* 0=none, 1-9=scenes, 10=Particle(P), 11=DataLayout(L), 12=Spatial(S) */
} game_input;

/* Scene names for HUD */
const char *scene_name(int scene);

/* ══════════════════════════════════════════════════════ */
/*                    Game State                           */
/* ══════════════════════════════════════════════════════ */

typedef struct game_state {
    thing_pool   pool;
    thing_ref    player_ref;
    float        spawn_timer;
    int          trolls_spawned;
    int          trolls_killed;
    int          slots_reused;
    unsigned int rng_state;
    int          current_scene; /* 1-11, default 8 */

    /* ── Scene-specific state ── */

    /* Scene 5: free list comparison */
    thing_pool   pool_no_freelist;
    int          s5_with_next;
    int          s5_without_next;

    /* Scene 6: generational IDs demo */
    thing_ref    s6_saved_ref;
    int          s6_phase;
    float        s6_timer;

    /* Scene 7: prepend vs append */
    thing_idx    s7_prepend_parent;
    thing_idx    s7_append_parent;
    int          s7_items_added;

    /* Scene 9: separate pools */
    thing_pool   s9_player_pool;
    thing_pool   s9_troll_pool;
    thing_ref    s9_player_ref;
    int          s9_troll_count;

    /* ── Scene 10: Particle Laboratory ── */
    uint8_t  particle_archetype[MAX_THINGS];
    uint8_t  particle_aggressiveness[MAX_THINGS];
    float    particle_prediction_error[MAX_THINGS];
    int      particle_value_score[MAX_THINGS];
    float    particle_phase[MAX_THINGS];
    int      lab_particle_count;
    int      lab_score;
    int      lab_archetype_changes;
    int      lab_collision_checks;
    float    lab_update_time_us;
    float    lab_render_time_us;
    float    lab_prev_player_x;
    float    lab_prev_player_y;

    /* ── Scene 11: Data Layout Toggle Laboratory ── */
    int      dl_layout;         /* 0=AoS, 1=SoA, 2=Hybrid */
    int      dl_entity_count;
    float    dl_update_time_us;
    float    dl_render_time_us;
    int      dl_layout_switches;

    /* ── Scene 12: Spatial Partition Laboratory ── */
    int      sp_mode;             /* 0=Naive, 1=Grid, 2=Hash, 3=Quadtree */
    int      sp_entity_count;
    int      sp_checks_this_frame;
    int      sp_collisions_this_frame;
    float    sp_update_time_us;
    int      sp_mode_switches;
    bool     sp_show_debug;       /* toggle debug overlay with D key */
    /* Entity data for spatial lab — simple parallel arrays */
    float    sp_x[SP_MAX_ENTITIES];
    float    sp_y[SP_MAX_ENTITIES];
    float    sp_dx[SP_MAX_ENTITIES];
    float    sp_dy[SP_MAX_ENTITIES];
    float    sp_size[SP_MAX_ENTITIES];
    uint32_t sp_color[SP_MAX_ENTITIES];
    uint8_t  sp_active[SP_MAX_ENTITIES];
    uint8_t  sp_hit[SP_MAX_ENTITIES];  /* flash on collision this frame */

    /* ── Scene 13: Memory Arena Laboratory ── */
    int      ma_mode;              /* 0=malloc, 1=arena, 2=pool, 3=scratch */
    int      ma_allocs_this_frame;
    int      ma_frees_this_frame;
    int      ma_bytes_used;
    int      ma_peak_bytes;
    float    ma_update_time_us;
    float    ma_frame_times[120];  /* rolling 2s history for variance */
    int      ma_frame_idx;
    int      ma_entity_count;
    float    ma_spawn_timer;
    /* Arena backing */
    uint8_t  ma_arena_mem[MA_ARENA_SIZE];
    int      ma_arena_offset;
    /* Pool slots */
    uint8_t  ma_pool_used[MA_MAX_ENTITIES];
    /* Entity data (all modes use same arrays) */
    float    ma_x[MA_MAX_ENTITIES];
    float    ma_y[MA_MAX_ENTITIES];
    float    ma_dx[MA_MAX_ENTITIES];
    float    ma_dy[MA_MAX_ENTITIES];
    float    ma_lifetime[MA_MAX_ENTITIES];
    uint32_t ma_color[MA_MAX_ENTITIES];
    uint8_t  ma_active[MA_MAX_ENTITIES];

    /* ── Scene 14: Infinite Growth Laboratory (Capstone) ── */
    int      ig_mode;
    int      ig_total_created;
    int      ig_active_count;
    int      ig_peak_active;
    int      ig_destroyed_count;
    float    ig_spawn_rate;        /* entities per second */
    float    ig_spawn_accum;
    float    ig_update_time_us;
    float    ig_frame_times[120];
    int      ig_frame_idx;
    int      ig_mode_switches;
    float    ig_sim_radius;
    int      ig_budget_max;
    /* Dynamic entity storage — grows via realloc in unbounded mode.
     * THIS IS THE POINT: no fixed limit. Grows until system degrades. */
    float   *ig_x;                 /* malloc'd arrays — NULL until scene14_init */
    float   *ig_y;
    float   *ig_dx;
    float   *ig_dy;
    float   *ig_lifetime;
    uint32_t *ig_color;
    uint8_t *ig_active;
    int      ig_capacity;          /* current allocation size */
    int      ig_bytes_allocated;   /* total bytes malloc'd (for HUD) */
} game_state;

/* ══════════════════════════════════════════════════════ */
/*         Particle Laboratory — Archetype Enum            */
/* ══════════════════════════════════════════════════════ */

typedef enum particle_archetype {
    ARCHETYPE_BOUNCE = 0,
    ARCHETYPE_WANDER,
    ARCHETYPE_SEEK_PLAYER,
    ARCHETYPE_PREDICTIVE_SEEK,
    ARCHETYPE_ORBIT,
    ARCHETYPE_DRIFT_NOISE,
    ARCHETYPE_COUNT
} particle_archetype;

#define LAB_MAX_PARTICLES 1000

/* ══════════════════════════════════════════════════════ */
/*         Data Layout Laboratory — Types                  */
/* ══════════════════════════════════════════════════════ */

#define DL_MAX_ENTITIES 2000
#define DL_LAYOUT_AOS    0
#define DL_LAYOUT_SOA    1
#define DL_LAYOUT_HYBRID 2
#define DL_LAYOUT_COUNT  3

typedef struct dl_particle_aos {
    float x, y;
    float dx, dy;
    float aggression;
    uint8_t archetype;
    uint32_t color;
    uint8_t active;
} dl_particle_aos;

typedef struct dl_particles_soa {
    float x[DL_MAX_ENTITIES];
    float y[DL_MAX_ENTITIES];
    float dx[DL_MAX_ENTITIES];
    float dy[DL_MAX_ENTITIES];
    float aggression[DL_MAX_ENTITIES];
    uint8_t archetype[DL_MAX_ENTITIES];
    uint32_t color[DL_MAX_ENTITIES];
    uint8_t active[DL_MAX_ENTITIES];
} dl_particles_soa;

typedef struct dl_particle_hot {
    float x, y;
    float dx, dy;
} dl_particle_hot;

typedef struct dl_particle_cold {
    float aggression;
    uint8_t archetype;
    uint32_t color;
    uint8_t active;
} dl_particle_cold;

extern dl_particle_aos  g_dl_aos[DL_MAX_ENTITIES];
extern dl_particles_soa g_dl_soa;
extern dl_particle_hot  g_dl_hot[DL_MAX_ENTITIES];
extern dl_particle_cold g_dl_cold[DL_MAX_ENTITIES];

const char *dl_layout_name(int layout);

/* ══════════════════════════════════════════════════════ */
/*         Spatial Partition Laboratory — Types             */
/* ══════════════════════════════════════════════════════ */

#define SP_MODE_NAIVE     0  /* O(N²) all-pairs — intentionally slow baseline */
#define SP_MODE_GRID      1  /* uniform grid — fixed cell size */
#define SP_MODE_HASH      2  /* spatial hash — hash position to bucket */
#define SP_MODE_QUADTREE  3  /* quadtree — adaptive subdivision */
#define SP_MODE_COUNT     4

#define SP_GRID_COLS     20  /* uniform grid: 800/20 = 40px cells */
#define SP_GRID_ROWS     15  /* uniform grid: 600/15 = 40px cells */
#define SP_GRID_CELLS    (SP_GRID_COLS * SP_GRID_ROWS)
#define SP_CELL_W        (SCREEN_W / SP_GRID_COLS)
#define SP_CELL_H        (SCREEN_H / SP_GRID_ROWS)
#define SP_MAX_PER_CELL  64  /* max entities per grid cell */

#define SP_HASH_BUCKETS  512
#define SP_MAX_PER_BUCKET 32

/* Uniform grid cell — stores indices of entities in this cell */
typedef struct sp_grid_cell {
    int indices[SP_MAX_PER_CELL];
    int count;
} sp_grid_cell;

/* Hash bucket */
typedef struct sp_hash_bucket {
    int indices[SP_MAX_PER_BUCKET];
    int count;
} sp_hash_bucket;

/* Simple quadtree node */
#define SP_QT_MAX_DEPTH  6
#define SP_QT_MAX_NODES  1024
#define SP_QT_SPLIT_THRESHOLD 8

typedef struct sp_qt_node {
    float x, y, w, h;           /* bounds */
    int indices[SP_QT_SPLIT_THRESHOLD];
    int count;
    int children[4];            /* NW, NE, SW, SE — 0 = no child */
    bool is_leaf;
} sp_qt_node;

const char *sp_mode_name(int mode);

/* ══════════════════════════════════════════════════════ */
/*         Memory Arena Laboratory — Types                 */
/* ══════════════════════════════════════════════════════ */

#define MA_MODE_MALLOC    0  /* standard malloc/free — baseline, intentionally spiky */
#define MA_MODE_ARENA     1  /* linear arena — bump pointer, bulk reset */
#define MA_MODE_POOL      2  /* free-list pool — fixed slots, reuse */
#define MA_MODE_SCRATCH   3  /* scratch arena — reset every frame */
#define MA_MODE_COUNT     4

const char *ma_mode_name(int mode);

/* ══════════════════════════════════════════════════════ */
/*    Infinite Growth Laboratory — Types (Capstone)        */
/* ══════════════════════════════════════════════════════ */

/* Stage 1: Allocation approaches */
#define IG_MODE_MALLOC      0  /* malloc/realloc unbounded — INTENTIONALLY BAD, shows degradation */
#define IG_MODE_ARENA       1  /* arena pre-alloc — bump pointer, no realloc, stable */
#define IG_MODE_POOL        2  /* pool recycling — free list, zero alloc in game loop */
/* Stage 2: Lifetime + budget */
#define IG_MODE_LIFETIME    3  /* entities expire, slots return to pool */
#define IG_MODE_BUDGET      4  /* hard max active cap, spawner adapts */
/* Stage 3: Spatial */
#define IG_MODE_STREAMING   5  /* only simulate within radius */
#define IG_MODE_LOD         6  /* near: full update, far: every 4th frame */
/* Stage 4: Work distribution */
#define IG_MODE_AMORTIZED   7  /* stagger 1/4 entities per frame */
#define IG_MODE_THREADED    8  /* split array, pthread per half */
/* Stage 5: Production */
#define IG_MODE_PRODUCTION  9  /* arena + pool + lifetime + budget + amortized combined */
#define IG_MODE_COUNT      10

const char *ig_mode_name(int mode);

/* ══════════════════════════════════════════════════════ */
/*                    Game API                             */
/* ══════════════════════════════════════════════════════ */

void game_init(game_state *state);
void game_update(game_state *state, const game_input *input, float dt);
void game_render(const game_state *state, uint32_t *pixels, int width, int height);
void game_hud(const game_state *state, char *buf, int buf_size);

#endif /* GAME_H */
