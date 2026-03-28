#include "cub3d.h"

/* ============================================================
** render.c — Rendu 3D du monde en vraie 3D
**
** Architecture :
**   1. prefill_sky()    : dégradé ciel
**   2. r3d_begin_frame() : matrices vue/projection
**   3. render_terrain() : parcoure les chunks visibles,
**                         rasterise chaque face de bloc en quad 3D :
**                         - Face TOP   : dessus du terrain (herbe/terre)
**                         - Face SIDE  : flancs des falaises (terre/roche)
**                         - Face WALL  : murs solides
**                         - Blocs 3D arbres : troncs + feuilles
**   4. render_objects() : rochers billboard
**   5. HUD
** ============================================================ */

void put_pixel(t_game *game, int x, int y, Uint32 color)
{
    if (x < 0 || x >= game->screen_w) return ;
    if (y < 0 || y >= game->screen_h) return ;
    game->pixels[y * game->screen_w + x] = color;
}

Uint32 make_color(Uint8 r, Uint8 g, Uint8 b)
{
    return (0xFF000000 | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b);
}

/* ────────────────────────────────────────────────────────────
** Ciel : dégradé vertical
** ──────────────────────────────────────────────────────────── */
static void prefill_sky(t_game *game)
{
    int     x;
    int     y;
    float   t;
    Uint8   r;
    Uint8   g;
    Uint8   b;
    int     total;

    total = game->screen_w * game->screen_h;
    /* Remplir tout en noir d'abord (zones sol non atteintes) */
    memset(game->pixels, 0, (size_t)total * sizeof(Uint32));
    /* Initialiser zbuf à +infini */
    y = 0;
    while (y < game->screen_h)
    {
        t = (float)y / (float)game->screen_h;
        r = (Uint8)(100 + t * 55);
        g = (Uint8)(160 + t * 50);
        b = (Uint8)(230 + t * 20);
        x = 0;
        while (x < game->screen_w)
        {
            game->pixels[y * game->screen_w + x] =
                0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
            x++;
        }
        y++;
    }
}

/* ────────────────────────────────────────────────────────────
** Utilitaires : calcul du brouillard + sélection texture
** ──────────────────────────────────────────────────────────── */
static float fog_at(t_game *game, float wx, float wy)
{
    float dx;
    float dy;
    float dist;
    float fog;

    dx   = wx - game->player.x;
    dy   = wy - game->player.y;
    dist = sqrtf(dx * dx + dy * dy);
    fog  = (dist - 3.0f) / (MAX_DIST - 3.0f);
    if (fog < 0.0f) fog = 0.0f;
    if (fog > 1.0f) fog = 1.0f;
    return (fog);
}

static t_tex *pick_top_tex(t_game *game, float depth)
{
    if (depth < 0.5f)  return &game->tex[TEX_GRASS];
    if (depth < 4.0f)  return &game->tex[TEX_DIRT];
    return &game->tex[TEX_STONE];
}

static t_tex *pick_side_tex_depth(t_game *game, int level, int surf)
{
    int depth;
    depth = surf - level - 1;
    if (depth <= 0)  return &game->tex[TEX_GRASS];
    if (depth < 4)   return &game->tex[TEX_DIRT];
    return &game->tex[TEX_STONE];
}

