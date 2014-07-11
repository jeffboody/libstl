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
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "stl_model.h"

#define LOG_TAG "stl"
#include "stl_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

static float sqdist(stl_vec3f_t* a, stl_vec3f_t* b)
{
	assert(a);
	assert(b);
	LOGD("debug");

	float dx = b->x - a->x;
	float dy = b->y - a->y;
	float dz = b->z - a->z;
	return dx*dx + dy*dy + dz*dz;
}

/***********************************************************
* public                                                   *
***********************************************************/

stl_model_t* stl_model_import(const char* fname)
{
	assert(fname);
	LOGD("debug fname=%s", fname);

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		LOGE("fopen %s failed", fname);
		return NULL;
	}

	stl_model_t* self = (stl_model_t*) malloc(sizeof(stl_model_t));
	if(self == NULL)
	{
		LOGE("malloc failed");
		goto fail_malloc;
	}

	// read header
	char header[80];
	if(fread(&header, sizeof(header), 1, f) != 1)
	{
		LOGE("invalid header");
		goto fail_header;
	}

	// read count
	unsigned int count;
	if(fread(&count, sizeof(unsigned int), 1, f) != 1)
	{
		LOGE("invalid count");
		goto fail_count;
	}
	self->count = count;

	// allocate normals
	self->normals = (stl_vec3f_t*) malloc(3*count*sizeof(stl_vec3f_t));
	if(self->normals == NULL)
	{
		LOGE("invalid normals");
		goto fail_normals;
	}

	// allocate vertices
	self->vertices = (stl_vec3f_t*) malloc(3*count*sizeof(stl_vec3f_t));
	if(self->vertices == NULL)
	{
		LOGE("invalid vertices");
		goto fail_vertices;
	}

	self->radius   = 0.0f;
	self->center.x = 0.0f;
	self->center.y = 0.0f;
	self->center.z = 0.0f;

	// read normals and vertices
	float minx = 1000.0f;
	float miny = 1000.0f;
	float minz = 1000.0f;
	float maxx = -1000.0f;
	float maxy = -1000.0f;
	float maxz = -1000.0f;
	int   i;
	for(i = 0; i < count; ++i)
	{
		float nx;
		float ny;
		float nz;

		if(fread(&nx, sizeof(float), 1, f) != 1)
		{
			LOGE("invalid nx");
			goto fail_fread;
		}

		if(fread(&ny, sizeof(float), 1, f) != 1)
		{
			LOGE("invalid ny");
			goto fail_fread;
		}

		if(fread(&nz, sizeof(float), 1, f) != 1)
		{
			LOGE("invalid nz");
			goto fail_fread;
		}

		int j;
		for(j = 0; j < 3; ++j)
		{
			float vx;
			float vy;
			float vz;

			if(fread(&vx, sizeof(float), 1, f) != 1)
			{
				LOGE("invalid vx");
				goto fail_fread;
			}

			if(fread(&vy, sizeof(float), 1, f) != 1)
			{
				LOGE("invalid vy");
				goto fail_fread;
			}

			if(fread(&vz, sizeof(float), 1, f) != 1)
			{
				LOGE("invalid vz");
				goto fail_fread;
			}

			// store normals and vertices
			int idx = 3*i + j;
			self->normals[idx].x  = nx;
			self->normals[idx].y  = ny;
			self->normals[idx].z  = nz;
			self->vertices[idx].x = vx;
			self->vertices[idx].y = vy;
			self->vertices[idx].z = vz;

			// determine bounding box
			if(vx < minx)
			{
				minx = vx;
			}
			if(vy < miny)
			{
				miny = vy;
			}
			if(vz < minz)
			{
				minz = vz;
			}
			if(vx > maxx)
			{
				maxx = vx;
			}
			if(vy > maxy)
			{
				maxy = vy;
			}
			if(vz > maxz)
			{
				maxz = vz;
			}
		}

		// read "null" attrib
		short attrib = 0;
		if((fread(&attrib, sizeof(short), 1, f) != 1) ||
		   (attrib != 0))
		{
			LOGE("invalid attrib");
			goto fail_fread;
		}
	}

	// compute center
	self->center.x = minx + (maxx - minx)/2.0f;
	self->center.y = miny + (maxy - miny)/2.0f;
	self->center.z = minz + (maxz - minz)/2.0f;

	// compute radius
	float rsq = 0.0f;
	for(i = 0; i < count; ++i)
	{
		int j;
		for(j = 0; j < 3; ++j)
		{
			int   idx = 3*i + j;
			float d   = sqdist(&self->center, &self->vertices[idx]);
			if(d > rsq)
			{
				rsq = d;
			}
		}
	}
	self->radius = sqrtf(rsq);

	fclose(f);

	// success
	return self;

	// failure
	fail_fread:
		free(self->vertices);
	fail_vertices:
		free(self->normals);
	fail_normals:
	fail_count:
	fail_header:
		free(self);
	fail_malloc:
		fclose(f);
	return NULL;
}

void stl_model_delete(stl_model_t** _self)
{
	assert(_self);

	stl_model_t* self = *_self;
	if(self)
	{
		LOGD("debug");

		free(self->vertices);
		free(self->normals);
		free(self);
		*_self = NULL;
	}
}
