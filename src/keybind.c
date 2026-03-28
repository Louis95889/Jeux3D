#include "cub3d.h"

static void draw_text(t_game *game, const char *text, int y, SDL_Color color)
{
    SDL_Surface *surf;
    SDL_Texture *tex;
    SDL_Rect     dst;

    surf = TTF_RenderUTF8_Blended(game->font, text, color);
    if (!surf) return ;
    tex = SDL_CreateTextureFromSurface(game->renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) return ;
    SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);
    dst.x = (game->screen_w - dst.w) / 2;
    dst.y = y;
    SDL_RenderCopy(game->renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

void render_keybind_screen(t_game *game)
{
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 50,  255};
    SDL_Color gray   = {160, 160, 160, 255};

    SDL_SetRenderDrawColor(game->renderer, 10, 10, 10, 255);
    SDL_RenderClear(game->renderer);
    draw_text(game, "CONFIGURATION DES TOUCHES",
        game->screen_h / 2 - 120, yellow);
    draw_text(game, "Appuyez sur la touche que vous souhaitez",
        game->screen_h / 2 - 60, white);
    draw_text(game, "utiliser pour le SNEAK (accroupi)",
        game->screen_h / 2 - 20, white);
    draw_text(game, "Touches interdites : Z Q S D ESPACE CLIC",
        game->screen_h / 2 + 40, gray);
    draw_text(game, "Echap = quitter",
        game->screen_h / 2 + 90, gray);
    SDL_RenderPresent(game->renderer);
}

void handle_keybind_events(t_game *game)
{
    SDL_Event       event;
    SDL_Scancode    sc;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            game->running = 0;
            game->keybind_done = 1;
            return ;
        }
        if (event.type == SDL_KEYDOWN)
        {
            sc = event.key.keysym.scancode;
            if (sc == SDL_SCANCODE_ESCAPE)
            {
                game->running = 0;
                game->keybind_done = 1;
                return ;
            }
            if (sc == SDL_SCANCODE_W     || sc == SDL_SCANCODE_UP    ||
                sc == SDL_SCANCODE_S     || sc == SDL_SCANCODE_DOWN  ||
                sc == SDL_SCANCODE_A     || sc == SDL_SCANCODE_LEFT  ||
                sc == SDL_SCANCODE_D     || sc == SDL_SCANCODE_RIGHT ||
                sc == SDL_SCANCODE_SPACE)
                return ;
            game->player.sneak_key = sc;
            game->keybind_done     = 1;
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
    }
}
