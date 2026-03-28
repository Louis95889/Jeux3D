#include "cub3d.h"

/*
** player3d.c — Rendu de Steve en 3D isométrique dans l'inventaire.
**
** CORRECTIONS crash inventaire :
**   - On n'appelle plus SDL_CreateTexture / SDL_DestroyTexture par frame.
**     La texture steve_tex est pré-allouée une fois dans init.c
**     (game->steve_tex, game->steve_buf).
**   - On vérifie que game->steve_tex != NULL avant toute opération SDL.
**   - La taille du buffer est fixe (steve_tex_w × steve_tex_h).
**   - On efface le buffer avec memset() avant chaque dessin.
*/

typedef struct s_v3  { float x; float y; float z; } t_v3;
typedef struct s_pt  { float x; float y; }           t_pt;

static t_v3 rotate_y(t_v3 v, float a)
{
    t_v3 r;
    r.x =  v.x * cosf(a) + v.z * sinf(a);
    r.y =  v.y;
    r.z = -v.x * sinf(a) + v.z * cosf(a);
    return (r);
}

static t_pt project(t_v3 v, int ox, int oy, float s)
{
    t_pt p;
    p.x = ox + (v.x - v.z) * 0.7f * s;
    p.y = oy - v.y * s + (v.x + v.z) * 0.35f * s;
    return (p);
}

static int face_visible(t_pt p[4])
{
    float ax;
    float ay;
    float bx;
    float by;

    ax = p[1].x - p[0].x; ay = p[1].y - p[0].y;
    bx = p[3].x - p[0].x; by = p[3].y - p[0].y;
    return ((ax * by - ay * bx) > 0);
}

static void clamp_uv(float *u, float *v)
{
    if (*u < 0.0f) *u = 0.0f;
    if (*u > 1.0f) *u = 1.0f;
    if (*v < 0.0f) *v = 0.0f;
    if (*v > 1.0f) *v = 1.0f;
}

static void draw_face_affine(Uint32 *buf, int bw, int bh,
    t_pt pts[4], t_tex *tex, int bright)
{
    float   e0x, e0y, e1x, e1y, det, px, py, u, v;
    int     min_x, max_x, min_y, max_y, x, y, sx, sy, i;
    Uint32  pixel;
    Uint8   *p, r, g, b, a;

    if (!tex || !tex->pixels) return ;

    e0x = pts[1].x - pts[0].x; e0y = pts[1].y - pts[0].y;
    e1x = pts[3].x - pts[0].x; e1y = pts[3].y - pts[0].y;
    det = e0x * e1y - e0y * e1x;
    if (fabsf(det) < 0.001f)
        return ;

    min_x = (int)pts[0].x; max_x = (int)pts[0].x;
    min_y = (int)pts[0].y; max_y = (int)pts[0].y;
    i = 1;
    while (i < 4)
    {
        if ((int)pts[i].x < min_x) min_x = (int)pts[i].x;
        if ((int)pts[i].x > max_x) max_x = (int)pts[i].x;
        if ((int)pts[i].y < min_y) min_y = (int)pts[i].y;
        if ((int)pts[i].y > max_y) max_y = (int)pts[i].y;
        i++;
    }
    min_x--; min_y--; max_x++; max_y++;
    if (min_x < 0)   min_x = 0;
    if (max_x >= bw) max_x = bw - 1;
    if (min_y < 0)   min_y = 0;
    if (max_y >= bh) max_y = bh - 1;

    y = min_y;
    while (y <= max_y)
    {
        x = min_x;
        while (x <= max_x)
        {
            px = (float)x - pts[0].x;
            py = (float)y - pts[0].y;
            u  = (px * e1y - py * e1x) / det;
            v  = (e0x * py - e0y * px) / det;
            if (u >= -0.02f && u <= 1.02f && v >= -0.02f && v <= 1.02f)
            {
                clamp_uv(&u, &v);
                sx = (int)(u * (tex->w - 1));
                sy = (int)(v * (tex->h - 1));
                if (sx < 0) sx = 0;
                if (sy < 0) sy = 0;
                if (sx >= tex->w) sx = tex->w - 1;
                if (sy >= tex->h) sy = tex->h - 1;
                pixel = tex->pixels[sy * tex->w + sx];
                p = (Uint8 *)&pixel;
                b = p[0]; g = p[1]; r = p[2]; a = p[3];
                if (a > 10)
                    buf[y * bw + x] = 0xFF000000
                        | ((Uint32)(r * bright / 255) << 16)
                        | ((Uint32)(g * bright / 255) << 8)
                        | ((Uint32)(b * bright / 255));
            }
            x++;
        }
        y++;
    }
}

