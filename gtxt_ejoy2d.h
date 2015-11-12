#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_ejoy2d_h
#define gametext_ejoy2d_h

#ifndef USED_IN_EDITOR
#include <shader.h>
#include <render.h>
#include <renderbuffer.h>
#endif // USED_IN_EDITOR

#ifndef USED_IN_EDITOR

// "renderbuffer.h"
#define ej_vertex_pack				vertex_pack

// "shader.h"
#define ej_shader_texture			shader_texture
#define ej_shader_program			shader_program
#define ej_shader_draw				shader_draw
#define ej_shader_reset				shader_reset

#endif // USED_IN_EDITOR

#endif // gametext_ejoy2d_h

#ifdef __cplusplus
}
#endif