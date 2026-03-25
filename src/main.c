#include "cub3d.h"

int main(int argc, char **argv)
{
    t_game  game;
    int     seed;
    float   spawn_h;

    seed = 0;
    if (argc >= 2)
        seed = atoi(argv[1]);
    memset(&game, 0, sizeof(t_game));
    game.player.dir_x     =  0.0f;
    game.player.dir_y     =  1.0f;
    game.player.plane_x   =  0.66f;
    game.player.plane_y   =  0.0f;
    game.player.on_ground = 1;

    if (init_sdl(&game) != 0)
        return (1);

    game.world.game_ptr = &game;
    world_init(&game.world, seed);

    /*
    ** Position de spawn : centre du monde.
    ** z = hauteur du terrain au spawn + hauteur joueur.
    */
    game.player.x = (float)(CHUNK_SIZE / 2) + 0.5f;
    game.player.y = (float)(CHUNK_SIZE / 2) + 0.5f;
    spawn_h = world_get_height(&game.world,
        (int)game.player.x, (int)game.player.y);
    game.player.z = spawn_h + PLAYER_HEIGHT;

    if (load_obj_defs(&game) != 0)
    {
        cleanup(&game);
        return (1);
    }
    generate_objects(&game);

    while (game.running && !game.keybind_done)
    {
        render_keybind_screen(&game);
        handle_keybind_events(&game);
        SDL_Delay(16);
    }
    while (game.running)
    {
        handle_events(&game);
        render_frame(&game);
        SDL_Delay(16);
    }
    cleanup(&game);
    return (0);
}
