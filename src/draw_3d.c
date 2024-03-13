
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
    -0.8,  0.8, -0.8,
     0.8, -0.8,  0.8,
};

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
    1, 1, 1,
    1, 1, 1,
};

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


static const GLfloat g_axis_pos[] = {
    -1, 0, 0,
     1, 0, 0,
     0,-1, 0,
     0, 1, 0,
     0, 0,-1,
     0, 0, 1,
};

static const GLfloat g_axis_col[] = {
     1, 1, 1,
     1, 1, 1,
     1, 1, 1,
     1, 1, 1,
     1, 1, 1,
     1, 1, 1,
};


static bool g_initialized = false;

// Cube with VAO.
GLuint h_cube_vao;
GLuint h_cube_pos_vbo;
GLuint h_cube_col_vbo;
GLuint h_cube_idx_vbo;

GLuint h_hyp_vao;

// Axis.
GLuint h_axis_vao;
GLuint h_axis_pos_vbo;
GLuint h_axis_col_vbo;

// Shader programs.
GLuint h_vertex_shader;
GLuint h_fragment_shader;
GLuint h_shader_prog;

GLint h_model;
GLint h_view;
GLint h_projection;


// Vertex shader.
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

// Fragment shader.
const char* fragment_src =
    "#version 450 core\n"
    "in vec3 rgbcol;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "    out_color = vec4(rgbcol, 1.0);\n"
    "\n}";


static void
gen_shaders(void) {

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


void
draw_3d_init(pds_s *pds) {

    if ( true == g_initialized ) {
        return;
    }

    g_initialized = true;

    logfmt("OpenGL %s, GLSL %s\n"
        , glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    gen_shaders();

    // VAO tracks calls to:
    //   glVertexAttribPointer
    //   glEnableVertexAttribArray
    //   glDisableVertexAttribArray
    //   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ...) (can only be one of these in a VAO).

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
    // VBO binding, the GL_ELEMENT_ARRAY_BUFFER binding is captured in the VAO,
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


void
draw_3d_cleanup(pds_s *pds) {

    if ( false == g_initialized ) {
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


void
draw_3d(pds_s *pds) {

    draw_3d_init(pds);

    f32 aspect = (f32)(pds->disco.winpos.w) / (f32)(pds->disco.winpos.h);

    mat4 projection;
    glm_perspective(/*fovy*/ 50.0f, aspect, /*near-z*/ 0.1f, /*far-z*/ 100.0f, projection);
    //f32 orth = 2.0f;
    //glm_ortho(/*l*/ -orth*aspect, /*r*/ orth*aspect, /*b*/ -orth, /*t*/ orth, /*near*/ 0.0f, /*far*/ 100.0f, projection);

    mat4 view;
    glm_lookat((vec3){pds->camera.x,pds->camera.y,pds->camera.z}, (vec3){0,0,0}, (vec3){0,1,0}, view);

    versor rx; glm_quatv(rx, pds->model.xrot, (vec3){1,0,0});
    versor ry; glm_quatv(ry, pds->model.yrot, (vec3){0,1,0});
    versor rz; glm_quatv(rz, pds->model.zrot, (vec3){0,0,1});
    versor q;
    glm_quat_identity(q);
    glm_quat_mul(rz,q,q); // q = rz * q
    glm_quat_mul(rx,q,q); // q = rx * q
    glm_quat_mul(ry,q,q); // q = ry * q
    mat4 model;
    glm_quat_mat4(q, model);


    // Global state, does not need to be called every frame, but can be
    // called between draw calls if necessary, depending on models.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glUseProgram(h_shader_prog);

    glUniformMatrix4fv(h_model,      /*count*/ 1, /*transpose*/ GL_FALSE, model[0]);
    glUniformMatrix4fv(h_view,       /*count*/ 1, /*transpose*/ GL_FALSE, view[0]);
    glUniformMatrix4fv(h_projection, /*count*/ 1, /*transpose*/ GL_FALSE, projection[0]);

    //glPolygonMode(GL_FRONT, GL_LINE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBindVertexArray(h_cube_vao);
    GLsizei count = sizeof(g_cube_indexes) / sizeof(GLshort);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, /*offset*/ NULL);
    // Draw hypotenuse.
    glDrawArrays(GL_LINES, 8, 2);

    // Draw reference axis without any transformations.
    glm_mat4_identity(model);
    glUniformMatrix4fv(h_model, /*count*/ 1, /*transpose*/ GL_FALSE, model[0]);

    glBindVertexArray(h_axis_vao);
    count = sizeof(g_axis_pos) / sizeof(GLfloat);
    glDrawArrays(GL_LINES, /*first*/0, count);

}
// draw_3d()
