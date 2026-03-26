#include "cub3d.h"
#include <dirent.h>

/*
** Parse une ligne "cle=valeur" et retourne la valeur.
** Retourne NULL si la clé ne correspond pas.
*/
static char *parse_value(char *line, const char *key)
{
    int     klen;
    char    *eq;

    klen = strlen(key);
    if (strncmp(line, key, klen) != 0)
        return (NULL);
    eq = strchr(line, '=');
    if (!eq)
        return (NULL);
    eq++;
    /* Supprime le '\n' en fin */
    int len = strlen(eq);
    if (len > 0 && eq[len - 1] == '\n')
        eq[len - 1] = '\0';
    return (eq);
}

/*
** Charge un fichier .obj et remplit une t_obj_def.
** Retourne 0 si succès, -1 si erreur.
*/
static int load_obj_file(t_game *game, t_obj_def *def, const char *path)
{
    FILE    *f;
    char    buf[256];
    char    *val;
    char    tex_path[256];

    f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "Cannot open %s\n", path);
        return (-1);
    }

    /* Valeurs par défaut */
    memset(def, 0, sizeof(t_obj_def));
    def->width        = 1.0f;
    def->height_base  = 1.0f;
    def->height_extra = 0.0f;
    def->extra_width  = 0.0f;
    def->spawn_chance = 5;

    while (fgets(buf, sizeof(buf), f))
    {
        /* Ignore commentaires et lignes vides */
        if (buf[0] == '#' || buf[0] == '\n')
            continue ;

        if ((val = parse_value(buf, "name")))
            strncpy(def->name, val, sizeof(def->name) - 1);
        else if ((val = parse_value(buf, "texture_base")) && val[0])
        {
            snprintf(tex_path, sizeof(tex_path), "textures/%s", val);
            load_one_tex(&def->tex_base, tex_path);
        }
        else if ((val = parse_value(buf, "texture_extra")) && val[0])
        {
            snprintf(tex_path, sizeof(tex_path), "textures/%s", val);
            load_one_tex(&def->tex_extra, tex_path);
        }
        else if ((val = parse_value(buf, "width")))
            def->width = atof(val);
        else if ((val = parse_value(buf, "height_base")))
            def->height_base = atof(val);
        else if ((val = parse_value(buf, "height_extra")))
            def->height_extra = atof(val);
        else if ((val = parse_value(buf, "extra_width")))
            def->extra_width = atof(val);
        else if ((val = parse_value(buf, "spawn_chance")))
            def->spawn_chance = atoi(val);
    }
    fclose(f);
    (void)game;
    return (0);
}

/*
** Charge tous les fichiers .obj du dossier objects/.
** Remplit game->obj_defs[] et game->obj_def_count.
*/
int load_obj_defs(t_game *game)
{
    DIR             *dir;
    struct dirent   *entry;
    char            path[512];
    int             len;

    game->obj_def_count = 0;
    dir = opendir("objects");
    if (!dir)
    {
        fprintf(stderr, "Cannot open objects/ directory\n");
        return (-1);
    }
    while ((entry = readdir(dir)) != NULL
        && game->obj_def_count < MAX_OBJ_DEFS)
    {
        len = strlen(entry->d_name);
        /* Charge uniquement les fichiers .obj */
        if (len > 4 && strcmp(entry->d_name + len - 4, ".obj") == 0)
        {
            snprintf(path, sizeof(path), "objects/%s", entry->d_name);
            if (load_obj_file(game,
                &game->obj_defs[game->obj_def_count], path) == 0)
            {
                fprintf(stderr, "Loaded obj: %s\n",
                    game->obj_defs[game->obj_def_count].name);
                game->obj_def_count++;
            }
        }
    }
    closedir(dir);
    fprintf(stderr, "Total obj defs: %d\n", game->obj_def_count);
    return (0);
}

void free_obj_defs(t_game *game)
{
    int i;

    i = 0;
    while (i < game->obj_def_count)
    {
        if (game->obj_defs[i].tex_base.pixels)
        {
            free(game->obj_defs[i].tex_base.pixels);
            game->obj_defs[i].tex_base.pixels = NULL;
        }
        if (game->obj_defs[i].tex_extra.pixels)
        {
            free(game->obj_defs[i].tex_extra.pixels);
            game->obj_defs[i].tex_extra.pixels = NULL;
        }
        i++;
    }
}
