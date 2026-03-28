#include "cub3d.h"

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

/*
** Rendu d'un seul objet billboard en vraie 3D.
** On utilise r3d_draw_billboard() qui projette correctement
** le quad en 3D avec zbuffer et perspective-correct.
*/
static void render_one_object(t_game *game, t_object *obj)
{
    t_obj_def   *def;
    float       h_terrain;
    float       z_bot;
    float       z_top;
    float       z_extra_bot;
    float       z_extra_top;
    float       dist;

    if (obj->def_index < 0 || obj->def_index >= game->obj_def_count)
        return ;
    def = &game->obj_defs[obj->def_index];
    if (!def->tex_base.pixels)
        return ;

    dist = sqrtf(obj->dist);
    if (dist > MAX_DIST)
        return ;

    /* Altitude de la surface : on prend le floorf pour aligner
    ** avec le sol visuel (qui est rendu à floorf(h)) */
    h_terrain = floorf(world_get_height(&game->world,
        (int)obj->x, (int)obj->y));

    /* Base du billboard : posé sur le sol */
    z_bot = h_terrain;
    z_top = h_terrain + def->height_base;

    /* Rendu base */
    r3d_draw_billboard(game,
        obj->x, obj->y,
        z_bot, z_top,
        def->width * 0.5f,
        &def->tex_base, 0.0f, 1.0f);

    /* Extra (feuillage arbre par exemple) */
    if (def->height_extra > 0.0f && def->tex_extra.pixels)
    {
        z_extra_bot = z_top;
        z_extra_top = z_extra_bot + def->height_extra;
        r3d_draw_billboard(game,
            obj->x, obj->y,
            z_extra_bot, z_extra_top,
            def->extra_width * 0.5f,
            &def->tex_extra, 0.0f, 1.0f);
    }
}

void render_objects(t_game *game)
{
    int     i;
    float   dx;
    float   dy;

    if (game->obj_count <= 0)
        return ;

    /* Calcul des distances */
    i = 0;
    while (i < game->obj_count)
    {
        dx = game->objects[i].x - game->player.x;
        dy = game->objects[i].y - game->player.y;
        game->objects[i].dist = dx * dx + dy * dy;
        i++;
    }
    /* Tri back-to-front (painter's algorithm) */
    qsort(game->objects, game->obj_count, sizeof(t_object), cmp_obj_dist);

    i = 0;
    while (i < game->obj_count)
    {
        render_one_object(game, &game->objects[i]);
        i++;
    }
}
