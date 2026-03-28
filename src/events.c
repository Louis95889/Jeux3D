#include "cub3d.h"

static int is_wall(t_game *game, float x, float y)
{
    return (world_get_tile(&game->world,
        (int)floorf(x), (int)floorf(y)) == TILE_WALL);
}

static void rotate(t_player *p, float angle)
{
    float old_dir_x;
    float old_plane_x;

    old_dir_x   = p->dir_x;
    old_plane_x = p->plane_x;
    p->dir_x    =  p->dir_x   * cosf(angle) - p->dir_y   * sinf(angle);
    p->dir_y    =  old_dir_x  * sinf(angle) + p->dir_y   * cosf(angle);
    p->plane_x  =  p->plane_x * cosf(angle) - p->plane_y * sinf(angle);
    p->plane_y  =  old_plane_x * sinf(angle) + p->plane_y * cosf(angle);
    p->yaw     += angle;
}

static void apply_gravity(t_game *game)
{
    t_player    *p;
    float       surf;
    float       floor_z;

    p = &game->player;
    /*
    ** On utilise floorf() sur la hauteur de surface pour que
    ** le sol soit toujours à un niveau entier — évite le glissement
    ** progressif dû aux valeurs flottantes du Perlin noise.
    */
    surf    = floorf(world_get_height(&game->world,
                (int)floorf(p->x), (int)floorf(p->y)));
    floor_z = surf + PLAYER_HEIGHT;

    p->vel_z -= MC_GRAVITY;
    p->vel_z *= MC_DRAG;
    p->z     += p->vel_z;

    if (p->z <= floor_z)
    {
        p->z         = floor_z;
        p->vel_z     = 0.0f;
        p->on_ground = 1;
    }
    else
        p->on_ground = 0;
}

/*
** z_offset : décalage visuel vertical (bob + sneak).
** Exprimé en radians de pitch supplémentaire.
*/
static void update_z_offset(t_game *game)
{
    t_player    *p;
    float       target;
    float       limit;

    p     = &game->player;
    limit = (float)game->screen_h / 4.0f;
    if (!p->on_ground)
        target = p->vel_z * (float)game->screen_h * 2.0f;
    else if (p->sneaking)
        target = -(float)game->screen_h * 0.08f;
    else
        target = 0.0f;
    p->z_offset += (target - p->z_offset) * 0.15f;
    if (p->z_offset >  limit) p->z_offset =  limit;
    if (p->z_offset < -limit) p->z_offset = -limit;
}

static void update_bob(t_game *game)
{
    t_player    *p;
    const Uint8 *keys;
    int         moving;

    p      = &game->player;
    keys   = SDL_GetKeyboardState(NULL);
    moving = (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S]
        || keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D]);
    if (moving && p->on_ground)
    {
        p->bob_angle  += MC_BOB_SPEED;
        p->bob_offset  = sinf(p->bob_angle)
            * MC_BOB_AMP * (float)game->screen_h;
    }
    else
    {
        p->bob_offset *= 0.85f;
        if (fabsf(p->bob_offset) < 0.5f)
        {
            p->bob_offset = 0.0f;
            p->bob_angle  = 0.0f;
        }
    }
}

static void toggle_inventory(t_game *game)
{
    game->player.inv_open = !game->player.inv_open;
    if (game->player.inv_open)
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
    }
    else
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_ShowCursor(SDL_DISABLE);
    }
}

