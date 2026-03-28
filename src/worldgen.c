#include "cub3d.h"

# define HT_SIZE 128
# define HT_MASK (HT_SIZE - 1)
# define HT_EMPTY -1

static int ht_hash(int cx, int cy)
{
    return ((cx * 73856093 ^ cy * 19349663) & HT_MASK);
}

static void ht_insert(t_world *world, int slot)
{
    int h;
    int i;

    h = ht_hash(world->chunks[slot].cx, world->chunks[slot].cy);
    i = 0;
    while (i < HT_SIZE)
    {
        int idx = (h + i) & HT_MASK;
        if (world->ht[idx] == HT_EMPTY)
        {
            world->ht[idx] = slot;
            return ;
        }
        i++;
    }
}

static void ht_remove(t_world *world, int slot)
{
    int h;
    int i;

    h = ht_hash(world->chunks[slot].cx, world->chunks[slot].cy);
    i = 0;
    while (i < HT_SIZE)
    {
        int idx = (h + i) & HT_MASK;
        if (world->ht[idx] == slot)
        {
            world->ht[idx] = HT_EMPTY;
            return ;
        }
        i++;
    }
}

static int ht_find(t_world *world, int cx, int cy)
{
    int h;
    int i;
    int idx;
    int slot;

    h = ht_hash(cx, cy);
    i = 0;
    while (i < HT_SIZE)
    {
        idx  = (h + i) & HT_MASK;
        slot = world->ht[idx];
        if (slot == HT_EMPTY)
            return (-1);
        if (world->chunks[slot].loaded
            && world->chunks[slot].cx == cx
            && world->chunks[slot].cy == cy)
            return (slot);
        i++;
    }
    return (-1);
}

static float wg_fade(float t)
{
    return (t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f));
}

static float wg_lerp(float a, float b, float t)
{
    return (a + t * (b - a));
}

static float wg_grad(int hash, float x, float y)
{
    int h;

    h = hash & 3;
    if (h == 0) return ( x + y);
    if (h == 1) return (-x + y);
    if (h == 2) return ( x - y);
    return (-x - y);
}

static float wg_noise(int *perm, float x, float y)
{
    int   xi;
    int   yi;
    float xf;
    float yf;
    float u;
    float v;
    int   aa;
    int   ab;
    int   ba;
    int   bb;

    xi = (int)floorf(x) & 255;
    yi = (int)floorf(y) & 255;
    xf = x - floorf(x);
    yf = y - floorf(y);
    u  = wg_fade(xf);
    v  = wg_fade(yf);
    aa = perm[perm[xi]     + yi];
    ab = perm[perm[xi]     + yi + 1];
    ba = perm[perm[xi + 1] + yi];
    bb = perm[perm[xi + 1] + yi + 1];
    return (wg_lerp(
        wg_lerp(wg_grad(aa, xf,        yf),
                wg_grad(ba, xf - 1.0f, yf),        u),
        wg_lerp(wg_grad(ab, xf,        yf - 1.0f),
                wg_grad(bb, xf - 1.0f, yf - 1.0f), u),
        v));
}

static float wg_fbm(int *perm, float x, float y,
    int octaves, float freq, float persist)
{
    float val;
    float amp;
    float mx;
    int   i;

    val = 0.0f; amp = 1.0f; mx = 0.0f; i = 0;
    while (i < octaves)
    {
        val += wg_noise(perm, x * freq, y * freq) * amp;
        mx  += amp; amp *= persist; freq *= 2.0f; i++;
    }
    return (val / mx);
}

void world_init(t_world *world, int seed)
{
    int         i;
    int         j;
    int         tmp;
    long long   s;

    memset(world, 0, sizeof(t_world));
    if (seed == 0)
        seed = (int)(SDL_GetTicks() % 99991) + 1;
    world->seed = seed;
    world->last_pcx = -99999;
    world->last_pcy = -99999;
    i = 0;
    while (i < HT_SIZE)
    {
        world->ht[i] = HT_EMPTY;
        i++;
    }
    i = 0;
    while (i < 256)
    {
        world->perm[i] = i;
        i++;
    }
    s = (long long)seed;
    i = 255;
    while (i > 0)
    {
        s = (s * 6364136223846793005LL) + 1442695040888963407LL;
        j = (int)(((unsigned long long)s >> 33) % (unsigned int)(i + 1));
        if (j < 0) j = -j;
        tmp = world->perm[i];
        world->perm[i] = world->perm[j];
        world->perm[j] = tmp;
        i--;
    }
    i = 0;
    while (i < 256)
    {
        world->perm[i + 256]   = world->perm[i];
        world->perm2[i]        = world->perm[(i + 37) & 255];
        world->perm2[i + 256]  = world->perm2[i];
        world->perm3[i]        = world->perm[(i + 89) & 255];
        world->perm3[i + 256]  = world->perm3[i];
        world->perm_h[i]       = world->perm[(i + 131) & 255];
        world->perm_h[i + 256] = world->perm_h[i];
        i++;
    }
    world->game_ptr = NULL;
}