static void draw_cube_part(t_game *g, Uint32 *buf, int bw, int bh,
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    float angle, int ox, int oy, float s,
    int f_front, int f_back, int f_right, int f_left, int f_top,
    int swap_rl)
{
    t_v3 raw[8] = {
        {x1,y1,z1},{x2,y1,z1},{x2,y1,z2},{x1,y1,z2},
        {x1,y2,z1},{x2,y2,z1},{x2,y2,z2},{x1,y2,z2}
    };
    t_v3 c[8];
    t_pt pts[4];
    int  i = 0;
    int  fr = swap_rl ? f_left  : f_right;
    int  fl = swap_rl ? f_right : f_left;

    /* Vérification des indices de faces */
    if (f_front < 0 || f_front >= SKIN_FACES) return ;
    if (f_back  < 0 || f_back  >= SKIN_FACES) return ;
    if (f_right < 0 || f_right >= SKIN_FACES) return ;
    if (f_left  < 0 || f_left  >= SKIN_FACES) return ;
    if (f_top   < 0 || f_top   >= SKIN_FACES) return ;

    while (i < 8) { c[i] = rotate_y(raw[i], angle); i++; }

    pts[0]=project(c[5],ox,oy,s); pts[1]=project(c[4],ox,oy,s);
    pts[2]=project(c[0],ox,oy,s); pts[3]=project(c[1],ox,oy,s);
    if (face_visible(pts))
        draw_face_affine(buf,bw,bh,pts,&g->faces[f_back],150);

    pts[0]=project(c[4],ox,oy,s); pts[1]=project(c[7],ox,oy,s);
    pts[2]=project(c[3],ox,oy,s); pts[3]=project(c[0],ox,oy,s);
    if (face_visible(pts))
        draw_face_affine(buf,bw,bh,pts,&g->faces[fl],160);

    pts[0]=project(c[6],ox,oy,s); pts[1]=project(c[5],ox,oy,s);
    pts[2]=project(c[1],ox,oy,s); pts[3]=project(c[2],ox,oy,s);
    if (face_visible(pts))
        draw_face_affine(buf,bw,bh,pts,&g->faces[fr],160);

    pts[0]=project(c[4],ox,oy,s); pts[1]=project(c[5],ox,oy,s);
    pts[2]=project(c[6],ox,oy,s); pts[3]=project(c[7],ox,oy,s);
    if (face_visible(pts))
        draw_face_affine(buf,bw,bh,pts,&g->faces[f_top],210);

    pts[0]=project(c[7],ox,oy,s); pts[1]=project(c[6],ox,oy,s);
    pts[2]=project(c[2],ox,oy,s); pts[3]=project(c[3],ox,oy,s);
    if (face_visible(pts))
        draw_face_affine(buf,bw,bh,pts,&g->faces[f_front],255);
}

/*
** draw_steve_3d : utilise la texture pré-allouée game->steve_tex.
** Ne crée plus de texture SDL à chaque appel → pas de crash.
**
** ox, oy : point d'ancrage (bas du personnage) dans l'UI renderer
** angle   : rotation Y
** scale   : facteur de taille
*/
void draw_steve_3d(t_game *g, int ox, int oy, float angle, float scale)
{
    int     bw;
    int     bh;
    int     lox;
    int     loy;
    SDL_Rect dst;

    if (!g->steve_tex || !g->steve_buf)
        return ;

    bw  = g->steve_tex_w;
    bh  = g->steve_tex_h;

    /* Effacer le buffer (transparent) */
    memset(g->steve_buf, 0, (size_t)(bw * bh) * sizeof(Uint32));

    /* Point d'ancrage local dans le buffer */
    lox = bw / 2;
    loy = (int)(bh * 0.80f);

    /* S'assurer que scale est raisonnable */
    if (scale < 0.5f)  scale = 0.5f;
    if (scale > 8.0f)  scale = 8.0f;

    draw_cube_part(g, g->steve_buf, bw, bh,
        -4,0,-2, 0,12,2, angle, lox, loy, scale,
        LEG_L_FRONT, LEG_L_BACK, LEG_L_RIGHT, LEG_L_LEFT, LEG_L_TOP, 0);

    draw_cube_part(g, g->steve_buf, bw, bh,
        0,0,-2, 4,12,2, angle, lox, loy, scale,
        LEG_R_FRONT, LEG_R_BACK, LEG_R_RIGHT, LEG_R_LEFT, LEG_R_TOP, 1);

    draw_cube_part(g, g->steve_buf, bw, bh,
        -4,12,-2, 4,24,2, angle, lox, loy, scale,
        BODY_FRONT, BODY_BACK, BODY_RIGHT, BODY_LEFT, BODY_TOP, 1);

    draw_cube_part(g, g->steve_buf, bw, bh,
        -6,12,-2, -4,24,2, angle, lox, loy, scale,
        ARM_L_FRONT, ARM_L_BACK, ARM_L_RIGHT, ARM_L_LEFT, ARM_L_TOP, 0);

    draw_cube_part(g, g->steve_buf, bw, bh,
        4,12,-2, 6,24,2, angle, lox, loy, scale,
        ARM_R_FRONT, ARM_R_BACK, ARM_R_RIGHT, ARM_R_LEFT, ARM_R_TOP, 1);

    draw_cube_part(g, g->steve_buf, bw, bh,
        -4,24,-4, 4,32,4, angle, lox, loy, scale,
        HEAD_FRONT, HEAD_BACK, HEAD_RIGHT, HEAD_LEFT, HEAD_TOP, 0);

    /* Mettre à jour la texture pré-allouée */
    SDL_UpdateTexture(g->steve_tex, NULL,
        g->steve_buf, bw * (int)sizeof(Uint32));

    /* Rendre dans la zone UI */
    dst = (SDL_Rect){ ox - bw / 2, oy - bh + 30, bw, bh };
    SDL_RenderCopy(g->renderer, g->steve_tex, NULL, &dst);
}
