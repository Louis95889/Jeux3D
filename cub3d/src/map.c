#include "cub3d.h"

/*
** generate_objects est appele au demarrage depuis main.c.
** Avec le monde infini, les objets sont geres par world_update().
** Cette fonction initialise simplement le compteur.
*/
void generate_objects(t_game *game)
{
    game->obj_count = 0;
    world_update(game);
    fprintf(stderr, "World initialized, seed=%d, objects=%d\n",
        game->world.seed, game->obj_count);
}
