/**
 * Direct OpenGL 3D Drawing.
 *
 * @file   draw_3d.c
 * @author Matthew Hagerty
 * @date   March 12, 2024
 *
 * @ref https://paroj.github.io/gltut/
 * @ref https://www.khronos.org/opengl/wiki/Main_Page
 * @ref https://docs.gl/gl4/glBindVertexArray
 * @ref https://cglm.readthedocs.io/en/latest/affine.html
 * @ref https://learnopengl.com/
 * @ref https://www.opengl-tutorial.org/
 * @ref https://en.wikipedia.org/wiki/Active_and_passive_transformation
 *
 * Backlog:
 *   Add a shader loader rather than hard-coded strings.
 *   Add error checking to shader setup.
 *   Add a simple model database / scene graph to make drawing easier:
 *     points
 *     lines
 *     triangles
 *     textures
 *     lights
 *   Add an object loader (https://github.com/assimp/assimp)
 */


/*
This file could be used a nice starting point to learn OpenGL.  The environment
is all set up, and all OpenGL functions are available.  Just delete all the code
in the call-back functions (draw_3d_init, draw_3d_cleanup, draw_3d) and start
following along with your favorite OpenGL tutorial.

One of the most confusing aspects of learning OpenGL is the names given to its
concepts, like "glGenBuffers" that does not actually generate a buffer.

OpenGL has a concept of "bind points", which are basically global pointers that
are internal to OpenGL.  Various OpenGL functions use the global bind points
(pointers) to know what internal data buffer to operate on.  There are several
bind points, but only one of each kind, so only one object can be "bound" at any
one time.  The bind points for objects in OpenGL:

  GL_ARRAY_BUFFER               Vertex attributes
  GL_ATOMIC_COUNTER_BUFFER      Atomic counter storage
  GL_COPY_READ_BUFFER           Buffer copy source
  GL_COPY_WRITE_BUFFER          Buffer copy destination
  GL_DISPATCH_INDIRECT_BUFFER   Indirect compute dispatch commands
  GL_DRAW_INDIRECT_BUFFER       Indirect command arguments
  GL_ELEMENT_ARRAY_BUFFER       Vertex array indices
  GL_PIXEL_PACK_BUFFER          Pixel read target
  GL_PIXEL_UNPACK_BUFFER        Texture data source
  GL_QUERY_BUFFER               Query result buffer
  GL_SHADER_STORAGE_BUFFER      Read-write storage for shaders
  GL_TEXTURE_BUFFER             Texture data buffer
  GL_TRANSFORM_FEEDBACK_BUFFER  Transform feedback buffer
  GL_UNIFORM_BUFFER             Uniform block storage

Bind points might be thought of like this (internal to OpenGL):

  // Global array of bind points.
  gl_object *bind_points[NUM_BIND_POINTS];
  bind_points[GL_ARRAY_BUFFER] = NULL;

  glBindBuffer(GL_ARRAY_BUFFER, h_user_obj_1) {
      bind_points[GL_ARRAY_BUFFER] = get_ptr(h_user_obj_1);
  }


OpenGL also has internal state, which is also affected by various OpenGL
functions.  Knowing what functions affect the state is critical to using OpenGL
correctly and effectively.


Some better names for a few functions would go a long way toward making OpenGL
easier to understand and learn.  These are some alternate names for functions
with confusing names:

glGenBuffers -> glGenObject
  Used to create an object to manage OpenGL buffers and other data.  A handle
  to the newly created object is returned for use with other OpenGL functions.
  The handle is opaque to the host program.

glBindBuffer -> glSetGlobalObject
  Used to assign the specified object to an internal OpenGL bind point (global
  pointer) that is used as the target for other OpenGL functions.

glBufferData -> glAllocateCopy
  Allocates a buffer on the GPU for whatever object is currently set (bound)
  at the specific bing point, then copies the specified data to the buffer.
  Basically malloc() followed by memcpy().

glVertexAttribPointer -> glVertexAttribFormat
  Describes how the data in the currently bound buffer is formatted, and which
  vertex shader attribute will use the data.  Also specifies the offset of the
  data in the buffer.


Example, this:

  glGenBuffers(1, &pos_obj);
  glBindBuffer(GL_ARRAY_BUFFER, pos_obj);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

Becomes:

  glGenObject(1, &pos_obj);
  glSetGlobalObject(GL_ARRAY_BUFFER, pos_obj);
  glAllocateCopy(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
  glSetGlobalObject(GL_ARRAY_BUFFER, 0);


*/

