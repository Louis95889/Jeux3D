#ifndef CUB3D_H
# define CUB3D_H

# include <SDL2/SDL.h>
# include <SDL2/SDL_image.h>
# include <SDL2/SDL_ttf.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <math.h>

# define WIN_W      1280
# define WIN_H      720
# define TILE_SIZE  64
# define ROT_SPEED  0.03f
# define MOUSE_SENS 0.00375f
# define FONT_PATH  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

# define MC_JUMP_FORCE   0.18f
# define MC_GRAVITY      0.012f
# define MC_DRAG         0.98f
# define MC_WALK_SPEED   0.07f
# define MC_SNEAK_SPEED  0.025f
# define MC_BOB_SPEED    0.18f
# define MC_BOB_AMP      0.012f
# define Z_FLOOR         60.0f
# define PLAYER_HEIGHT   1.8f

# define FOV_DEG        70.0f
# define NEAR_PLANE     0.1f
# define FAR_PLANE      200.0f

# define MAX_DIST       15.0f
# define TEX_SIZE       64
# define HOTBAR_SIZE    9
# define SKIN_FACES     36
# define MAX_OBJECTS    4096
# define MAX_OBJ_DEFS   32

# define TILE_EMPTY  '0'
# define TILE_WALL   '1'
# define TILE_PLAYER 'P'

# define OBJ_TREE    0
# define OBJ_ROCK    1

# define CHUNK_SIZE      16
# define RENDER_DIST     2
# define MAX_CHUNKS      100
# define MAX_TREE_BLOCKS 64

/* Block types */
# define BTYPE_AIR   0
# define BTYPE_TRUNK 1
# define BTYPE_LEAF  2
# define BTYPE_STONE 3

/* Texture indices */
# define TEX_WALL_NS  0
# define TEX_WALL_EO  1
# define TEX_GRASS    2
# define TEX_DIRT     3
# define TEX_STONE    4
# define TEX_TRUNK    5
# define TEX_LEAF     6
# define TEX_COUNT    7

/* Face count for Minecraft skin */
typedef enum e_face
{
    HEAD_FRONT=0, HEAD_BACK,  HEAD_RIGHT,  HEAD_LEFT,  HEAD_TOP,  HEAD_BOT,
    BODY_FRONT,   BODY_BACK,  BODY_RIGHT,  BODY_LEFT,  BODY_TOP,  BODY_BOT,
    ARM_L_FRONT,  ARM_L_BACK, ARM_L_RIGHT, ARM_L_LEFT, ARM_L_TOP, ARM_L_BOT,
    ARM_R_FRONT,  ARM_R_BACK, ARM_R_RIGHT, ARM_R_LEFT, ARM_R_TOP, ARM_R_BOT,
    LEG_L_FRONT,  LEG_L_BACK, LEG_L_RIGHT, LEG_L_LEFT, LEG_L_TOP, LEG_L_BOT,
    LEG_R_FRONT,  LEG_R_BACK, LEG_R_RIGHT, LEG_R_LEFT, LEG_R_TOP, LEG_R_BOT
}   t_face_id;

/* ===================== 3D Math types ===================== */

typedef struct s_vec3
{
    float   x;
    float   y;
    float   z;
}   t_vec3;

typedef struct s_vec4
{
    float   x;
    float   y;
    float   z;
    float   w;
}   t_vec4;

/* Column-major 4x4 matrix */
typedef struct s_mat4
{
    float   m[4][4];
}   t_mat4;

/* A vertex ready for rasterization */
typedef struct s_vert
{
    t_vec4  clip;       /* homogeneous clip space */
    float   u;
    float   v;
    float   wx;         /* world X (for world-space UV) */
    float   wy;         /* world Y */
    float   wz;         /* world Z */
}   t_vert;

/* ===================== Game types ===================== */

typedef struct s_tex
{
    Uint32  *pixels;
    int     w;
    int     h;
}   t_tex;

typedef struct s_obj_def
{
    char    name[64];
    t_tex   tex_base;
    t_tex   tex_extra;
    float   width;
    float   height_base;
    float   height_extra;
    float   extra_width;
    int     spawn_chance;
}   t_obj_def;

typedef struct s_block
{
    int     wx;
    int     wy;
    int     z;
    int     type;
}   t_block;

typedef struct s_chunk
{
    int     cx;
    int     cy;
    char    tiles[CHUNK_SIZE][CHUNK_SIZE];
    float   height[CHUNK_SIZE][CHUNK_SIZE];
    int     loaded;
    int     tiles_ready;
    int     objs_ready;
    t_block extra_blocks[MAX_TREE_BLOCKS];
    int     nb_extra;
}   t_chunk;