static float calc_height_raw(t_world *world, int wx, int wy)
{
    float base;
    float detail;
    float h;

    base   = wg_fbm(world->perm_h, (float)wx, (float)wy, 4, 0.008f, 0.6f);
    detail = wg_fbm(world->perm_h,
        (float)wx + 500.0f, (float)wy + 500.0f, 2, 0.04f, 0.5f);
    h = 64.0f + base * 16.0f + detail * 3.0f;
    if (h < Z_FLOOR) h = Z_FLOOR;
    if (h > 96.0f)   h = 96.0f;
    return (h);
}

static float get_humidity(t_world *world, int wx, int wy)
{
    float h;

    h = wg_fbm(world->perm3, (float)wx, (float)wy, 4, 0.013f, 0.5f);
    h = (h + 1.0f) * 0.5f;
    if (h < 0.0f) h = 0.0f;
    if (h > 1.0f) h = 1.0f;
    return (h);
}

int world_get_biome(t_world *world, int wx, int wy)
{
    return (get_humidity(world, wx, wy) >= 0.45f) ? 1 : 0;
}

float world_get_height(t_world *world, int wx, int wy)
{
    int cx;
    int cy;
    int lx;
    int ly;
    int idx;

    cx = (wx >= 0) ? wx / CHUNK_SIZE : (wx - CHUNK_SIZE + 1) / CHUNK_SIZE;
    cy = (wy >= 0) ? wy / CHUNK_SIZE : (wy - CHUNK_SIZE + 1) / CHUNK_SIZE;
    lx = wx - cx * CHUNK_SIZE;
    ly = wy - cy * CHUNK_SIZE;
    if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE)
        return (64.0f);
    idx = ht_find(world, cx, cy);
    if (idx >= 0)
        return (world->chunks[idx].height[ly][lx]);
    return (calc_height_raw(world, wx, wy));
}

int world_get_block(t_world *world, int wx, int wy, int z)
{
    int     cx;
    int     cy;
    int     idx;
    int     i;
    t_chunk *chunk;

    cx  = (wx >= 0) ? wx / CHUNK_SIZE : (wx - CHUNK_SIZE + 1) / CHUNK_SIZE;
    cy  = (wy >= 0) ? wy / CHUNK_SIZE : (wy - CHUNK_SIZE + 1) / CHUNK_SIZE;
    idx = ht_find(world, cx, cy);
    if (idx < 0)
        return (BTYPE_AIR);
    chunk = &world->chunks[idx];
    i = 0;
    while (i < chunk->nb_extra)
    {
        if (chunk->extra_blocks[i].wx == wx
            && chunk->extra_blocks[i].wy == wy
            && chunk->extra_blocks[i].z == z)
            return (chunk->extra_blocks[i].type);
        i++;
    }
    return (BTYPE_AIR);
}

static void add_block(t_chunk *chunk, int wx, int wy, int z, int type)
{
    if (chunk->nb_extra >= MAX_TREE_BLOCKS)
        return ;
    chunk->extra_blocks[chunk->nb_extra].wx   = wx;
    chunk->extra_blocks[chunk->nb_extra].wy   = wy;
    chunk->extra_blocks[chunk->nb_extra].z    = z;
    chunk->extra_blocks[chunk->nb_extra].type = type;
    chunk->nb_extra++;
}