#include <stdbool.h>    // true,false
#include "GL/gl3w.h"    // OpenGL
#include "xyz.h"        // short types
#include "log.h"        // logfmt
#include "cglm/cglm.h"  // cglm
#include "program.h"
#include "draw_3d.h"


// https://www.khronos.org/opengl/wiki/Vertex_Specification_Best_Practices
//
// The "normalized" intergers just seems to singed and unsigned 2's complement
// numbers that have their full range mapped to [-1,1] or [0,1].  There is
// also consideration given to max negative being larger than max positive, so
// max negative is clamped to one less (to match the positive max), i.e.
// for a signed byte [-127,127] instead of [-128,127]
//
// 4-bytes alignment
// positions: [-1,1] GLfloat
// colors: [0,1] GLubyte in rgba
// normals: [-1,1] normalized GLshort or GL_INT_2_10_10_10_REV
// 2D textures: normalized GLshort or GLushort


/// Cube vertex positions.
static const GLfloat g_cube_positions[] = {
    -0.5f, -0.5f, -0.5f, // 0 bot, lt, back
    -0.5f,  0.5f, -0.5f, // 1 top, lt, back
     0.5f, -0.5f, -0.5f, // 2 bot, rt, back
     0.5f,  0.5f, -0.5f, // 3 top, rt, back
     0.5f, -0.5f,  0.5f, // 4 bot, rt, front
     0.5f,  0.5f,  0.5f, // 5 top, rt, front
    -0.5f, -0.5f,  0.5f, // 6 bot, lt, front
    -0.5f,  0.5f,  0.5f, // 7 top, lt, front
    // hypotenuse
    -0.6,  0.6, -0.6,
     0.6, -0.6,  0.6,
};

/// Cube vertex colors.
static const GLfloat g_cube_colors[] = {
    1.0, 0.0, 0.0,
    0.5, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.5, 0.0,
    0.0, 0.0, 1.0,
    0.0, 0.0, 5.0,
    1.0, 0.0, 1.0,
    0.5, 0.0, 0.5,
    // hypotenuse
    1, 0.5, 0,
    1, 0.5, 0,
};

/// Cube triangle vertex-indexes into the vertex-position and color arrays.
static const GLushort g_cube_indexes[] = {
    0, 1, 2, // back 1
    2, 1, 3, // back 2
    2, 3, 4, // right 1
    4, 3, 5, // right 2
    4, 5, 6, // front 1
    6, 5, 7, // front 2
    6, 7, 0, // left 1
    0, 7, 1, // left 2
    6, 0, 2, // bottom 1
    2, 4, 6, // bottom 2
    7, 5, 3, // top 1
    7, 3, 1, // top 2
};


/// Reference axis vertex positions.
static const GLfloat g_axis_pos[] = {
    -1, 0, 0,
     1, 0, 0,
     0,-1, 0,
     0, 1, 0,
     0, 0,-1,
     0, 0, 1,
};

/// Reference axis colors.
static const GLfloat g_axis_col[] = {
     1, 1, 1,
     1, 1, 1,
     1, 1, 1,
     1, 1, 1,
     1, 1, 1,
     1, 1, 1,
};


/// Cube OpenGL object handles.
GLuint h_cube_vao;
GLuint h_cube_pos_vbo;
GLuint h_cube_col_vbo;
GLuint h_cube_idx_vbo;

/// Hypotenuse VAO object handle.
GLuint h_hyp_vao;


/// Axis OpenGL object handles.
GLuint h_axis_vao;
GLuint h_axis_pos_vbo;
GLuint h_axis_col_vbo;


/// Shader OpenGL object handles.
GLuint h_vertex_shader;
GLuint h_fragment_shader;
GLuint h_shader_prog;

/// Model, view, and projection uniform OpenGL object handles.
GLint h_model;
GLint h_view;
GLint h_projection;


