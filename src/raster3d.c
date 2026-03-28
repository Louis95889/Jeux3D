#include "cub3d.h"

/*
** ============================================================
** raster3d.c — Rasteriseur software 3D complet
**
** Fonctionnement :
**   1. r3d_begin_frame() : construit mat_view + mat_proj chaque frame
**   2. r3d_draw_quad()   : projette 4 sommets → 2 triangles,
**                          rasterize avec zbuffer + UV perspective-correct
**   3. r3d_draw_billboard() : billboard toujours face caméra
**
** UVs en world-space : les coordonnées de texture sont calculées
** à partir de la position 3D du sommet dans le monde, ce qui garantit
** que les textures restent STATIQUES quelle que soit la distance.
**
** Perspective-correct : on interpole u/w, v/w, 1/w sur les scanlines,
** puis on récupère u = (u/w)/(1/w) par pixel → pas de déformation affine.
**
** Border 20% : si border=1, les pixels dans les 20% de bordure d'une face
** sont assombris à 80% pour créer l'effet contour bloc Minecraft.
** ============================================================ */

/* ────────────────────────────────────────────────────────────
** Mise à jour des matrices vue/projection chaque frame
** ──────────────────────────────────────────────────────────── */
void r3d_begin_frame(t_game *game)
{
    t_player    *p;
    t_vec3      eye;
    t_vec3      center;
    t_vec3      up;
    float       fov;
    float       aspect;
    float       pitch_rad;
    float       cos_p;

    p      = &game->player;
    fov    = FOV_DEG * (float)M_PI / 180.0f;
    aspect = (float)game->screen_w / (float)game->screen_h;

    /*
    ** Espace monde : player.x/y = plan carte, player.z = altitude.
    ** Convention LookAt : Y = haut monde.
    **   eye.x = carte x
    **   eye.y = altitude (z monde)
    **   eye.z = carte y
    */
    eye.x = p->x;
    eye.y = p->z + p->bob_offset * 0.01f;
    eye.z = p->y;

    /*
    ** pitch stocké en "pixels offset" dans events.c (limite = screen_h/4 * PI).
    ** Conversion en radians : pitch_rad = pitch / (screen_h * 0.5)
    ** On limite à ±85° pour éviter le gimbal lock.
    */
    pitch_rad = p->pitch / ((float)game->screen_h * 0.5f);
    if (pitch_rad >  1.48f) pitch_rad =  1.48f;
    if (pitch_rad < -1.48f) pitch_rad = -1.48f;

    cos_p     = cosf(pitch_rad);
    center.x  = eye.x + p->dir_x * cos_p;
    center.y  = eye.y + sinf(pitch_rad);
    center.z  = eye.z + p->dir_y * cos_p;

    up.x = 0.0f; up.y = 1.0f; up.z = 0.0f;

    game->mat_view = mat4_look_at(eye, center, up);
    game->mat_proj = mat4_perspective(fov, aspect, NEAR_PLANE, FAR_PLANE);
    game->mat_vp   = mat4_mul(game->mat_proj, game->mat_view);
}

/* NDC → pixel screen */
static void ndc_to_screen(t_game *game, t_vec4 clip, float *sx, float *sy)
{
    float ndc_x;
    float ndc_y;

    ndc_x = clip.x / clip.w;
    ndc_y = clip.y / clip.w;
    *sx   = (ndc_x + 1.0f) * 0.5f * (float)game->screen_w;
    *sy   = (1.0f - ndc_y) * 0.5f * (float)game->screen_h;
}

