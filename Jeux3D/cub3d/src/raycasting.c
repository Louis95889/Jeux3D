#include "cub3d.h"

static Uint32 apply_fog(float dist, Uint32 color)
{
    float fog;
    Uint8 r;
    Uint8 g;
    Uint8 b;

    fog = (dist - 3.0f) / (MAX_DIST - 3.0f);
    if (fog < 0.0f) fog = 0.0f;
    if (fog > 1.0f) fog = 1.0f;
    r = (color >> 16) & 0xFF;
    g = (color >> 8)  & 0xFF;
    b = (color)       & 0xFF;
    r = (Uint8)(r * (1.0f - fog) + 180 * fog);
    g = (Uint8)(g * (1.0f - fog) + 210 * fog);
    b = (Uint8)(b * (1.0f - fog) + 255 * fog);
    return (make_color(r, g, b));
}

static void init_ray(t_game *game, t_ray *ray, int x)
{
    t_player *p;

    p = &game->player;
    ray->camera_x = 2.0f * x / (float)game->screen_w - 1.0f;
    ray->ray_dx   = p->dir_x + p->plane_x * ray->camera_x;
    ray->ray_dy   = p->dir_y + p->plane_y * ray->camera_x;
    ray->map_x    = (int)floorf(p->x);
    ray->map_y    = (int)floorf(p->y);
    ray->delta_x  = (ray->ray_dx == 0) ? 1e30f : fabsf(1.0f / ray->ray_dx);
    ray->delta_y  = (ray->ray_dy == 0) ? 1e30f : fabsf(1.0f / ray->ray_dy);
}

static void init_step(t_game *game, t_ray *ray)
{
    t_player *p;

    p = &game->player;
    if (ray->ray_dx < 0)
    {
        ray->step_x      = -1;
        ray->side_dist_x = (p->x - ray->map_x) * ray->delta_x;
    }
    else
    {
        ray->step_x      =  1;
        ray->side_dist_x = (ray->map_x + 1.0f - p->x) * ray->delta_x;
    }
    if (ray->ray_dy < 0)
    {
        ray->step_y      = -1;
        ray->side_dist_y = (p->y - ray->map_y) * ray->delta_y;
    }
    else
    {
        ray->step_y      =  1;
        ray->side_dist_y = (ray->map_y + 1.0f - p->y) * ray->delta_y;
    }
}

/*
** DDA avec limite stricte de distance.
** On arrete des que wall_dist depasse MAX_DIST.
** Evite de charger des chunks a l'infini en plaine ouverte.
*/
static int dda(t_game *game, t_ray *ray)
{
    float   dist;
    char    tile;

    while (1)
    {
        if (ray->side_dist_x < ray->side_dist_y)
        {
            dist = ray->side_dist_x - ray->delta_x;
            if (dist >= MAX_DIST)
                return (0);
            ray->side_dist_x += ray->delta_x;
            ray->map_x       += ray->step_x;
            ray->side         = 0;
        }
        else
        {
            dist = ray->side_dist_y - ray->delta_y;
            if (dist >= MAX_DIST)
                return (0);
            ray->side_dist_y += ray->delta_y;
            ray->map_y       += ray->step_y;
            ray->side         = 1;
        }
        tile = world_get_tile(&game->world, ray->map_x, ray->map_y);
        if (tile == TILE_WALL)
            return (1);
    }
}

static void calc_column(t_game *game, t_ray *ray)
{
    int horizon;

    if (ray->side == 0)
        ray->wall_dist = ray->side_dist_x - ray->delta_x;
    else
        ray->wall_dist = ray->side_dist_y - ray->delta_y;
    if (ray->side == 0)
        ray->wall_x = game->player.y + ray->wall_dist * ray->ray_dy;
    else
        ray->wall_x = game->player.x + ray->wall_dist * ray->ray_dx;
    ray->wall_x -= floorf(ray->wall_x);
    ray->line_h  = (int)(game->screen_h / ray->wall_dist);
    horizon      = game->screen_h / 2
        + (int)game->player.pitch
        + (int)game->player.z_offset
        + (int)game->player.bob_offset;
    ray->draw_start = horizon - ray->line_h / 2;
    ray->draw_end   = horizon + ray->line_h / 2;
    if (ray->draw_start < 0)             ray->draw_start = 0;
    if (ray->draw_end >= game->screen_h) ray->draw_end = game->screen_h - 1;
}

static void draw_column_textured(t_game *game, t_ray *ray, int x)
{
    t_tex   *tex;
    int     tex_x;
    int     tex_y;
    int     horizon;
    int     y;
    float   step;
    float   tex_pos;
    Uint32  color;

    tex   = (ray->side == 0) ? &game->tex[0] : &game->tex[1];
    tex_x = (int)(ray->wall_x * (float)tex->w);
    if (tex_x < 0)       tex_x = 0;
    if (tex_x >= tex->w) tex_x = tex->w - 1;
    step    = (float)tex->h / (float)ray->line_h;
    horizon = game->screen_h / 2
        + (int)game->player.pitch
        + (int)game->player.z_offset
        + (int)game->player.bob_offset;
    tex_pos = (ray->draw_start - horizon + ray->line_h / 2) * step;
    y = ray->draw_start;
    while (y <= ray->draw_end)
    {
        tex_y = (int)tex_pos % tex->h;
        if (tex_y < 0) tex_y = 0;
        tex_pos += step;
        color = tex->pixels[tex_y * tex->w + tex_x];
        color = apply_fog(ray->wall_dist, color);
        put_pixel(game, x, y, color);
        y++;
    }
}

void cast_rays(t_game *game)
{
    t_ray   ray;
    int     x;

    x = 0;
    while (x < game->screen_w)
    {
        init_ray(game, &ray, x);
        init_step(game, &ray);
        if (dda(game, &ray) && ray.wall_dist < MAX_DIST)
        {
            calc_column(game, &ray);
            draw_column_textured(game, &ray, x);
            game->zbuf[x] = ray.wall_dist;
        }
        else
            game->zbuf[x] = MAX_DIST;
        x++;
    }
}
