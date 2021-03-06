/*
 * Copyright (c) 2014 Jeff Boody
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <math.h>
#include "loax/loax_client.h"
#include "loax/loax_gl2.h"
#include "loax/gl2.h"
#include "a3d/a3d_shader.h"
#include "a3d/math/a3d_mat4f.h"
#include "libstl/stl_model.h"

#define LOG_TAG "stlview"
#include "loax/loax_log.h"

// flat shading - each normal across polygon is constant
// per-vertex normal + uniform color + uniform mvp
static const char* VSHADER =
	"attribute vec3 vertex;\n"
	"attribute vec3 normal;\n"
	"uniform   vec4 color;\n"
	"uniform   mat3 nm;\n"
	"uniform   mat4 mvp;\n"
	"varying   vec4 varying_color;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec4 ambient        = vec4(0.2, 0.2, 0.2, 1.0);\n"
	"	vec3 light_position = vec3(5.0, 5.0, 10.0);\n"
	"	light_position      = normalize(light_position);\n"   // TODO - optimize
	"	vec3 nm_normal      = normalize(nm * normal);\n"
	"	\n"
	"	float ndotlp = dot(nm_normal, light_position);\n"
	"	if(ndotlp > 0.0)\n"
	"	{\n"
	"		vec4 diffuse  = 0.8*vec4(ndotlp, ndotlp, ndotlp, 0.0);\n"
	"		varying_color = color * (ambient + diffuse);\n"
	"	}\n"
	"	else\n"
	"	{\n"
	"		varying_color = color * ambient;\n"
	"	}\n"
	"	gl_Position = mvp * vec4(vertex, 1.0);\n"
	"}\n";

static const char* FSHADER =
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"precision mediump int;\n"
	"#endif\n"
	"\n"
	"varying vec4 varying_color;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = varying_color;\n"
	"}\n";

// camera
float g_rfactor = 5.0f;   // radius scaling factor
float g_sfactor = 5.0f;   // stereo disperity in degrees
float g_theta   = 180.0f;
float g_phi     = 0.0f;

// geometry
GLsizei g_ec;
GLuint  g_vid;
GLuint  g_nid;

// shader state
GLuint g_program;
GLint  g_attribute_vertex;
GLint  g_attribute_normal;
GLint  g_uniform_color;
GLint  g_uniform_nm;
GLint  g_uniform_mvp;

static void draw(int w, int h, float s, stl_model_t* model)
{
	// transforms
	a3d_mat4f_t pm;
	a3d_mat4f_t mvm;
	a3d_mat4f_t mvp;
	a3d_mat3f_t nm;
	if(h > w)
	{
		GLfloat a = (GLfloat) h / (GLfloat) w;
		a3d_mat4f_frustum(&pm, 1, -1.0f, 1.0f, -a, a,
		                  0.1f*model->radius, 100.0f*model->radius);
	}
	else
	{
		GLfloat a = (GLfloat) w / (GLfloat) h;
		a3d_mat4f_frustum(&pm, 1, -a, a, -1.0f, 1.0f,
		                  0.1f*model->radius, 100.0f*model->radius);
	}

	float r     = g_rfactor*model->radius;
	float theta = (g_theta + s*g_sfactor)*M_PI/180.0f;
	float phi   = g_phi*M_PI/180.0f;
	float dx    = r*cosf(theta)*cosf(phi);
	float dy    = r*sinf(theta)*cosf(phi);
	float dz    = r*sinf(phi);
	a3d_mat4f_lookat(&mvm, 1,
	                 model->center.x + dx,
	                 model->center.y + dy,
	                 model->center.z + dz,
	                 model->center.x,
	                 model->center.y,
	                 model->center.z,
	                 0.0f, 0.0f, 1.0f);
	a3d_mat4f_mulm_copy(&pm, &mvm, &mvp);
	a3d_mat4f_normalmatrix(&mvm, &nm);

	// draw stl model
	a3d_vec4f_t color = { 1.0f, 1.0f, 1.0f, 1.0f };
	glUseProgram(g_program);
	glEnableVertexAttribArray(g_attribute_vertex);
	glEnableVertexAttribArray(g_attribute_normal);
	glUniform4fv(g_uniform_color, 1, (GLfloat*) &color);
	glUniformMatrix3fv(g_uniform_nm, 1, GL_FALSE, (GLfloat*) &nm);
	glUniformMatrix4fv(g_uniform_mvp, 1, GL_FALSE, (GLfloat*) &mvp);
	glBindBuffer(GL_ARRAY_BUFFER, g_vid);
	glVertexAttribPointer(g_attribute_vertex, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, g_nid);
	glVertexAttribPointer(g_attribute_normal, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, g_ec);
	glDisableVertexAttribArray(g_attribute_normal);
	glDisableVertexAttribArray(g_attribute_vertex);
}

int main(int argc, char** argv)
{
	int w = 0;
	int h = 0;

	if(argc != 2)
	{
		LOGE("usage: %s [file.stl]", argv[0]);
		return EXIT_FAILURE;
	}

	stl_model_t* model = stl_model_import(argv[1]);
	if(model == NULL)
	{
		return EXIT_FAILURE;
	}

	loax_client_t* c = NULL;
	while(1)
	{
		c = loax_client_new();
		if(c == NULL)
		{
			goto fail_client;
		}

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);

		// create vbo
		g_ec = 3*model->count;
		glGenBuffers(1, &g_vid);
		glGenBuffers(1, &g_nid);
		glBindBuffer(GL_ARRAY_BUFFER, g_vid);
		glBufferData(GL_ARRAY_BUFFER, g_ec*sizeof(stl_vec3f_t), model->vertices, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, g_nid);
		glBufferData(GL_ARRAY_BUFFER, g_ec*sizeof(stl_vec3f_t), model->normals, GL_STATIC_DRAW);

		// create shaders
		g_program = a3d_shader_make_source(VSHADER, FSHADER);
		if(g_program == 0)
		{
			goto fail_program;
		}

		// get attributes
		g_attribute_vertex = glGetAttribLocation(g_program, "vertex");
		g_attribute_normal = glGetAttribLocation(g_program, "normal");
		g_uniform_color    = glGetUniformLocation(g_program, "color");
		g_uniform_nm       = glGetUniformLocation(g_program, "nm");
		g_uniform_mvp      = glGetUniformLocation(g_program, "mvp");

		int stereo = 1;
		do
		{
			loax_event_t e;
			while(loax_client_poll(c, &e))
			{
				if(e.type == LOAX_EVENT_KEYUP)
				{
					if(e.event_key.keycode == 't')
					{
						stereo = 1 - stereo;
					}
					else if(e.event_key.keycode == 'a')
					{
						g_theta -= 5.0f;
						if(g_theta < 0.0f)
						{
							g_theta += 360.0f;
						}
					}
					else if(e.event_key.keycode == 'd')
					{
						g_theta += 5.0f;
						if(g_theta >= 360.0f)
						{
							g_theta -= 360.0f;
						}
					}
					else if(e.event_key.keycode == 'w')
					{
						g_phi += 5.0f;
						if(g_phi > 85.0f)
						{
							g_phi = 85.0f;
						}
					}
					else if(e.event_key.keycode == 's')
					{
						g_phi -= 5.0f;
						if(g_phi < -85.0f)
						{
							g_phi = -85.0f;
						}
					}
					else if(e.event_key.keycode == 'j')
					{
						g_rfactor -= 0.5f;
						if(g_rfactor < -0.5f)
						{
							g_rfactor = 0.5f;
						}
					}
					else if(e.event_key.keycode == 'k')
					{
						g_rfactor += 0.5f;
						if(g_rfactor > 10.0f)
						{
							g_rfactor = 10.0f;
						}
					}
					else if(e.event_key.keycode == '[')
					{
						g_sfactor -= 0.5f;
						if(g_sfactor < -0.5f)
						{
							g_sfactor = 0.5f;
						}
					}
					else if(e.event_key.keycode == ']')
					{
						g_sfactor += 0.5f;
						if(g_sfactor > 10.0f)
						{
							g_sfactor = 10.0f;
						}
					}
				}
			}

			loax_client_size(c, &w, &h);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			if(stereo)
			{
				glEnable(GL_SCISSOR_TEST);

				// draw left
				int w2 = w/2;
				glViewport(0, 0, w2, h);
				glScissor(0, 0, w2, h);
				draw(w2, h, -1.0f, model);

				// draw right
				glViewport(w2, 0, w2, h);
				glScissor(w2, 0, w2, h);
				draw(w/2, h, 1.0f, model);

				glDisable(GL_SCISSOR_TEST);
			}
			else
			{
				glViewport(0, 0, w, h);
				draw(w, h, 0.0f, model);
			}
		} while(loax_client_swapbuffers(c));

		glDeleteProgram(g_program);
		glDeleteBuffers(1, &g_nid);
		glDeleteBuffers(1, &g_vid);
		loax_client_delete(&c);
	}

	// failure
	fail_program:
		glDeleteBuffers(1, &g_nid);
		glDeleteBuffers(1, &g_vid);
		loax_client_delete(&c);
	fail_client:
		stl_model_delete(&model);
	return EXIT_FAILURE;
}
