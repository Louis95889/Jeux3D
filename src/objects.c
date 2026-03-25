#include "cub3d.h"

static Uint32 sample_tex(t_tex *tex, float u, float v, float fog)
{
    int    sx;
    int    sy;
    Uint32 px;
    Uint8  *p;
    Uint8  r;
    Uint8  g;
    Uint8  b;

    if (!tex->pixels) return (0xFF808080);
    sx = (int)(u * (tex->w - 1));
    sy = (int)(v * (tex->h - 1));
    if (sx < 0)       sx = 0;
    if (sx >= tex->w) sx = tex->w - 1;
    if (sy < 0)       sy = 0;
    if (sy >= tex->h) sy = tex->h - 1;
    px = tex->pixels[sy * tex->w + sx];
    p  = (Uint8 *)&px;
    b  = p[0]; g = p[1]; r = p[2];
    r  = (Uint8)(r * (1.0f - fog) + 180 * fog);
    g  = (Uint8)(g * (1.0f - fog) + 210 * fog);
    b  = (Uint8)(b * (1.0f - fog) + 255 * fog);
    return (0xFF000000 | ((Uint32)r << 16) | ((Uint32)g << 8) | b);
}

static void draw_slice(t_game *game, int x,
    int y_center, int h, t_tex *tex, float u, float fog)
{
    int   y0;
    int   y1;
    int   y;
    float v;
    float step;

    y0 = y_center - h / 2;
    y1 = y_center + h / 2;
    if (y0 < 0)               y0 = 0;
    if (y1 >= game->screen_h) y1 = game->screen_h - 1;
    if (y0 > y1) return ;
    step = 1.0f / (float)(y1 - y0 + 1);
    v    = 0.0f;
    y    = y0;
    while (y <= y1)
    {
        put_pixel(game, x, y, sample_tex(tex, u, v, fog));
        v += step;
        y++;
    }
}

static int cmp_obj_dist(const void *a, const void *b)
{
    const t_object *oa;
    const t_object *ob;

    oa = (const t_object *)a;
    ob = (const t_object *)b;
    if (ob->dist > oa->dist) return (1);
    if (ob->dist < oa->dist) return (-1);
    return (0);
}

static void render_one_object(t_game *game, t_object *obj)
{
    t_obj_def   *def;
    float       dx;
    float       dy;
    float       inv_det;
    float       tx;
    float       ty;
    float       fog;
    int         screen_x;
    int         sw;
    int         sh;
    int         horizon;
    int         block_h;
    int         obj_w;
    int         x;
    int         x0;
    int         x1;
    int         base_h;
    int         base_cy;
    float       tex_u;

    if (obj->def_index < 0 || obj->def_index >= game->obj_def_count)
        return ;
    def = &game->obj_defs[obj->def_index];
    dx  = obj->x - game->player.x;
    dy  = obj->y - game->player.y;
    inv_det = 1.0f / (game->player.plane_x * game->player.dir_y
        - game->player.dir_x * game->player.plane_y);
    tx = inv_det * ( game->player.dir_y  * dx - game->player.dir_x  * dy);
    ty = inv_det * (-game->player.plane_y * dx + game->player.plane_x * dy);
    if (ty <= 0.05f || ty > MAX_DIST)
        return ;
    fog      = (ty - 3.0f) / (MAX_DIST - 3.0f);
    if (fog < 0.0f) fog = 0.0f;
    if (fog > 1.0f) fog = 1.0f;
    sw       = game->screen_w;
    sh       = game->screen_h;
    horizon  = sh / 2
        + (int)game->player.pitch
        + (int)game->player.z_offset;
    screen_x = (int)((sw / 2) * (1.0f + tx / ty));
    block_h  = (int)((float)sh / ty);
    obj_w    = (int)(block_h * def->width);
    x0       = screen_x - obj_w / 2;
    x1       = screen_x + obj_w / 2;
    base_h   = (int)(block_h * def->height_base);
    base_cy  = horizon + base_h / 2;
    x = x0;
    while (x <= x1)
    {
        if (x >= 0 && x < sw && ty < game->zbuf[x])
        {
            tex_u = (float)(x - x0) / (float)(obj_w + 1);
            draw_slice(game, x, base_cy, base_h,
                &def->tex_base, tex_u, fog);
        }
        x++;
    }
    if (def->height_extra > 0.0f && def->tex_extra.pixels)
    {
        int extra_w  = (int)(block_h * def->extra_width);
        int extra_h  = (int)(block_h * def->height_extra);
        int extra_cy = base_cy - base_h / 2 - extra_h / 2;
        x0 = screen_x - extra_w / 2;
        x1 = screen_x + extra_w / 2;
        x  = x0;
        while (x <= x1)
        {
            if (x >= 0 && x < sw && ty < game->zbuf[x])
            {
                tex_u = (float)(x - x0) / (float)(extra_w + 1);
                draw_slice(game, x, extra_cy, extra_h,
                    &def->tex_extra, tex_u, fog);
            }
            x++;
        }
    }
}

void render_objects(t_game *game)
{
    int     i;
    float   dx;
    float   dy;

    if (game->obj_count <= 0)
        return ;
    i = 0;
    while (i < game->obj_count)
    {
        dx = game->objects[i].x - game->player.x;
        dy = game->objects[i].y - game->player.y;
        game->objects[i].dist = dx * dx + dy * dy;
        i++;
    }
    /* qsort O(n log n) au lieu du bubble sort O(n²) */
    qsort(game->objects, game->obj_count, sizeof(t_object), cmp_obj_dist);
    i = 0;
    while (i < game->obj_count)
    {
        if (game->objects[i].dist < MAX_DIST * MAX_DIST)
            render_one_object(game, &game->objects[i]);
        i++;
    }
}