/* ────────────────────────────────────────────────────────────
** Rendu de la face TOP d'une tile (plan horizontal)
**
** La tile va de (wx, wy) à (wx+1, wy+1) en espace carte.
** En espace vue :  x←wx,  y←h,  z←wy
**
** UVs world-space : u = frac(wx), v = frac(wy)
** → la texture est alignée sur la grille monde, toujours statique.
** ──────────────────────────────────────────────────────────── */
static void render_tile_top(t_game *game, int wx, int wy, float h,
    t_tex *tex, float fog)
{
    t_vec3  p0, p1, p2, p3;
    float   fy;

    fy = floorf(h);

    /*
    ** Face horizontale (sol vu de dessus).
    ** Convention UV : u=0..1 selon X monde, v=0..1 selon Z(=Y carte) monde.
    ** p0=(wx,  wy  ) u=0,v=0
    ** p1=(wx+1,wy  ) u=1,v=0
    ** p2=(wx+1,wy+1) u=1,v=1
    ** p3=(wx,  wy+1) u=0,v=1
    ** On passe en deux triangles avec UVs explicites par sommet.
    */
    p0 = (t_vec3){ (float)wx,       fy, (float)wy       };
    p1 = (t_vec3){ (float)(wx + 1), fy, (float)wy       };
    p2 = (t_vec3){ (float)(wx + 1), fy, (float)(wy + 1) };
    p3 = (t_vec3){ (float)wx,       fy, (float)(wy + 1) };

    /* tri1 : p0(0,0), p1(1,0), p2(1,1) */
    r3d_draw_quad(game, p0, p1, p2, p3,
        0.0f, 0.0f, 1.0f, 1.0f,
        tex, fog, 1);
}

/* ────────────────────────────────────────────────────────────
** Rendu d'une face SIDE entre deux niveaux h_lo et h_hi
** sur le bord est (direction +X monde)
** ──────────────────────────────────────────────────────────── */
static void render_side_x(t_game *game,
    float x0, float wy, float h_lo, float h_hi,
    t_tex *tex, float fog)
{
    t_vec3  p0, p1, p2, p3;
    float   lo;
    float   hi;

    lo = (float)h_lo;
    hi = (float)h_hi;

    p0 = (t_vec3){ x0, lo, (float)wy       };
    p1 = (t_vec3){ x0, lo, (float)(wy + 1) };
    p2 = (t_vec3){ x0, hi, (float)(wy + 1) };
    p3 = (t_vec3){ x0, hi, (float)wy       };

    r3d_draw_quad(game, p0, p1, p2, p3,
        0.0f, 0.0f, 1.0f, 1.0f,
        tex, fog, 0);
}

static void render_side_z(t_game *game,
    float wx, float z0, float h_lo, float h_hi,
    t_tex *tex, float fog)
{
    t_vec3  p0, p1, p2, p3;
    float   lo;
    float   hi;

    lo = (float)h_lo;
    hi = (float)h_hi;

    p0 = (t_vec3){ (float)wx,       lo, z0 };
    p1 = (t_vec3){ (float)(wx + 1), lo, z0 };
    p2 = (t_vec3){ (float)(wx + 1), hi, z0 };
    p3 = (t_vec3){ (float)wx,       hi, z0 };

    r3d_draw_quad(game, p0, p1, p2, p3,
        0.0f, 0.0f, 1.0f, 1.0f,
        tex, fog, 0);
}

/* ────────────────────────────────────────────────────────────
** Rendu d'un bloc 3D (arbre : tronc ou feuilles)
** Un bloc occupe (wx, wy, bz) à (wx+1, wy+1, bz+1)
** ──────────────────────────────────────────────────────────── */
static void render_block_3d(t_game *game, int wx, int wy, int bz,
    t_tex *tex, float fog)
{
    t_vec3  p[8];
    float   x0, x1, y0, y1, z0, z1;

    x0 = (float)wx;       x1 = x0 + 1.0f;
    y0 = (float)bz;       y1 = y0 + 1.0f;   /* altitude */
    z0 = (float)wy;       z1 = z0 + 1.0f;

    /* 8 coins du cube */
    p[0] = (t_vec3){x0, y0, z0};
    p[1] = (t_vec3){x1, y0, z0};
    p[2] = (t_vec3){x1, y0, z1};
    p[3] = (t_vec3){x0, y0, z1};
    p[4] = (t_vec3){x0, y1, z0};
    p[5] = (t_vec3){x1, y1, z0};
    p[6] = (t_vec3){x1, y1, z1};
    p[7] = (t_vec3){x0, y1, z1};

    /* Toutes les faces : UVs 0..1 */
    r3d_draw_quad(game, p[4], p[5], p[6], p[7], 0.f,0.f,1.f,1.f, tex, fog, 0);
    r3d_draw_quad(game, p[0], p[1], p[5], p[4], 0.f,0.f,1.f,1.f, tex, fog, 0);
    r3d_draw_quad(game, p[2], p[3], p[7], p[6], 0.f,0.f,1.f,1.f, tex, fog, 0);
    r3d_draw_quad(game, p[3], p[0], p[4], p[7], 0.f,0.f,1.f,1.f, tex, fog, 0);
    r3d_draw_quad(game, p[1], p[2], p[6], p[5], 0.f,0.f,1.f,1.f, tex, fog, 0);
}

