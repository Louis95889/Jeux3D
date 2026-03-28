#include "cub3d.h"

static void render_crosshair(t_game *game)
{
    int cx = game->screen_w / 2;
    int cy = game->screen_h / 2;
    int sz = 10;

    SDL_SetRenderDrawColor(game->renderer, 0, 0, 0, 180);
    SDL_RenderDrawLine(game->renderer, cx-sz+1, cy+1, cx+sz+1, cy+1);
    SDL_RenderDrawLine(game->renderer, cx+1, cy-sz+1, cx+1, cy+sz+1);
    SDL_SetRenderDrawColor(game->renderer, 255, 255, 255, 220);
    SDL_RenderDrawLine(game->renderer, cx-sz, cy, cx+sz, cy);
    SDL_RenderDrawLine(game->renderer, cx, cy-sz, cx, cy+sz);
}

static void render_hotbar(t_game *game)
{
    int      ss      = 50;
    int      pad     = 4;
    int      total_w = HOTBAR_SIZE * (ss + pad) - pad;
    int      start_x = (game->screen_w - total_w) / 2;
    int      base_y  = game->screen_h - ss - 20;
    int      i       = 0;
    SDL_Rect rect;

    while (i < HOTBAR_SIZE)
    {
        int x = start_x + i * (ss + pad);
        if (i == game->player.hotbar_slot)
        {
            rect = (SDL_Rect){x-3, base_y-3, ss+6, ss+6};
            SDL_SetRenderDrawColor(game->renderer, 255, 255, 255, 200);
            SDL_RenderFillRect(game->renderer, &rect);
            rect = (SDL_Rect){x-2, base_y-2, ss+4, ss+4};
            SDL_SetRenderDrawColor(game->renderer, 80, 80, 80, 220);
            SDL_RenderFillRect(game->renderer, &rect);
        }
        rect = (SDL_Rect){x, base_y, ss, ss};
        SDL_SetRenderDrawColor(game->renderer, 50, 50, 50, 200);
        SDL_RenderFillRect(game->renderer, &rect);
        SDL_SetRenderDrawColor(game->renderer, 120, 120, 120, 255);
        SDL_RenderDrawRect(game->renderer, &rect);
        i++;
    }
}