/* ────────────────────────────────────────────────────────────
** Échantillonnage de texture avec wrap
** ──────────────────────────────────────────────────────────── */
static Uint32 sample(t_tex *tex, float u, float v, float fog, int border,
    float bu, float bv)
{
    int    sx;
    int    sy;
    Uint32 px;
    Uint8  *p;
    Uint8  r;
    Uint8  g;
    Uint8  b;
    float  dim;

    if (!tex || !tex->pixels)
        return (0xFF808080u);

    /* Wrap */
    u = u - floorf(u);
    v = v - floorf(v);
    if (u < 0.0f) u += 1.0f;
    if (v < 0.0f) v += 1.0f;

    sx = (int)(u * (float)tex->w) % tex->w;
    sy = (int)(v * (float)tex->h) % tex->h;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    if (sx >= tex->w) sx = tex->w - 1;
    if (sy >= tex->h) sy = tex->h - 1;

    px = tex->pixels[sy * tex->w + sx] | 0xFF000000u;
    p  = (Uint8 *)&px;
    b  = p[0]; g = p[1]; r = p[2];

    /* Contour noir 20% : assombrir si dans les bordures */
    dim = 1.0f;
    if (border)
    {
        float edge = 0.20f;
        if (bu < edge || bu > 1.0f - edge || bv < edge || bv > 1.0f - edge)
            dim = 0.72f;
    }

    /* Brouillard */
    if (fog > 0.0f)
    {
        r = (Uint8)((float)r * dim * (1.0f - fog) + 180.0f * fog);
        g = (Uint8)((float)g * dim * (1.0f - fog) + 210.0f * fog);
        b = (Uint8)((float)b * dim * (1.0f - fog) + 255.0f * fog);
    }
    else
    {
        r = (Uint8)((float)r * dim);
        g = (Uint8)((float)g * dim);
        b = (Uint8)((float)b * dim);
    }
    return (0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b);
}

/* ────────────────────────────────────────────────────────────
** Rasterisation d'un triangle avec :
**   - Z-buffer (zbuf flottant par pixel)
**   - UV perspective-correct (interpolation 1/w, u/w, v/w)
**   - border (dégradé de bordure optionnel)
**
** Les UV passés sont en espace MONDE (world-space UVs)
** → la texture est statique quel que soit l'angle / la distance.
**
** Algorithme : parcours du bounding-box, test de half-space.
** ──────────────────────────────────────────────────────────── */
typedef struct s_tri_v
{
    float   sx, sy;     /* écran */
    float   invw;       /* 1/w clip */
    float   u_w;        /* u/w  */
    float   v_w;        /* v/w  */
}   t_tri_v;

