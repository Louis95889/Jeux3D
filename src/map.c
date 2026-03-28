#include "cub3d.h"

void generate_objects(t_game *game)
{
    game->obj_count = 0;
    world_update(game);
    fprintf(stderr, "World initialized, seed=%d, objects=%d\n",
        game->world.seed, game->obj_count);
}