/// Vertex shader source.
const char *vertex_src =
    "#version 450 core\n"
    "in vec3 position;\n"
    "in vec3 rgb;\n"
    "out vec3 rgbcol;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "void main() {\n"
    "    vec4 pt = vec4(position, 1.0);\n"
    "    gl_Position = projection * view * model * pt;\n"
    "    rgbcol = rgb;\n"
    "\n}";

/// Fragment shader source.
const char* fragment_src =
    "#version 450 core\n"
    "in vec3 rgbcol;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "    out_color = vec4(rgbcol, 1.0);\n"
    "\n}";


/**
 * Generate the OpenGL shaders.
 */
static void
gen_shaders(void) {

    // TODO check for errors, make more robust, include file loading.

    // Make a vertex and fragment shader.
    h_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(h_vertex_shader, 1, &vertex_src, /*length*/ NULL);
    glCompileShader(h_vertex_shader);

    h_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(h_fragment_shader, 1, &fragment_src, /*length*/ NULL);
    glCompileShader(h_fragment_shader);

    // Make a program.
    h_shader_prog = glCreateProgram();
    glAttachShader(h_shader_prog, h_vertex_shader);
    glAttachShader(h_shader_prog, h_fragment_shader);

    // TODO glBindFragDataLocation() and glDrawBuffers()

    // Shader objects can be flagged for deletion now with glDeleteShader().
    // They stick around in VRAM until glDetachShader() has been called on them.
    glDeleteShader(h_vertex_shader);
    glDeleteShader(h_fragment_shader);

    // Link the program.
    glLinkProgram(h_shader_prog);
}
// gen_shaders()


/**
 * Initialize the 3D drawing.
 *
 * @param[in] pds   Program data struct object.
 */
void
draw_3d_init(pds_s *pds) {

    if ( true == pds->draw3d_initialized ) {
        return;
    }

    pds->draw3d_initialized = true;

    logfmt("OpenGL %s, GLSL %s\n"
        , glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    // TODO error checking.
    gen_shaders();

    // VAO captures state and associations when these functions are called:
    //   glVertexAttribPointer(1)
    //   glEnableVertexAttribArray
    //   glDisableVertexAttribArray
    //   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ...) (can only be one of these in a VAO).
    //
    // (1) VAOs store which buffer objects are associated with which attributes
    //     at the time glVertexAttribPointer() is called, but they do NOT store
    //     the GL_ARRAY_BUFFER binding itself.

    // Generate a Vertex Array oject to store all the VBOs and attributes.
    glGenVertexArrays(1, &h_cube_vao);
    glBindVertexArray(h_cube_vao);

    // Generate a Vector Buffer Object handle and activate (bind) the VBO.
    glGenBuffers(1, &h_cube_pos_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, h_cube_pos_vbo);
    // Copy data from the CPU to the GPU active buffer.
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_cube_positions), g_cube_positions, GL_STATIC_DRAW);

    // Set up how the position data is formatted to the shader input attribute.
    GLint attr_loc = glGetAttribLocation(h_shader_prog, "position");
    // The VAO will capture which buffer is bound for the attribute association.
    glVertexAttribPointer(attr_loc, /*num vals*/ 3, /*type*/ GL_FLOAT, /*normalize*/ GL_FALSE, /*stride*/ 0, /*offset*/ 0);
    glEnableVertexAttribArray(attr_loc);

    // Colors.
    glGenBuffers(1, &h_cube_col_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, h_cube_col_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_cube_colors), g_cube_colors, GL_STATIC_DRAW);
    attr_loc = glGetAttribLocation(h_shader_prog, "rgb");
    glVertexAttribPointer(attr_loc, /*num vals*/ 3, /*type*/ GL_FLOAT, /*normalize*/ GL_FALSE, /*stride*/ 0, /*offset*/ 0);
    glEnableVertexAttribArray(attr_loc);

    // Create an index buffer and copy data to the GPU.  Unlike GL_ARRAY_BUFFER
    // VBO binding, the GL_ELEMENT_ARRAY_BUFFER binding is captured by the VAO,
    // and is required to use the glDrawElements commands.
    glGenBuffers(1, &h_cube_idx_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, h_cube_idx_vbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(g_cube_indexes), g_cube_indexes, GL_STATIC_DRAW);

    // Disable the VAO binding to prevent additional changes.
    glBindVertexArray(0);


    // Axis object.
    glGenVertexArrays(1, &h_axis_vao);
    glBindVertexArray(h_axis_vao);

    glGenBuffers(1, &h_axis_pos_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, h_axis_pos_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_axis_pos), g_axis_pos, GL_STATIC_DRAW);
    attr_loc = glGetAttribLocation(h_shader_prog, "position");
    glVertexAttribPointer(attr_loc, /*num vals*/ 3, /*type*/ GL_FLOAT, /*normalize*/ GL_FALSE, /*stride*/ 0, /*offset*/ 0);
    glEnableVertexAttribArray(attr_loc);

    glGenBuffers(1, &h_axis_col_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, h_axis_col_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_axis_col), g_axis_col, GL_STATIC_DRAW);
    attr_loc = glGetAttribLocation(h_shader_prog, "rgb");
    glVertexAttribPointer(attr_loc, /*num vals*/ 3, /*type*/ GL_FLOAT, /*normalize*/ GL_FALSE, /*stride*/ 0, /*offset*/ 0);
    glEnableVertexAttribArray(attr_loc);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    h_model      = glGetUniformLocation(h_shader_prog, "model");
    h_view       = glGetUniformLocation(h_shader_prog, "view");
    h_projection = glGetUniformLocation(h_shader_prog, "projection");

    pds->model.xrot = 0;
    pds->model.yrot = 0;
    pds->model.zrot = 0;

    pds->camera.x = 0;
    pds->camera.y = 0;
    pds->camera.z = 20;
}
// draw_3d_init()


