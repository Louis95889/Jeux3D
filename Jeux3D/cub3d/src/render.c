#include "cub3d.h"

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

static int get_horizon(t_game *game)
{
    return (game->screen_h / 2
        + (int)game->player.pitch
        + (int)game->player.z_offset
        + (int)game->player.bob_offset);
}

static float get_scale(t_game *game)
{
    return ((float)game->screen_w / (2.0f * game->player.plane_x));
}

static int project_y(float h, float pz, float dist, int horizon, float scale)
{
    if (dist < 0.001f) return horizon;
    return (horizon - (int)((h - pz) * scale / dist));
}

static Uint32 apply_fog(Uint32 color, float fog)
{
    Uint8 r;
    Uint8 g;
    Uint8 b;

    if (fog <= 0.0f) return color;
    r = (color >> 16) & 0xFF;
    g = (color >> 8)  & 0xFF;
    b =  color        & 0xFF;
    r = (Uint8)(r * (1.0f - fog) + 180 * fog);
    g = (Uint8)(g * (1.0f - fog) + 210 * fog);
    b = (Uint8)(b * (1.0f - fog) + 255 * fog);
    return (make_color(r, g, b));
}

static Uint32 sample_tex(t_tex *tex, float u, float v)
{
    int sx;
    int sy;

    if (!tex->pixels) return (0xFF808080);
    sx = (int)(u * (float)tex->w) % tex->w;
    sy = (int)(v * (float)tex->h) % tex->h;
    if (sx < 0) sx += tex->w;
    if (sy < 0) sy += tex->h;
    if (sx >= tex->w) sx = tex->w - 1;
    if (sy >= tex->h) sy = tex->h - 1;
    /* Forcer alpha a 0xFF : nos textures sol sont opaques */
    return (tex->pixels[sy * tex->w + sx] | 0xFF000000u);
}

/*
** Dessine une tranche verticale texturée [y0..y1].
** Respecte l'alpha : pixels transparents (alpha < 16) sont ignorés.
*/
static void draw_vspan(t_game *game, int x, int y0, int y1,
    float u, t_tex *tex, float fog)
{
    int    y;
    float  v;
    Uint32 color;

    if (y0 < 0)               y0 = 0;
    if (y1 >= game->screen_h) y1 = game->screen_h - 1;
    if (y0 > y1)              return ;
    y = y0;
    while (y <= y1)
    {
        v     = (float)(y - y0) / (float)(y1 - y0 + 1);
        color = sample_tex(tex, u, v);
        if (((color >> 24) & 0xFF) >= 16)
            put_pixel(game, x, y, apply_fog(color, fog));
        y++;
    }
}


static t_tex *pick_side_tex(t_game *game, int face_lv, int surf_h)
{
    int depth;

    depth = surf_h - face_lv - 1;
    if (depth <= 0) return (&game->tex[TEX_GRASS]);
    if (depth < 4)  return (&game->tex[TEX_DIRT]);
    return (&game->tex[TEX_STONE]);
}