/* ────────────────────────────────────────────────────────────
** Rendu d'un mur solide (TILE_WALL)
** Le mur fait 4 blocs de haut.
** ──────────────────────────────────────────────────────────── */
static void render_wall(t_game *game, int wx, int wy, float h, float fog)
{
    t_vec3  p[8];
    float   x0, x1, y0, y1, z0, z1;

    x0 = (float)wx;   x1 = x0 + 1.0f;
    y0 = (float)h;    y1 = y0 + 4.0f;
    z0 = (float)wy;   z1 = z0 + 1.0f;

    p[0] = (t_vec3){x0, y0, z0};
    p[1] = (t_vec3){x1, y0, z0};
    p[2] = (t_vec3){x1, y0, z1};
    p[3] = (t_vec3){x0, y0, z1};
    p[4] = (t_vec3){x0, y1, z0};
    p[5] = (t_vec3){x1, y1, z0};
    p[6] = (t_vec3){x1, y1, z1};
    p[7] = (t_vec3){x0, y1, z1};

    r3d_draw_quad(game, p[4], p[5], p[6], p[7], 0.f,0.f,1.f,1.f, &game->tex[TEX_WALL_NS], fog, 0);
    r3d_draw_quad(game, p[0], p[1], p[5], p[4], 0.f,0.f,1.f,1.f, &game->tex[TEX_WALL_NS], fog, 0);
    r3d_draw_quad(game, p[2], p[3], p[7], p[6], 0.f,0.f,1.f,1.f, &game->tex[TEX_WALL_NS], fog, 0);
    r3d_draw_quad(game, p[3], p[0], p[4], p[7], 0.f,0.f,1.f,1.f, &game->tex[TEX_WALL_EO], fog, 0);
    r3d_draw_quad(game, p[1], p[2], p[6], p[5], 0.f,0.f,1.f,1.f, &game->tex[TEX_WALL_EO], fog, 0);
}

/* ────────────────────────────────────────────────────────────
** Rendu du terrain complet
** On itère sur les chunks chargés, tile par tile.
** Pour chaque tile :
**   - Face TOP
**   - Faces SIDE avec les voisins (falaises)
**   - Blocs 3D (arbres)
**   - Murs
** ──────────────────────────────────────────────────────────── */
static int in_range(t_game *game, int wx, int wy)
{
    float dx;
    float dy;
    dx = (float)wx + 0.5f - game->player.x;
    dy = (float)wy + 0.5f - game->player.y;
    return (dx * dx + dy * dy < MAX_DIST * MAX_DIST);
}