static void render_text(t_game *game, const char *txt, int x, int y)
{
    SDL_Color   white = {255, 255, 255, 255};
    SDL_Surface *surf;
    SDL_Texture *tex;
    SDL_Rect     dst;

    surf = TTF_RenderUTF8_Blended(game->font_small, txt, white);
    if (!surf) return ;
    tex = SDL_CreateTextureFromSurface(game->renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) return ;
    SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);
    dst.x = x; dst.y = y;
    SDL_RenderCopy(game->renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

static void render_stats(t_game *game)
{
    char buf[32];
    snprintf(buf, 32, "x = %.2f", game->player.x);
    render_text(game, buf, 10, 20);
    snprintf(buf, 32, "y = %.2f", game->player.y);
    render_text(game, buf, 10, 40);
    snprintf(buf, 32, "z = %.2f", game->player.z);
    render_text(game, buf, 10, 60);
}

static void draw_slot(t_game *game, int x, int y, int sz, int sel)
{
    SDL_Rect r = {x, y, sz, sz};

    SDL_SetRenderDrawColor(game->renderer, 40, 40, 40, 230);
    SDL_RenderFillRect(game->renderer, &r);
    if (sel)
        SDL_SetRenderDrawColor(game->renderer, 255, 255, 255, 255);
    else
        SDL_SetRenderDrawColor(game->renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(game->renderer, &r);
}

static void draw_label(t_game *game, const char *txt,
    int x, int y, SDL_Color col)
{
    SDL_Surface *surf;
    SDL_Texture *tex;
    SDL_Rect     dst;

    surf = TTF_RenderUTF8_Blended(game->font_small, txt, col);
    if (!surf) return ;
    tex = SDL_CreateTextureFromSurface(game->renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) return ;
    SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);
    dst.x = x; dst.y = y;
    SDL_RenderCopy(game->renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

/*
** Gestion drag souris pour rotation de Steve.
** Utilise les deltas accumulés depuis les événements SDL_MOUSEMOTION.
*/
static void handle_steve_drag(t_game *game, SDL_Rect zone)
{
    int    mx;
    int    my;
    Uint32 btn;

    btn = SDL_GetMouseState(&mx, &my);
    if (!(btn & SDL_BUTTON(SDL_BUTTON_LEFT)))
    {
        game->steve_dragging = 0;
        return ;
    }
    if (!game->steve_dragging)
    {
        if (mx >= zone.x && mx <= zone.x + zone.w
            && my >= zone.y && my <= zone.y + zone.h)
            game->steve_dragging = 1;
        return ;
    }
    game->steve_angle    += (float)game->steve_mouse_dx * 0.015f;
    game->steve_mouse_dx  = 0;
}

static void render_inventory(t_game *game)
{
    SDL_Color   white  = {255, 255, 255, 255};
    SDL_Color   gray   = {180, 180, 180, 255};
    SDL_Color   yellow = {255, 220,  50, 255};
    int         margin = 20;
    int         ss     = 44;
    int         pad    = 5;
    int         step   = ss + pad;
    int         inv_w  = 9 * step + margin * 2;
    int         inv_h  = 30 + 4*step + 20 + 3*step + 16 + step + 16;
    int         ox     = (game->screen_w - inv_w) / 2;
    int         oy     = (game->screen_h - inv_h) / 2 + 35;
    int         row, col, x, y;
    SDL_Rect    bg;

    /* Garder l'inventaire dans les limites de l'écran */
    if (ox < 0) ox = 0;
    if (oy < margin) oy = margin;
    if (oy + inv_h > game->screen_h - margin)
        oy = game->screen_h - inv_h - margin;

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    bg = (SDL_Rect){ox, oy, inv_w, inv_h};
    SDL_SetRenderDrawColor(game->renderer, 18, 18, 18, 220);
    SDL_RenderFillRect(game->renderer, &bg);
    SDL_SetRenderDrawColor(game->renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(game->renderer, &bg);

    draw_label(game, "Inventaire", ox + margin, oy + 8, white);

    int top_y = oy + 30;

    /* Emplacements armure */
    const char *armor[] = {"H", "T", "J", "B"};
    col = 0;
    while (col < 4)
    {
        y = top_y + col * step;
        draw_slot(game, ox + margin, y, ss, 0);
        draw_label(game, armor[col],
            ox + margin + ss/2 - 4, y + ss/2 - 8, gray);
        col++;
    }

    /* Zone Steve : calcul sécurisé de la taille */
    int sz_x = ox + margin + step + 4;
    int sz_w = 3 * step - 8;
    int sz_h = 4 * step - 4;

    /* Clamp pour ne pas dépasser le buffer pré-alloué */
    if (sz_w < 10) sz_w = 10;
    if (sz_h < 10) sz_h = 10;

    SDL_Rect zone = {sz_x, top_y, sz_w, sz_h};
    SDL_SetRenderDrawColor(game->renderer, 25, 25, 45, 210);
    SDL_RenderFillRect(game->renderer, &zone);
    SDL_SetRenderDrawColor(game->renderer, 80, 80, 110, 255);
    SDL_RenderDrawRect(game->renderer, &zone);

    /*
    ** Calcul de scale basé sur la taille de la zone, clampé
    ** pour que ça tienne dans le buffer steve pré-alloué.
    */
    float scale    = (float)sz_h / 42.0f;
    if (scale > 6.0f) scale = 6.0f;
    if (scale < 0.5f) scale = 0.5f;

    int   anchor_x = sz_x + sz_w / 2;
    int   anchor_y = top_y + sz_h;

    /* draw_steve_3d utilise le buffer pré-alloué → pas de crash */
    draw_steve_3d(game, anchor_x, anchor_y, game->steve_angle, scale);
    handle_steve_drag(game, zone);

    /* Fabrication 2x2 */
    int craft_x = ox + margin + 5 * step;
    draw_label(game, "Fabrication", craft_x, oy + 8, yellow);
    row = 0;
    while (row < 2)
    {
        col = 0;
        while (col < 2)
        {
            x = craft_x + col * step;
            y = top_y + row * step;
            draw_slot(game, x, y, ss, 0);
            col++;
        }
        row++;
    }
    draw_label(game, "->",
        craft_x + 2*step + 4, top_y + step/2 - 8, white);
    draw_slot(game,
        craft_x + 2*step + 28, top_y + step/2 - ss/2, ss, 0);

    /* Séparateur */
    int sep_y = top_y + 4*step + 8;
    SDL_SetRenderDrawColor(game->renderer, 80, 80, 80, 255);
    SDL_RenderDrawLine(game->renderer,
        ox + 8, sep_y, ox + inv_w - 8, sep_y);

    /* Inventaire 3×9 */
    int inv_y = sep_y + 14;
    row = 0;
    while (row < 3)
    {
        col = 0;
        while (col < 9)
        {
            x = ox + margin + col * step;
            y = inv_y + row * step;
            draw_slot(game, x, y, ss, 0);
            col++;
        }
        row++;
    }

    /* Séparateur hotbar */
    int sep2_y = inv_y + 3*step + 6;
    SDL_SetRenderDrawColor(game->renderer, 80, 80, 80, 255);
    SDL_RenderDrawLine(game->renderer,
        ox + 8, sep2_y, ox + inv_w - 8, sep2_y);

    /* Hotbar */
    int hb_y = sep2_y + 8;
    col = 0;
    while (col < 9)
    {
        x = ox + margin + col * step;
        draw_slot(game, x, hb_y, ss, col == game->player.hotbar_slot);
        col++;
    }
}

void render_hud(t_game *game)
{
    if (game->player.inv_open)
    {
        render_inventory(game);
        return ;
    }
    render_crosshair(game);
    render_hotbar(game);
    render_stats(game);
}