static void raster_triangle(t_game *game, t_tri_v A, t_tri_v B, t_tri_v C,
    t_tex *tex, float fog, int border)
{
    int     min_x;
    int     max_x;
    int     min_y;
    int     max_y;
    int     x;
    int     y;
    float   denom;
    float   w0;
    float   w1;
    float   w2;
    float   wa;
    float   wb;
    float   wc;
    float   inv_w;
    float   u;
    float   v;
    float   depth;
    float   e01x;
    float   e01y;
    float   e20x;
    float   e20y;
    /* Pour les border UVs (position relative sur la face) */
    float   bu;
    float   bv;
    float   ba_u;
    float   ba_v;
    float   bb_u;
    float   bb_v;
    float   bc_u;
    float   bc_v;

    /* Edge vectors */
    e01x = B.sx - A.sx; e01y = B.sy - A.sy;
    e20x = A.sx - C.sx; e20y = A.sy - C.sy;

    /* Aire signée × 2 — on calcule denom pour les coordonnées barycentriques
    ** mais on ne fait PAS de backface culling : on rend les deux faces.
    ** (Le culling causait des faces manquantes à cause du winding variable.) */
    denom = e01x * (-e20y) - e01y * (-e20x);
    if (fabsf(denom) < 0.5f)
        return ; /* triangle dégénéré */

    /* Bounding box clippé à l'écran */
    min_x = (int)fminf(fminf(A.sx, B.sx), C.sx);
    max_x = (int)fmaxf(fmaxf(A.sx, B.sx), C.sx) + 1;
    min_y = (int)fminf(fminf(A.sy, B.sy), C.sy);
    max_y = (int)fmaxf(fmaxf(A.sy, B.sy), C.sy) + 1;
    if (min_x < 0)               min_x = 0;
    if (max_x >= game->screen_w) max_x = game->screen_w - 1;
    if (min_y < 0)               min_y = 0;
    if (max_y >= game->screen_h) max_y = game->screen_h - 1;

    /* Coordonnées border en UV non-corrigées (0-1 sur la face) */
    ba_u = A.u_w / (A.invw > 1e-9f ? A.invw : 1e-9f);
    ba_v = A.v_w / (A.invw > 1e-9f ? A.invw : 1e-9f);
    bb_u = B.u_w / (B.invw > 1e-9f ? B.invw : 1e-9f);
    bb_v = B.v_w / (B.invw > 1e-9f ? B.invw : 1e-9f);
    bc_u = C.u_w / (C.invw > 1e-9f ? C.invw : 1e-9f);
    bc_v = C.v_w / (C.invw > 1e-9f ? C.invw : 1e-9f);

    y = min_y;
    while (y <= max_y)
    {
        x = min_x;
        while (x <= max_x)
        {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;

            /* Coordonnées barycentriques */
            w0 = (B.sx - A.sx) * (py - A.sy) - (B.sy - A.sy) * (px - A.sx);
            w1 = (C.sx - B.sx) * (py - B.sy) - (C.sy - B.sy) * (px - B.sx);
            w2 = (A.sx - C.sx) * (py - C.sy) - (A.sy - C.sy) * (px - C.sx);

            /* Teste si le pixel est dans le triangle (signe adapté au winding) */
            if ((denom < 0.0f && w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f)
                || (denom > 0.0f && w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f))
            {
                /* Normalisation */
                float invd = 1.0f / (w0 + w1 + w2);
                wa = w0 * invd;
                wb = w1 * invd;
                wc = w2 * invd;

                /* Interpolation perspective-correct */
                inv_w = wa * A.invw + wb * B.invw + wc * C.invw;
                if (inv_w < 1e-9f) { x++; continue ; }

                depth = 1.0f / inv_w;

                /* Z-buffer */
                int idx = y * game->screen_w + x;
                if (depth >= game->zbuf[idx]) { x++; continue ; }

                /* UV perspective-correct */
                u = (wa * A.u_w + wb * B.u_w + wc * C.u_w) * depth;
                v = (wa * A.v_w + wb * B.v_w + wc * C.v_w) * depth;

                /* UV border (interpolation affine suffit pour les contours) */
                bu = wa * ba_u + wb * bb_u + wc * bc_u;
                bv = wa * ba_v + wb * bb_v + wc * bc_v;

                game->zbuf[idx]   = depth;
                game->pixels[idx] = sample(tex, u, v, fog, border, bu, bv);
            }
            x++;
        }
        y++;
    }
}

/* ────────────────────────────────────────────────────────────
** Clip d'un triangle contre le plan near (w > NEAR_PLANE).
** Retourne le nombre de triangles output (0, 1 ou 2).
** Les triangles clippés sont écrits dans out[0..1] (3 t_vert chacun).
**
** On clip en espace clip (avant division perspective) pour éviter
** les divisions par zéro et les artefacts.
** ──────────────────────────────────────────────────────────── */
typedef struct s_cvert
{
    t_vec4  clip;
    float   u;
    float   v;
}   t_cvert;

static float lerp_f(float a, float b, float t) { return a + (b - a) * t; }

static t_cvert lerp_cv(t_cvert a, t_cvert b, float t)
{
    t_cvert r;
    r.clip.x = lerp_f(a.clip.x, b.clip.x, t);
    r.clip.y = lerp_f(a.clip.y, b.clip.y, t);
    r.clip.z = lerp_f(a.clip.z, b.clip.z, t);
    r.clip.w = lerp_f(a.clip.w, b.clip.w, t);
    r.u      = lerp_f(a.u, b.u, t);
    r.v      = lerp_f(a.v, b.v, t);
    return (r);
}