/*
** Passe 1 : remplir uniquement le CIEL.
** Le bas de l'écran (y >= horizon) reste noir →
** les colonnes terrain le rempliront avec la bonne couleur.
*/
static void prefill_sky(t_game *game)
{
    int     x;
    int     y;
    int     horizon;
    float   t;
    Uint8   r;
    Uint8   g;
    Uint8   b;

    horizon = get_horizon(game);
    /* Remplir tout en noir d'abord */
    memset(game->pixels, 0,
        (size_t)(game->screen_w * game->screen_h) * sizeof(Uint32));
    /* Puis peindre le ciel */
    y = 0;
    while (y < horizon && y < game->screen_h)
    {
        t = (float)y / (float)(game->screen_h > 0 ? game->screen_h : 1);
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

/*
** Rendu d'une colonne complète :
**
** 1. DDA tile par tile le long du rayon
** 2. Blocs 3D (arbres) : tronc + feuilles niveau par niveau
** 3. Faces SIDE entre niveaux différents (falaises, montées)
** 4. A la fin, remplir [y_floor..screen_h] avec la couleur du sol
**    → herbe, terre ou roche selon la profondeur
**
** PAS de draw_vspan pour la face TOP : le dessus des blocs est rendu
** par le floor casting implicite (couleur sol à y_floor).
*/
static void render_col(t_game *game, int x)
{
    float   cam_x;
    float   rdx;
    float   rdy;
    float   ddx;
    float   ddy;
    float   sdx;
    float   sdy;
    int     mx;
    int     my;
    int     sx_s;
    int     sy_s;
    int     side;
    float   dist;
    float   wall_x;
    float   scale;
    float   fog;
    int     horizon;
    int     y_floor;
    int     curr_h;
    int     prev_h;
    int     h;
    int     yt;
    int     yb;
    int     steps;
    int     btype;
    int     bz;
    int     base_h;
    float   last_fog;
    int     last_h;

    horizon = get_horizon(game);
    scale   = get_scale(game);
    cam_x   = 2.0f * (float)x / (float)game->screen_w - 1.0f;
    rdx     = game->player.dir_x + game->player.plane_x * cam_x;
    rdy     = game->player.dir_y + game->player.plane_y * cam_x;
    mx      = (int)floorf(game->player.x);
    my      = (int)floorf(game->player.y);
    ddx     = (rdx == 0.0f) ? 1e30f : fabsf(1.0f / rdx);
    ddy     = (rdy == 0.0f) ? 1e30f : fabsf(1.0f / rdy);
    if (rdx < 0) { sx_s = -1; sdx = (game->player.x - mx) * ddx; }
    else         { sx_s =  1; sdx = (mx + 1.0f - game->player.x) * ddx; }
    if (rdy < 0) { sy_s = -1; sdy = (game->player.y - my) * ddy; }
    else         { sy_s =  1; sdy = (my + 1.0f - game->player.y) * ddy; }

    y_floor  = game->screen_h;
    prev_h   = (int)floorf(world_get_height(&game->world,
                    (int)floorf(game->player.x),
                    (int)floorf(game->player.y)));
    last_fog = 1.0f;
    last_h   = prev_h;
    steps    = 0;

    while (steps < (int)(MAX_DIST * 2 + 4))
    {
        if (sdx < sdy) { sdx += ddx; mx += sx_s; side = 0; }
        else           { sdy += ddy; my += sy_s; side = 1; }
        dist = (side == 0) ? sdx - ddx : sdy - ddy;
        if (dist > MAX_DIST) break ;
        if (dist < 0.05f) { steps++; continue ; }

        if (side == 0) wall_x = game->player.y + dist * rdy;
        else           wall_x = game->player.x + dist * rdx;
        wall_x -= floorf(wall_x);

        fog = (dist - 3.0f) / (MAX_DIST - 3.0f);
        if (fog < 0.0f) fog = 0.0f;
        if (fog > 1.0f) fog = 1.0f;
        last_fog = fog;

        /* Blocs 3D (arbres) à cette position */
        if (world_get_tile(&game->world, mx, my) != TILE_WALL)
        {
            base_h = (int)floorf(world_get_height(&game->world, mx, my));
            bz = base_h + 1;
            while (bz <= base_h + 8)
            {
                btype = world_get_block(&game->world, mx, my, bz);
                if (btype != BTYPE_AIR)
                {
                    t_tex *btex = (btype == BTYPE_TRUNK)
                        ? &game->tex[TEX_TRUNK]
                        : &game->tex[TEX_LEAF];
                    yt = project_y((float)(bz + 1), game->player.z,
                        dist, horizon, scale);
                    yb = project_y((float)bz, game->player.z,
                        dist, horizon, scale);
                    if (yb > y_floor) yb = y_floor;
                    if (yt < yb)
                    {
                        draw_vspan(game, x, yt, yb, wall_x, btex, fog);
                        /* Le tronc opaque bloque le sol derrière */
                        if (btype == BTYPE_TRUNK && yt < y_floor)
                            y_floor = yt;
                    }
                }
                bz++;
            }
        }

        curr_h = (int)floorf(world_get_height(&game->world, mx, my));
        last_h = curr_h;

        /* Mur solide : 4 blocs de hauteur */
        if (world_get_tile(&game->world, mx, my) == TILE_WALL)
        {
            int wtop = curr_h + 4;
            yt = project_y((float)wtop,   game->player.z, dist, horizon, scale);
            yb = project_y((float)curr_h, game->player.z, dist, horizon, scale);
            if (yb > y_floor) yb = y_floor;
            if (yt < yb)
            {
                t_tex *tex = (side == 0) ? &game->tex[TEX_WALL_NS]
                                         : &game->tex[TEX_WALL_EO];
                draw_vspan(game, x, yt, yb, wall_x, tex, fog);
                y_floor = yt;
            }
            game->zbuf[x] = dist;
            last_fog = fog;
            break ;
        }

        /* Faces SIDE entre niveaux */
        if (curr_h != prev_h)
        {
            int lo = (curr_h < prev_h) ? curr_h : prev_h;
            int hi = (curr_h > prev_h) ? curr_h : prev_h;
            h = lo;
            while (h < hi)
            {
                yt = project_y((float)(h + 1), game->player.z,
                    dist, horizon, scale);
                yb = project_y((float)h, game->player.z,
                    dist, horizon, scale);
                if (yb > y_floor) yb = y_floor;
                if (yt < yb)
                {
                    draw_vspan(game, x, yt, yb, wall_x,
                        pick_side_tex(game, h, hi), fog);
                    if (yt < y_floor) y_floor = yt;
                }
                h++;
            }
            if (curr_h > prev_h)
            {
                game->zbuf[x] = dist;
                break ;
            }
        }

        /* Mettre à jour y_floor avec le sommet du terrain courant */
        yt = project_y((float)(curr_h + 1), game->player.z,
            dist, horizon, scale);
        if (yt < y_floor) y_floor = yt;

        game->zbuf[x] = dist;
        prev_h = curr_h;
        steps++;
    }

    /*
    ** Remplir la zone sol [y_floor..screen_h] avec la texture herbe/terre/roche.
    ** On utilise la couleur de la dernière tile visible.
    ** depth = player.z - last_h détermine la couche.
    */
    if (y_floor < game->screen_h)
    {
        float   depth;
        Uint32  sol_color;
        t_tex   *sol_tex;
        int     y;
        float   v;
        float   u;

        depth = game->player.z - (float)last_h;
        if (depth < 0.5f)
            sol_tex = &game->tex[TEX_GRASS];
        else if (depth < 4.0f)
            sol_tex = &game->tex[TEX_DIRT];
        else
            sol_tex = &game->tex[TEX_STONE];

        y = y_floor;
        while (y < game->screen_h)
        {
            /* u = position horizontale sur la texture (wall_x)
            ** v = position verticale repetee (coordonnee monde mod 1) */
            u = wall_x;
            v = (float)(y - y_floor) / (float)(game->screen_h - y_floor + 1);
            sol_color = sample_tex(sol_tex, u, v);
            sol_color = apply_fog(sol_color, last_fog);
            put_pixel(game, x, y, sol_color);
            y++;
        }
    }
}

void render_frame(t_game *game)
{
    int x;
    int i;

    i = 0;
    while (i < game->screen_w)
    {
        game->zbuf[i] = MAX_DIST;
        i++;
    }

    /* Passe 1 : ciel seulement, bas = noir */
    prefill_sky(game);

    /* Passe 2 : terrain + arbres + sol par colonne */
    x = 0;
    while (x < game->screen_w)
    {
        render_col(game, x);
        x++;
    }

    /* Passe 3 : objets billboard */
    render_objects(game);

    SDL_UpdateTexture(game->framebuf, NULL, game->pixels,
        game->screen_w * sizeof(Uint32));
    SDL_RenderCopy(game->renderer, game->framebuf, NULL, NULL);
    render_hud(game);
    SDL_RenderPresent(game->renderer);
}
