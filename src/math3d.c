#include "cub3d.h"

/* ============================================================
** Vecteurs 3D
** ============================================================ */
t_vec3 vec3_sub(t_vec3 a, t_vec3 b)
{
    t_vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return (r);
}

float vec3_dot(t_vec3 a, t_vec3 b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

t_vec3 vec3_cross(t_vec3 a, t_vec3 b)
{
    t_vec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return (r);
}

t_vec3 vec3_norm(t_vec3 v)
{
    float   len;
    t_vec3  r;

    len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-8f)
        return ((t_vec3){0, 0, 1});
    r.x = v.x / len;
    r.y = v.y / len;
    r.z = v.z / len;
    return (r);
}

/* ============================================================
** Matrices 4x4  (row-major: m[row][col])
** ============================================================ */
t_mat4 mat4_identity(void)
{
    t_mat4 r;
    memset(&r, 0, sizeof(r));
    r.m[0][0] = 1.0f;
    r.m[1][1] = 1.0f;
    r.m[2][2] = 1.0f;
    r.m[3][3] = 1.0f;
    return (r);
}

t_mat4 mat4_mul(t_mat4 a, t_mat4 b)
{
    t_mat4  r;
    int     i;
    int     j;
    int     k;

    memset(&r, 0, sizeof(r));
    i = 0;
    while (i < 4)
    {
        j = 0;
        while (j < 4)
        {
            k = 0;
            while (k < 4)
            {
                r.m[i][j] += a.m[i][k] * b.m[k][j];
                k++;
            }
            j++;
        }
        i++;
    }
    return (r);
}

t_vec4 mat4_mul_vec4(t_mat4 m, t_vec4 v)
{
    t_vec4 r;
    r.x = m.m[0][0]*v.x + m.m[0][1]*v.y + m.m[0][2]*v.z + m.m[0][3]*v.w;
    r.y = m.m[1][0]*v.x + m.m[1][1]*v.y + m.m[1][2]*v.z + m.m[1][3]*v.w;
    r.z = m.m[2][0]*v.x + m.m[2][1]*v.y + m.m[2][2]*v.z + m.m[2][3]*v.w;
    r.w = m.m[3][0]*v.x + m.m[3][1]*v.y + m.m[3][2]*v.z + m.m[3][3]*v.w;
    return (r);
}

/*
** Matrice de projection perspective standard OpenGL-style
** fov_rad : champ de vision vertical en radians
** aspect   : screen_w / screen_h
** near/far : plans de clipping
*/
t_mat4 mat4_perspective(float fov_rad, float aspect, float near, float far)
{
    t_mat4  r;
    float   f;
    float   range;

    memset(&r, 0, sizeof(r));
    f     = 1.0f / tanf(fov_rad * 0.5f);
    range = far - near;
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = -(far + near) / range;
    r.m[2][3] = -(2.0f * far * near) / range;
    r.m[3][2] = -1.0f;
    return (r);
}

/*
** LookAt : construit la matrice vue à partir de la position caméra,
** du point regardé, et du vecteur up.
*/
t_mat4 mat4_look_at(t_vec3 eye, t_vec3 center, t_vec3 up)
{
    t_vec3  f;
    t_vec3  s;
    t_vec3  u;
    t_mat4  r;

    f = vec3_norm(vec3_sub(center, eye));
    s = vec3_norm(vec3_cross(f, up));
    u = vec3_cross(s, f);
    memset(&r, 0, sizeof(r));
    r.m[0][0] =  s.x;  r.m[0][1] =  s.y;  r.m[0][2] =  s.z;
    r.m[1][0] =  u.x;  r.m[1][1] =  u.y;  r.m[1][2] =  u.z;
    r.m[2][0] = -f.x;  r.m[2][1] = -f.y;  r.m[2][2] = -f.z;
    r.m[0][3] = -vec3_dot(s, eye);
    r.m[1][3] = -vec3_dot(u, eye);
    r.m[2][3] =  vec3_dot(f, eye);
    r.m[3][3] =  1.0f;
    return (r);
}