static void render_terrain(t_game *game)
{
    int         ci;
    int         lx;
    int         ly;
    int         wx;
    int         wy;
    float       h;
    float       h_n;  /* voisin nord */
    float       h_s;
    float       h_e;
    float       h_w;
    float       fog;
    t_chunk     *chunk;
    int         bz;
    int         btype;
    t_tex       *btex;
    float       depth;

    ci = 0;
    while (ci < MAX_CHUNKS)
    {
        if (!game->world.chunks[ci].loaded
            || !game->world.chunks[ci].tiles_ready)
        {
            ci++;
            continue ;
        }
        chunk = &game->world.chunks[ci];
        ly = 0;
        while (ly < CHUNK_SIZE)
        {
            lx = 0;
            while (lx < CHUNK_SIZE)
            {
                wx = chunk->cx * CHUNK_SIZE + lx;
                wy = chunk->cy * CHUNK_SIZE + ly;

                /* Frustum simple : distance max */
                if (!in_range(game, wx, wy))
                {
                    lx++;
                    continue ;
                }

                h   = chunk->height[ly][lx];
                fog = fog_at(game, (float)wx + 0.5f, (float)wy + 0.5f);

                /* ── MURS ── */
                if (chunk->tiles[ly][lx] == TILE_WALL)
                {
                    render_wall(game, wx, wy, h, fog);
                    lx++;
                    continue ;
                }

                /* ── TOP ── */
                depth = game->player.z - h;
                render_tile_top(game, wx, wy, floorf(h),
                    pick_top_tex(game, depth), fog);

                /* ── SIDES (falaises) ── */
                /* Est : voisin (wx+1, wy) */
                h_e = world_get_height(&game->world, wx + 1, wy);
                if ((int)h_e > (int)h)
                {
                    int lo = (int)h;
                    int hi = (int)h_e;
                    int lv;
                    lv = lo;
                    while (lv < hi)
                    {
                        render_side_x(game, (float)(wx + 1),
                            wy, lv, lv + 1,
                            pick_side_tex_depth(game, lv, hi), fog);
                        lv++;
                    }
                }
                /* Ouest : voisin (wx-1, wy) */
                h_w = world_get_height(&game->world, wx - 1, wy);
                if ((int)h_w > (int)h)
                {
                    int lo = (int)h;
                    int hi = (int)h_w;
                    int lv = lo;
                    while (lv < hi)
                    {
                        render_side_x(game, (float)wx,
                            wy, lv, lv + 1,
                            pick_side_tex_depth(game, lv, hi), fog);
                        lv++;
                    }
                }
                /* Sud : voisin (wx, wy+1) */
                h_s = world_get_height(&game->world, wx, wy + 1);
                if ((int)h_s > (int)h)
                {
                    int lo = (int)h;
                    int hi = (int)h_s;
                    int lv = lo;
                    while (lv < hi)
                    {
                        render_side_z(game, wx, (float)(wy + 1),
                            lv, lv + 1,
                            pick_side_tex_depth(game, lv, hi), fog);
                        lv++;
                    }
                }
                /* Nord : voisin (wx, wy-1) */
                h_n = world_get_height(&game->world, wx, wy - 1);
                if ((int)h_n > (int)h)
                {
                    int lo = (int)h;
                    int hi = (int)h_n;
                    int lv = lo;
                    while (lv < hi)
                    {
                        render_side_z(game, wx, (float)wy,
                            lv, lv + 1,
                            pick_side_tex_depth(game, lv, hi), fog);
                        lv++;
                    }
                }

                /* ── BLOCS 3D (arbres) ── */
                bz = (int)floorf(h) + 1;
                while (bz <= (int)floorf(h) + 8)
                {
                    btype = world_get_block(&game->world, wx, wy, bz);
                    if (btype != BTYPE_AIR)
                    {
                        btex = (btype == BTYPE_TRUNK)
                            ? &game->tex[TEX_TRUNK]
                            : &game->tex[TEX_LEAF];
                        render_block_3d(game, wx, wy, bz, btex, fog);
                    }
                    bz++;
                }

                lx++;
            }
            ly++;
        }
        ci++;
    }
}

/* ────────────────────────────────────────────────────────────
** Frame principale
** ──────────────────────────────────────────────────────────── */
void render_frame(t_game *game)
{
    int i;

    /* Réinitialiser zbuf à FAR_PLANE (distance max = opaque) */
    i = 0;
    while (i < game->screen_w * game->screen_h)
    {
        game->zbuf[i] = FAR_PLANE;
        i++;
    }

    /* 1. Ciel */
    prefill_sky(game);

    /* 2. Matrices 3D */
    r3d_begin_frame(game);

    /* 3. Terrain + arbres */
    render_terrain(game);

    /* 4. Objets billboard */
    render_objects(game);

    /* 5. Présentation */
    SDL_UpdateTexture(game->framebuf, NULL, game->pixels,
        game->screen_w * sizeof(Uint32));
    SDL_RenderCopy(game->renderer, game->framebuf, NULL, NULL);
    render_hud(game);
    SDL_RenderPresent(game->renderer);
}