static void gen_tree(t_chunk *chunk, int wx, int wy, int base_z)
{
    int trunk_h;
    int i;
    int dx;
    int dy;
    int lz;

    trunk_h = 4;
    i = 0;
    while (i < trunk_h)
    {
        add_block(chunk, wx, wy, base_z + i, BTYPE_TRUNK);
        i++;
    }
    lz = 0;
    while (lz < 2)
    {
        dy = -1;
        while (dy <= 1)
        {
            dx = -1;
            while (dx <= 1)
            {
                if (dx != 0 || dy != 0)
                    add_block(chunk, wx + dx, wy + dy,
                        base_z + trunk_h - 1 + lz, BTYPE_LEAF);
                dx++;
            }
            dy++;
        }
        lz++;
    }
    add_block(chunk, wx, wy, base_z + trunk_h, BTYPE_LEAF);
    add_block(chunk, wx, wy, base_z + trunk_h + 1, BTYPE_LEAF);
}

static void gen_chunk_tiles(t_world *world, t_chunk *chunk)
{
    int   lx;
    int   ly;
    int   wx;
    int   wy;
    int   biome;
    int   hash;
    float detail;
    float cave_n;
    float h;

    ly = 0;
    while (ly < CHUNK_SIZE)
    {
        lx = 0;
        while (lx < CHUNK_SIZE)
        {
            wx  = chunk->cx * CHUNK_SIZE + lx;
            wy  = chunk->cy * CHUNK_SIZE + ly;
            h   = calc_height_raw(world, wx, wy);
            chunk->height[ly][lx] = h;
            biome  = world_get_biome(world, wx, wy);
            hash   = ((wx * 73856093) ^ (wy * 19349663)) & 0x7FFFFFFF;
            hash   = hash % 100;
            detail = wg_fbm(world->perm, (float)wx, (float)wy,
                2, 0.2f, 0.6f);
            cave_n = wg_fbm(world->perm2, (float)wx, (float)wy,
                3, 0.08f, 0.5f);
            if (cave_n > 0.55f && hash < 15)
            {
                chunk->height[ly][lx] = h - 3.0f - (cave_n - 0.55f) * 10.0f;
                if (chunk->height[ly][lx] < Z_FLOOR + 2.0f)
                    chunk->height[ly][lx] = Z_FLOOR + 2.0f;
                chunk->tiles[ly][lx] = TILE_EMPTY;
            }
            else if (biome == 1 && detail > 0.42f && hash < 8)
                chunk->tiles[ly][lx] = TILE_WALL;
            else
                chunk->tiles[ly][lx] = TILE_EMPTY;
            if (chunk->tiles[ly][lx] == TILE_EMPTY
                && chunk->nb_extra + 20 < MAX_TREE_BLOCKS)
            {
                int tree_h = ((wx * 31337) ^ (wy * 7919)) & 0x7FFFFFFF;
                tree_h = tree_h % 100;
                if ((biome == 1 && tree_h < 20)
                    || (biome == 0 && tree_h < 3))
                    gen_tree(chunk, wx, wy,
                        (int)floorf(chunk->height[ly][lx]) + 1);
            }
            lx++;
        }
        ly++;
    }
    chunk->tiles_ready = 1;
}

static void gen_chunk_objects(t_game *game, t_chunk *chunk)
{
    int   lx;
    int   ly;
    int   wx;
    int   wy;
    int   biome;
    int   hash;
    float px;
    float py;
    float dx;
    float dy;

    ly = 0;
    while (ly < CHUNK_SIZE)
    {
        lx = 0;
        while (lx < CHUNK_SIZE)
        {
            if (game->obj_count >= MAX_OBJECTS - 1)
                return ;
            wx = chunk->cx * CHUNK_SIZE + lx;
            wy = chunk->cy * CHUNK_SIZE + ly;
            if (chunk->tiles[ly][lx] == TILE_WALL) { lx++; continue ; }
            biome = world_get_biome(&game->world, wx, wy);
            hash  = ((wx * 31337) ^ (wy * 7919)) & 0x7FFFFFFF;
            hash  = hash % 100;
            if ((biome == 1 && hash < 4) || (biome == 0 && hash < 2))
            {
                px = (float)wx + 0.5f;
                py = (float)wy + 0.5f;
                dx = px - game->player.x;
                dy = py - game->player.y;
                if (dx * dx + dy * dy > 4.0f)
                {
                    game->objects[game->obj_count].x         = px;
                    game->objects[game->obj_count].y         = py;
                    game->objects[game->obj_count].def_index = OBJ_ROCK;
                    game->objects[game->obj_count].dist      = 0.0f;
                    game->obj_count++;
                }
            }
            lx++;
        }
        ly++;
    }
}