/**
 * Cleanup the 3D drawing.
 *
 * @param[in] pds   Program data struct object.
 */
void
draw_3d_cleanup(pds_s *pds) {

    if ( false == pds->draw3d_initialized ) {
        return;
    }

    glDeleteProgram(h_shader_prog);
    glDeleteShader(h_fragment_shader);
    glDeleteShader(h_vertex_shader);

    glDeleteBuffers(1, &h_cube_pos_vbo);
    glDeleteBuffers(1, &h_cube_idx_vbo);

    glDeleteVertexArrays(1, &h_cube_vao);
}
// draw_3d_cleanup()


/**
 * 3D drawing.
 *
 * @param[in] pds   Program data struct object.
 */
void
draw_3d(pds_s *pds) {

    // Updated per frame to track window size changes.
    f32 aspect = (f32)(pds->disco.winpos.w) / (f32)(pds->disco.winpos.h);

    // TODO update projects to be UI selectable and calculate approximate
    //      view between projection and orthographic so switching from one
    //      to the other is not visually jarring.
    mat4 projection;
    //glm_perspective(/*fovy*/ 50.0f, aspect, /*near-z*/ 0.1f, /*far-z*/ 100.0f, projection);
    f32 orth = 2.0f;
    glm_ortho(/*l*/ -orth*aspect, /*r*/ orth*aspect, /*b*/ -orth, /*t*/ orth, /*near*/ 0.0f, /*far*/ 100.0f, projection);

    // TODO make camera a rotation rather than an xyz vector.
    // Camera position has UI inputs.
    mat4 view;
    glm_lookat((vec3){pds->camera.x,pds->camera.y,pds->camera.z}, (vec3){0,0,0}, (vec3){0,1,0}, view);

    // Matrix multiply is NOT commutative, order matters.

    mat4 model;
    glm_mat4_identity(model);
    // Move cube to its corner for rotation.
    //glm_translate_make(model, (vec3){-0.5,0.5,-0.5});

    // Use quaternions to simplify rotations and set up rotations
    // around a global axis that does not change with the object
    // being rotated (active rotation).
    versor rx; glm_quatv(rx, pds->model.xrot, (vec3){1,0,0});
    versor ry; glm_quatv(ry, pds->model.yrot, (vec3){0,1,0});
    versor rz; glm_quatv(rz, pds->model.zrot, (vec3){0,0,1});
    versor q;
    glm_quat_identity(q);
    glm_quat_mul(rz,q,q); // q = rz * q
    glm_quat_mul(rx,q,q); // q = rx * q
    glm_quat_mul(ry,q,q); // q = ry * q
    mat4 rot;
    glm_quat_mat4(q, rot);

    glm_mat4_mul(rot, model, model); // model = rot * model


    // Global state, does not need to be called every frame, but these
    // can be called between various draw calls depending what the
    // kind of rendering result is desired.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    //glPolygonMode(GL_FRONT, GL_LINE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(h_shader_prog);

    glUniformMatrix4fv(h_model,      /*count*/ 1, /*transpose*/ GL_FALSE, model[0]);
    glUniformMatrix4fv(h_view,       /*count*/ 1, /*transpose*/ GL_FALSE, view[0]);
    glUniformMatrix4fv(h_projection, /*count*/ 1, /*transpose*/ GL_FALSE, projection[0]);

    glBindVertexArray(h_cube_vao);
    GLsizei count = sizeof(g_cube_indexes) / sizeof(GLshort);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, /*offset*/ NULL);
    // Draw hypotenuse with the vertex-position data.  The offset and count
    // inputs to glDrawArrays() are element-size units, not bytes.
    glDrawArrays(GL_LINES, /*offset*/ 8, /*count*/ 2);

    // Draw reference axis without any transformations.
    glm_mat4_identity(model);
    glUniformMatrix4fv(h_model, /*count*/ 1, /*transpose*/ GL_FALSE, model[0]);

    glBindVertexArray(h_axis_vao);
    count = sizeof(g_axis_pos) / (sizeof(GLfloat) * 3);
    glDrawArrays(GL_LINES, /*first*/ 0, count);

    glBindVertexArray(0);
    glUseProgram(0);
}
// draw_3d()

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2024 Matthew Hagerty
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