static int clip_near(t_cvert in[3], t_cvert out[2][3])
{
    int     inside[3];
    int     n_in;
    int     i;
    int     j;

    n_in = 0;
    i = 0;
    while (i < 3)
    {
        inside[i] = (in[i].clip.w >= NEAR_PLANE) ? 1 : 0;
        if (inside[i]) n_in++;
        i++;
    }
    if (n_in == 0)
        return (0);
    if (n_in == 3)
    {
        out[0][0] = in[0]; out[0][1] = in[1]; out[0][2] = in[2];
        return (1);
    }
    /* Clipping partiel : on génère les sommets clippés */
    t_cvert clipped[4];
    int     nc = 0;

    i = 0;
    while (i < 3)
    {
        j = (i + 1) % 3;
        int ai = inside[i];
        int bi = inside[j];
        if (ai)
            clipped[nc++] = in[i];
        if (ai != bi)
        {
            /* Intersection avec plan w = NEAR_PLANE */
            float da = in[i].clip.w - NEAR_PLANE;
            float db = in[j].clip.w - NEAR_PLANE;
            float t  = da / (da - db);
            clipped[nc++] = lerp_cv(in[i], in[j], t);
        }
        i++;
    }
    if (nc < 3)
        return (0);
    /* nc == 3 ou 4 → fan de triangles */
    out[0][0] = clipped[0]; out[0][1] = clipped[1]; out[0][2] = clipped[2];
    if (nc == 4)
    {
        out[1][0] = clipped[0]; out[1][1] = clipped[2]; out[1][2] = clipped[3];
        return (2);
    }
    return (1);
}

/* ────────────────────────────────────────────────────────────
** Rasterise un triangle depuis l'espace clip (après clipping near).
** ──────────────────────────────────────────────────────────── */
static void raster_clip_tri(t_game *game, t_cvert cv[3],
    t_tex *tex, float fog, int border)
{
    t_tri_v A;
    t_tri_v B;
    t_tri_v C;

    /* Division perspective + écran */
    ndc_to_screen(game, cv[0].clip, &A.sx, &A.sy);
    ndc_to_screen(game, cv[1].clip, &B.sx, &B.sy);
    ndc_to_screen(game, cv[2].clip, &C.sx, &C.sy);

    A.invw = (cv[0].clip.w > 1e-9f) ? 1.0f / cv[0].clip.w : 0.0f;
    B.invw = (cv[1].clip.w > 1e-9f) ? 1.0f / cv[1].clip.w : 0.0f;
    C.invw = (cv[2].clip.w > 1e-9f) ? 1.0f / cv[2].clip.w : 0.0f;

    /* UV / w pour interpolation perspective-correct */
    A.u_w = cv[0].u * A.invw;
    A.v_w = cv[0].v * A.invw;
    B.u_w = cv[1].u * B.invw;
    B.v_w = cv[1].v * B.invw;
    C.u_w = cv[2].u * C.invw;
    C.v_w = cv[2].v * C.invw;

    raster_triangle(game, A, B, C, tex, fog, border);
}

/* ────────────────────────────────────────────────────────────
** Projection + clip + rasterisation d'un triangle (3 sommets monde)
** uv[0..2] = coordonnées texture de chaque sommet
** ──────────────────────────────────────────────────────────── */
static void draw_tri(t_game *game,
    t_vec3 p0, float u0, float v0,
    t_vec3 p1, float u1, float v1,
    t_vec3 p2, float u2, float v2,
    t_tex *tex, float fog, int border)
{
    t_cvert     in[3];
    t_cvert     out[2][3];
    int         n;
    int         i;

    in[0].clip = mat4_mul_vec4(game->mat_vp, (t_vec4){p0.x,p0.y,p0.z,1.f});
    in[0].u    = u0; in[0].v = v0;
    in[1].clip = mat4_mul_vec4(game->mat_vp, (t_vec4){p1.x,p1.y,p1.z,1.f});
    in[1].u    = u1; in[1].v = v1;
    in[2].clip = mat4_mul_vec4(game->mat_vp, (t_vec4){p2.x,p2.y,p2.z,1.f});
    in[2].u    = u2; in[2].v = v2;

    n = clip_near(in, out);
    i = 0;
    while (i < n)
    {
        raster_clip_tri(game, out[i], tex, fog, border);
        i++;
    }
}