static int find_chunk(t_world *world, int cx, int cy)
{
    return (ht_find(world, cx, cy));
}

static int evict_chunk(t_world *world)
{
    t_game  *game;
    int     pcx;
    int     pcy;
    int     best;
    int     max_d;
    int     d;
    int     i;

    if (!world->game_ptr) return (0);
    game  = (t_game *)world->game_ptr;
    pcx   = (int)game->player.x / CHUNK_SIZE;
    pcy   = (int)game->player.y / CHUNK_SIZE;
    best  = 0; max_d = -1; i = 0;
    while (i < MAX_CHUNKS)
    {
        if (world->chunks[i].loaded)
        {
            d = abs(world->chunks[i].cx - pcx)
              + abs(world->chunks[i].cy - pcy);
            if (d > max_d) { max_d = d; best = i; }
        }
        i++;
    }
    return (best);
}

static int load_chunk_tiles(t_world *world, int cx, int cy)
{
    t_chunk *chunk;
    int     slot;
    int     i;

    slot = -1; i = 0;
    while (i < MAX_CHUNKS)
    {
        if (!world->chunks[i].loaded) { slot = i; break ; }
        i++;
    }
    if (slot < 0)
    {
        slot = evict_chunk(world);
        ht_remove(world, slot);
    }
    chunk              = &world->chunks[slot];
    memset(chunk, 0, sizeof(t_chunk));
    chunk->cx          = cx;
    chunk->cy          = cy;
    chunk->loaded      = 1;
    chunk->tiles_ready = 0;
    chunk->objs_ready  = 0;
    chunk->nb_extra    = 0;
    gen_chunk_tiles(world, chunk);
    ht_insert(world, slot);
    return (slot);
}

char world_get_tile(t_world *world, int wx, int wy)
{
    int cx;
    int cy;
    int lx;
    int ly;
    int idx;

    cx  = (wx >= 0) ? wx / CHUNK_SIZE : (wx - CHUNK_SIZE + 1) / CHUNK_SIZE;
    cy  = (wy >= 0) ? wy / CHUNK_SIZE : (wy - CHUNK_SIZE + 1) / CHUNK_SIZE;
    lx  = wx - cx * CHUNK_SIZE;
    ly  = wy - cy * CHUNK_SIZE;
    idx = find_chunk(world, cx, cy);
    if (idx < 0) idx = load_chunk_tiles(world, cx, cy);
    if (lx < 0 || lx >= CHUNK_SIZE || ly < 0 || ly >= CHUNK_SIZE)
        return (TILE_EMPTY);
    return (world->chunks[idx].tiles[ly][lx]);
}

void world_update(t_game *game)
{
    int pcx;
    int pcy;
    int cx;
    int cy;
    int i;

    pcx = (int)game->player.x / CHUNK_SIZE;
    pcy = (int)game->player.y / CHUNK_SIZE;
    i = 0;
    while (i < MAX_CHUNKS)
    {
        if (game->world.chunks[i].loaded)
        {
            if (abs(game->world.chunks[i].cx - pcx) > RENDER_DIST + 1
                || abs(game->world.chunks[i].cy - pcy) > RENDER_DIST + 1)
            {
                ht_remove(&game->world, i);
                game->world.chunks[i].loaded      = 0;
                game->world.chunks[i].tiles_ready = 0;
                game->world.chunks[i].objs_ready  = 0;
                game->world.chunks[i].nb_extra    = 0;
            }
        }
        i++;
    }
    cy = pcy - RENDER_DIST;
    while (cy <= pcy + RENDER_DIST)
    {
        cx = pcx - RENDER_DIST;
        while (cx <= pcx + RENDER_DIST)
        {
            if (find_chunk(&game->world, cx, cy) < 0)
                load_chunk_tiles(&game->world, cx, cy);
            cx++;
        }
        cy++;
    }
    if (pcx == game->world.last_pcx && pcy == game->world.last_pcy)
        return ;
    game->world.last_pcx = pcx;
    game->world.last_pcy = pcy;
    game->obj_count = 0;
    i = 0;
    while (i < MAX_CHUNKS && game->obj_count < MAX_OBJECTS - 1)
    {
        if (game->world.chunks[i].loaded && game->world.chunks[i].tiles_ready)
            gen_chunk_objects(game, &game->world.chunks[i]);
        i++;
    }
}