typedef struct s_world
{
    t_chunk chunks[MAX_CHUNKS];
    int     chunk_count;
    int     perm[512];
    int     perm2[512];
    int     perm3[512];
    int     perm_h[512];
    int     seed;
    void    *game_ptr;
    int     last_pcx;
    int     last_pcy;
    int     ht[128];
}   t_world;

typedef struct s_object
{
    float   x;
    float   y;
    int     def_index;
    float   dist;
}   t_object;

typedef struct s_player
{
    float           x;
    float           y;
    float           dir_x;
    float           dir_y;
    float           plane_x;
    float           plane_y;
    float           pitch;      /* vertical look angle in radians */
    float           yaw;        /* horizontal angle in radians    */
    float           z_offset;
    float           z;
    float           vel_z;
    int             on_ground;
    int             sneaking;
    float           bob_angle;
    float           bob_offset;
    int             hotbar_slot;
    int             inv_open;
    SDL_Scancode    sneak_key;
}   t_player;

typedef struct s_game
{
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *framebuf;
    SDL_Texture     *skin_tex;
    SDL_Texture     *steve_tex;         /* pre-allocated Steve texture */
    int             steve_tex_w;
    int             steve_tex_h;
    Uint32          *steve_buf;         /* pixel buffer for Steve     */
    Uint32          *skin_pixels;
    int             skin_w;
    int             skin_h;
    t_tex           faces[SKIN_FACES];
    Uint32          *pixels;
    float           *zbuf;
    TTF_Font        *font;
    TTF_Font        *font_small;
    t_world         world;
    t_player        player;
    t_object        objects[MAX_OBJECTS];
    int             obj_count;
    t_obj_def       obj_defs[MAX_OBJ_DEFS];
    int             obj_def_count;
    int             running;
    int             screen_w;
    int             screen_h;
    int             keybind_done;
    t_tex           tex[TEX_COUNT];
    float           steve_angle;
    int             steve_dragging;
    int             steve_drag_x;
    int             steve_mouse_dx;
    /* 3D renderer matrices (updated each frame) */
    t_mat4          mat_view;
    t_mat4          mat_proj;
    t_mat4          mat_vp;     /* view * proj combined */
}   t_game;

/* ===================== Function prototypes ===================== */

/* math3d.c */
t_mat4  mat4_identity(void);
t_mat4  mat4_mul(t_mat4 a, t_mat4 b);
t_vec4  mat4_mul_vec4(t_mat4 m, t_vec4 v);
t_mat4  mat4_perspective(float fov_rad, float aspect, float near, float far);
t_mat4  mat4_look_at(t_vec3 eye, t_vec3 center, t_vec3 up);
t_vec3  vec3_norm(t_vec3 v);
t_vec3  vec3_cross(t_vec3 a, t_vec3 b);
float   vec3_dot(t_vec3 a, t_vec3 b);
t_vec3  vec3_sub(t_vec3 a, t_vec3 b);

/* raster3d.c */
void    r3d_begin_frame(t_game *game);
void    r3d_draw_quad(t_game *game,
            t_vec3 p0, t_vec3 p1, t_vec3 p2, t_vec3 p3,
            float u0, float v0, float u1, float v1,
            t_tex *tex, float fog, int border);
void    r3d_draw_billboard(t_game *game,
            float wx, float wy, float wz_bot, float wz_top,
            float half_w, t_tex *tex, float u0, float u1);

/* textures.c */
int     load_one_tex(t_tex *tex, const char *path);
int     load_textures(t_game *game);
void    free_textures(t_game *game);

/* obj_loader.c */
int     load_obj_defs(t_game *game);
void    free_obj_defs(t_game *game);

/* init.c */
int     init_sdl(t_game *game);
void    cleanup(t_game *game);

/* worldgen.c */
void    world_init(t_world *world, int seed);
char    world_get_tile(t_world *world, int wx, int wy);
float   world_get_height(t_world *world, int wx, int wy);
int     world_get_block(t_world *world, int wx, int wy, int z);
void    world_update(t_game *game);
int     world_get_biome(t_world *world, int wx, int wy);

/* map.c */
void    generate_objects(t_game *game);

/* render.c */
void    render_frame(t_game *game);
void    put_pixel(t_game *game, int x, int y, Uint32 color);
Uint32  make_color(Uint8 r, Uint8 g, Uint8 b);

/* objects.c */
void    render_objects(t_game *game);

/* events.c */
void    handle_events(t_game *game);
void    move_player(t_game *game);

/* keybind.c */
void    render_keybind_screen(t_game *game);
void    handle_keybind_events(t_game *game);

/* hud.c */
void    render_hud(t_game *game);

/* player3d.c */
void    draw_steve_3d(t_game *game, int ox, int oy,
            float angle, float scale);

#endif