/* ────────────────────────────────────────────────────────────
** Interface publique : dessine un quad (rectangle 3D) en 2 triangles.
**
** Sommets : p0=bas-gauche, p1=bas-droite, p2=haut-droite, p3=haut-gauche
**           (sens anti-horaire vu de face)
**
** Les UVs sont en espace monde → texture statique.
** Le paramètre border active le contour 20%.
** ──────────────────────────────────────────────────────────── */
void r3d_draw_quad(t_game *game,
    t_vec3 p0, t_vec3 p1, t_vec3 p2, t_vec3 p3,
    float u0, float v0, float u1, float v1,
    t_tex *tex, float fog, int border)
{
    /*
    ** Quad : p0=coin0(u0,v0), p1=coin1(u1,v0)
    **        p2=coin2(u1,v1), p3=coin3(u0,v1)
    ** L'appelant définit le sens des UV via u0,v0,u1,v1.
    ** Triangle 1 : p0(u0,v0), p1(u1,v0), p2(u1,v1)
    ** Triangle 2 : p0(u0,v0), p2(u1,v1), p3(u0,v1)
    */
    draw_tri(game,
        p0, u0, v0,
        p1, u1, v0,
        p2, u1, v1,
        tex, fog, border);
    draw_tri(game,
        p0, u0, v0,
        p2, u1, v1,
        p3, u0, v1,
        tex, fog, border);
}

/* ────────────────────────────────────────────────────────────
** Billboard : sprite toujours face caméra.
** Position monde (wx, wy, wz_bot..wz_top), demi-largeur half_w.
** UVs : u0..u1 horizontal, 0..1 vertical.
** ──────────────────────────────────────────────────────────── */
void r3d_draw_billboard(t_game *game,
    float wx, float wy, float wz_bot, float wz_top,
    float half_w, t_tex *tex, float u0, float u1)
{
    float   dx;
    float   dz;
    float   len;
    float   rx;
    float   rz;
    t_vec3  p0, p1, p2, p3;
    float   dist;
    float   fog;

    /* Vecteur caméra → billboard */
    dx  = wx - game->player.x;
    dz  = wy - game->player.y;
    len = sqrtf(dx * dx + dz * dz);
    if (len < 0.01f) return ;

    /* Vecteur perpendiculaire (droit) */
    rx = -dz / len * half_w;
    rz =  dx / len * half_w;

    /* 4 coins : bas-gauche, bas-droite, haut-droite, haut-gauche
    ** Espace monde : x=carte-x, y=altitude, z=carte-y */
    p0 = (t_vec3){ wx - rx, wz_bot, wy - rz };
    p1 = (t_vec3){ wx + rx, wz_bot, wy + rz };
    p2 = (t_vec3){ wx + rx, wz_top, wy + rz };
    p3 = (t_vec3){ wx - rx, wz_top, wy - rz };

    dist = len;
    fog  = (dist - 3.0f) / (MAX_DIST - 3.0f);
    if (fog < 0.0f) fog = 0.0f;
    if (fog > 1.0f) fog = 1.0f;

    /* Pour les billboards, les UVs sont fixes (0→1 vertical, u0→u1 horiz) */
    draw_tri(game,
        p0, u0, 1.0f,
        p1, u1, 1.0f,
        p2, u1, 0.0f,
        tex, fog, 0);
    draw_tri(game,
        p0, u0, 1.0f,
        p2, u1, 0.0f,
        p3, u0, 0.0f,
        tex, fog, 0);
}