void move_player(t_game *game)
{
    t_player    *p;
    const Uint8 *keys;
    float       spd;
    float       strafe_x;
    float       strafe_y;

    if (game->player.inv_open)
    {
        apply_gravity(game);
        update_z_offset(game);
        return ;
    }
    p        = &game->player;
    keys     = SDL_GetKeyboardState(NULL);
    p->sneaking = (p->sneak_key != SDL_SCANCODE_UNKNOWN
        && keys[p->sneak_key]) ? 1 : 0;
    spd      = p->sneaking ? MC_SNEAK_SPEED : MC_WALK_SPEED;
    strafe_x =  p->dir_y;
    strafe_y = -p->dir_x;
    /*
    ** Déplacement avec step-up automatique (max 1 bloc).
    ** Si la tuile devant est plus haute d'au plus 1 bloc et
    ** que le joueur est au sol, on monte automatiquement.
    */
    {
        float nx, ny;
        float cur_surf, new_surf;
        cur_surf = floorf(world_get_height(&game->world,
            (int)floorf(p->x), (int)floorf(p->y)));

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])
        {
            nx = p->x + p->dir_x * spd;
            ny = p->y + p->dir_y * spd;
            if (!is_wall(game, nx, p->y))
            {
                new_surf = floorf(world_get_height(&game->world,
                    (int)floorf(nx), (int)floorf(p->y)));
                if (p->on_ground && new_surf - cur_surf <= 1.0f && new_surf - cur_surf > 0.0f)
                    p->vel_z = 0.12f;
                p->x = nx;
            }
            if (!is_wall(game, p->x, ny))
            {
                new_surf = floorf(world_get_height(&game->world,
                    (int)floorf(p->x), (int)floorf(ny)));
                if (p->on_ground && new_surf - cur_surf <= 1.0f && new_surf - cur_surf > 0.0f)
                    p->vel_z = 0.12f;
                p->y = ny;
            }
        }
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
        {
            nx = p->x - p->dir_x * spd;
            ny = p->y - p->dir_y * spd;
            if (!is_wall(game, nx, p->y))
            {
                new_surf = floorf(world_get_height(&game->world,
                    (int)floorf(nx), (int)floorf(p->y)));
                if (p->on_ground && new_surf - cur_surf <= 1.0f && new_surf - cur_surf > 0.0f)
                    p->vel_z = 0.12f;
                p->x = nx;
            }
            if (!is_wall(game, p->x, ny))
            {
                new_surf = floorf(world_get_height(&game->world,
                    (int)floorf(p->x), (int)floorf(ny)));
                if (p->on_ground && new_surf - cur_surf <= 1.0f && new_surf - cur_surf > 0.0f)
                    p->vel_z = 0.12f;
                p->y = ny;
            }
        }
        if (keys[SDL_SCANCODE_A])
        {
            nx = p->x - strafe_x * spd;
            ny = p->y - strafe_y * spd;
            if (!is_wall(game, nx, p->y))
            {
                new_surf = floorf(world_get_height(&game->world,
                    (int)floorf(nx), (int)floorf(p->y)));
                if (p->on_ground && new_surf - cur_surf <= 1.0f && new_surf - cur_surf > 0.0f)
                    p->vel_z = 0.12f;
                p->x = nx;
            }
            if (!is_wall(game, p->x, ny))
            {
                new_surf = floorf(world_get_height(&game->world,
                    (int)floorf(p->x), (int)floorf(ny)));
                if (p->on_ground && new_surf - cur_surf <= 1.0f && new_surf - cur_surf > 0.0f)
                    p->vel_z = 0.12f;
                p->y = ny;
            }
        }
        if (keys[SDL_SCANCODE_D])
        {
            nx = p->x + strafe_x * spd;
            ny = p->y + strafe_y * spd;
            if (!is_wall(game, nx, p->y))
            {
                new_surf = floorf(world_get_height(&game->world,
                    (int)floorf(nx), (int)floorf(p->y)));
                if (p->on_ground && new_surf - cur_surf <= 1.0f && new_surf - cur_surf > 0.0f)
                    p->vel_z = 0.12f;
                p->x = nx;
            }
            if (!is_wall(game, p->x, ny))
            {
                new_surf = floorf(world_get_height(&game->world,
                    (int)floorf(p->x), (int)floorf(ny)));
                if (p->on_ground && new_surf - cur_surf <= 1.0f && new_surf - cur_surf > 0.0f)
                    p->vel_z = 0.12f;
                p->y = ny;
            }
        }
    }
    if (keys[SDL_SCANCODE_LEFT])  rotate(p,  ROT_SPEED);
    if (keys[SDL_SCANCODE_RIGHT]) rotate(p, -ROT_SPEED);
    if (keys[SDL_SCANCODE_SPACE] && p->on_ground && !p->sneaking)
    {
        p->vel_z     = MC_JUMP_FORCE;
        p->on_ground = 0;
    }
    apply_gravity(game);
    update_z_offset(game);
    update_bob(game);
}

/*
** Gestion souris :
** - Rotation horizontale → yaw (rotate player dir)
** - Regard vertical → pitch en radians limité à ±PI/2.5
*/
static void handle_mouse(t_game *game, int xrel, int yrel)
{
    t_player    *p;
    float       pitch_limit;

    if (game->player.inv_open)
    {
        game->steve_mouse_dx += xrel;
        return ;
    }
    p           = &game->player;
    pitch_limit = (float)M_PI / 2.5f;

    if (xrel != 0)
        rotate(p, -(float)xrel * MOUSE_SENS);
    if (yrel != 0)
    {
        /*
        ** pitch stocké en "pixels d'offset" pour r3d_begin_frame()
        ** qui le convertit en angle via tanf().
        ** On limite à ±(screen_h/4) pixels.
        */
        float limit = (float)game->screen_h / 4.0f * (float)M_PI;
        p->pitch -= (float)yrel * MOUSE_SENS * 400.0f;
        if (p->pitch >  limit) p->pitch =  limit;
        if (p->pitch < -limit) p->pitch = -limit;
    }
    (void)pitch_limit;
}

void handle_events(t_game *game)
{
    SDL_Event   event;
    static int  first_move = 1;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            game->running = 0;
        if (event.type == SDL_KEYDOWN)
        {
            if (event.key.keysym.sym == SDLK_ESCAPE)
            {
                if (game->player.inv_open) toggle_inventory(game);
                else                       game->running = 0;
            }
            if (event.key.keysym.sym == SDLK_e)
                toggle_inventory(game);
            if (event.key.keysym.sym >= SDLK_1
                && event.key.keysym.sym <= SDLK_9)
                game->player.hotbar_slot = event.key.keysym.sym - SDLK_1;
        }
        if (event.type == SDL_MOUSEWHEEL && !game->player.inv_open)
        {
            game->player.hotbar_slot -= event.wheel.y;
            if (game->player.hotbar_slot < 0)
                game->player.hotbar_slot = 8;
            if (game->player.hotbar_slot > 8)
                game->player.hotbar_slot = 0;
        }
        if (event.type == SDL_MOUSEMOTION)
        {
            if (first_move) { first_move = 0; continue ; }
            handle_mouse(game, event.motion.xrel, event.motion.yrel);
        }
    }
    move_player(game);
    world_update(game);
}
