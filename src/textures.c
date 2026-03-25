#include "cub3d.h"

int load_one_tex(t_tex *tex, const char *path)
{
    SDL_Surface *raw;
    SDL_Surface *cvt;
    int         x;
    int         y;

    raw = IMG_Load(path);
    if (!raw)
    {
        fprintf(stderr, "IMG_Load error (%s): %s\n", path, IMG_GetError());
        return (-1);
    }
    cvt = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(raw);
    if (!cvt)
        return (-1);
    tex->w      = cvt->w;
    tex->h      = cvt->h;
    tex->pixels = malloc(sizeof(Uint32) * tex->w * tex->h);
    if (!tex->pixels)
    {
        SDL_FreeSurface(cvt);
        return (-1);
    }
    SDL_LockSurface(cvt);
    y = 0;
    while (y < tex->h)
    {
        x = 0;
        while (x < tex->w)
        {
            tex->pixels[y * tex->w + x] =
                ((Uint32 *)cvt->pixels)[y * (cvt->pitch / 4) + x];
            x++;
        }
        y++;
    }
    SDL_UnlockSurface(cvt);
    SDL_FreeSurface(cvt);
    return (0);
}

static int load_skin(t_game *game)
{
    SDL_Surface *raw;
    SDL_Surface *cvt;
    int         x;
    int         y;

    raw = IMG_Load("textures/skin.png");
    if (!raw)
    {
        fprintf(stderr, "Skin load error: %s\n", IMG_GetError());
        return (-1);
    }
    cvt = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(raw);
    if (!cvt) return (-1);
    game->skin_w      = cvt->w;
    game->skin_h      = cvt->h;
    game->skin_pixels = malloc(sizeof(Uint32) * cvt->w * cvt->h);
    if (!game->skin_pixels) { SDL_FreeSurface(cvt); return (-1); }
    SDL_LockSurface(cvt);
    y = 0;
    while (y < cvt->h)
    {
        x = 0;
        while (x < cvt->w)
        {
            game->skin_pixels[y * cvt->w + x] =
                ((Uint32 *)cvt->pixels)[y * (cvt->pitch / 4) + x];
            x++;
        }
        y++;
    }
    SDL_UnlockSurface(cvt);
    game->skin_tex = SDL_CreateTextureFromSurface(game->renderer, cvt);
    SDL_FreeSurface(cvt);
    if (!game->skin_tex) return (-1);
    SDL_SetTextureBlendMode(game->skin_tex, SDL_BLENDMODE_BLEND);
    return (0);
}

int load_textures(t_game *game)
{
    if (load_one_tex(&game->tex[TEX_WALL_NS], "textures/wall_ns.png") != 0)
        return (-1);
    if (load_one_tex(&game->tex[TEX_WALL_EO], "textures/wall_eo.png") != 0)
        return (-1);
    if (load_one_tex(&game->tex[TEX_GRASS],   "textures/grass_top.png") != 0)
        return (-1);
    if (load_one_tex(&game->tex[TEX_DIRT],    "textures/grass_side.png") != 0)
        return (-1);
    if (load_one_tex(&game->tex[TEX_STONE],   "textures/rock.png") != 0)
        return (-1);
    if (load_one_tex(&game->tex[TEX_TRUNK],   "textures/trunk.png") != 0)
        return (-1);
    if (load_one_tex(&game->tex[TEX_LEAF],    "textures/leaves.png") != 0)
        return (-1);
    if (load_skin(game) != 0)
        return (-1);
    return (0);
}

void free_textures(t_game *game)
{
    int i;

    i = 0;
    while (i < TEX_COUNT)
    {
        if (game->tex[i].pixels)
        {
            free(game->tex[i].pixels);
            game->tex[i].pixels = NULL;
        }
        i++;
    }
    if (game->skin_pixels) { free(game->skin_pixels); game->skin_pixels = NULL; }
    if (game->skin_tex)
    {
        SDL_DestroyTexture(game->skin_tex);
        game->skin_tex = NULL;
    }
}
