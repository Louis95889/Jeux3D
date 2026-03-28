#include "cub3d.h"

int init_sdl(t_game *game)
{
    SDL_DisplayMode mode;
    int             steve_w;
    int             steve_h;

    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP,
        "1", SDL_HINT_OVERRIDE);
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return (-1); }
    if (TTF_Init() != 0)
    { fprintf(stderr, "TTF_Init: %s\n", TTF_GetError()); return (-1); }
    if (IMG_Init(IMG_INIT_PNG) == 0)
    { fprintf(stderr, "IMG_Init: %s\n", IMG_GetError()); return (-1); }
    SDL_GetCurrentDisplayMode(0, &mode);
    game->screen_w = mode.w;
    game->screen_h = mode.h;
    game->window = SDL_CreateWindow("cub3D", 0, 0,
        game->screen_w, game->screen_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!game->window) return (-1);
    game->renderer = SDL_CreateRenderer(game->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!game->renderer) return (-1);
    SDL_RenderSetLogicalSize(game->renderer, game->screen_w, game->screen_h);
    game->framebuf = SDL_CreateTexture(game->renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        game->screen_w, game->screen_h);
    if (!game->framebuf) return (-1);

    /* Pixel buffer (couleurs) */
    game->pixels = malloc(sizeof(Uint32) * game->screen_w * game->screen_h);
    if (!game->pixels) return (-1);

    /*
    ** zbuf : maintenant PER-PIXEL (screen_w × screen_h floats)
    ** pour un vrai z-buffer 3D (plus screen_w uniquement).
    */
    game->zbuf = malloc(sizeof(float) * game->screen_w * game->screen_h);
    if (!game->zbuf) return (-1);

    game->font = TTF_OpenFont(FONT_PATH, 32);
    if (!game->font) return (-1);
    game->font_small = TTF_OpenFont(FONT_PATH, 18);
    if (!game->font_small) return (-1);
    if (load_textures(game) != 0) return (-1);

    /*
    ** Pré-allocation de la texture Steve pour l'inventaire.
    ** Taille fixe suffisante : 200×320 pixels.
    ** On évite ainsi les malloc/free chaque frame qui causaient le crash.
    */
    steve_w = 200;
    steve_h = 320;
    game->steve_tex_w = steve_w;
    game->steve_tex_h = steve_h;
    game->steve_buf   = calloc(steve_w * steve_h, sizeof(Uint32));
    if (!game->steve_buf)
    {
        fprintf(stderr, "Cannot alloc steve_buf\n");
        return (-1);
    }
    game->steve_tex = SDL_CreateTexture(game->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        steve_w, steve_h);
    if (!game->steve_tex)
    {
        fprintf(stderr, "Cannot create steve_tex: %s\n", SDL_GetError());
        return (-1);
    }
    SDL_SetTextureBlendMode(game->steve_tex, SDL_BLENDMODE_BLEND);

    SDL_ShowCursor(SDL_ENABLE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    game->running            = 1;
    game->keybind_done       = 0;
    game->player.sneak_key   = SDL_SCANCODE_UNKNOWN;
    game->player.hotbar_slot = 0;
    game->obj_count          = 0;
    game->obj_def_count      = 0;
    return (0);
}

void cleanup(t_game *game)
{
    free_obj_defs(game);
    free_textures(game);
    if (game->pixels)     free(game->pixels);
    if (game->zbuf)       free(game->zbuf);
    if (game->steve_buf)  free(game->steve_buf);
    if (game->steve_tex)  SDL_DestroyTexture(game->steve_tex);
    if (game->framebuf)   SDL_DestroyTexture(game->framebuf);
    if (game->font)       TTF_CloseFont(game->font);
    if (game->font_small) TTF_CloseFont(game->font_small);
    TTF_Quit(); IMG_Quit();
    if (game->renderer) SDL_DestroyRenderer(game->renderer);
    if (game->window)   SDL_DestroyWindow(game->window);
    SDL_Quit();
}
