/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
All rights reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "pong.h"
#include "RaspiTex.h"
#include "RaspiTexUtil.h"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "RaspiImv.h"
#include "GraphicsStub.h"

#define WITH_TWO_TEXTURES 1

static RASPITEXUTIL_SHADER_PROGRAM_T pong_shader = {
    .vertex_source =
#if 0
    "attribute vec2 vertex;\n"
    "varying vec2 texcoord;\n"
    "void main(void) {\n"
    "   texcoord = 0.5 * (1.0 - vertex );\n"
    "   gl_Position = vec4(vertex, 0.0, 1.0);\n"
    "}\n",
#else
		"file:shader/gl_scenes/pong1.vert.glsl",
#endif
    .fragment_source =
#if 0
    "#extension GL_OES_EGL_image_external : require\n"
    "uniform samplerExternalOES tex;\n"
		"uniform sampler2D guitex;\n"
		"uniform sampler2D blobstex;\n"
    "varying vec2 texcoord;\n"
    "void main(void) {\n"
		"    vec4 gui = texture2D(guitex, texcoord);\n"
		"    vec4 blob = texture2D(blobstex, -texcoord);\n"
		"    gl_FragColor.rgb = mix(texture2D(tex, texcoord).rgb, gui.rgb, gui.a );\n"
		"    gl_FragColor.rgb = mix(gl_FragColor.rgb, blob.rgb, blob.a );\n"
		"    gl_FragColor.a = 1.0;\n"
    "}\n",
#else
		"file:shader/gl_scenes/pong1.frag.glsl",
#endif
    .uniform_names = {"tex", "guitex", "blobstex"},
    .attribute_names = {"vertex"},
};

static RASPITEXUTIL_SHADER_PROGRAM_T pong_shader_sobel = {
    .vertex_source = "file:shader/gl_scenes/pong2.vert.glsl",
    .fragment_source = "file:shader/gl_scenes/pong2.frag.glsl",
    .uniform_names = {"tex", "guitex", "blobstex", "texelsize"},
    .attribute_names = {"vertex"},
};

static RASPITEXUTIL_SHADER_PROGRAM_T pong_shader_hsv = {
    .vertex_source = "file:shader/gl_scenes/pong3.vert.glsl",
    .fragment_source = "file:shader/gl_scenes/pong3.frag.glsl",
    .uniform_names = {"tex", "guitex", "blobstex"},
    .attribute_names = {"vertex"},
};
static RASPITEXUTIL_SHADER_PROGRAM_T pong_shader_mirror = {
    .vertex_source = "file:shader/gl_scenes/pong4.vert.glsl",
    .fragment_source = "file:shader/gl_scenes/pong4.frag.glsl",
    .uniform_names = {"tex", "guitex", "blobstex", "offset"},
    .attribute_names = {"vertex"},
};

//static RASPITEXUTIL_TEXTURE_T cameraBuffer; //for fancy output effects, unused
RASPITEXUTIL_TEXTURE_T guiBuffer; //connected with GfxTexture in C++ class
RASPITEXUTIL_TEXTURE_T blobsBuffer; //connected with GfxTexture in C++ class
static unsigned char render_into_framebuffer = 0;
static uint32_t GScreenWidth;
static uint32_t GScreenHeight;

static const EGLint attribute_list[] =
{
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_NONE
};

static GLfloat _varray[] = {
	-1.0f, -1.0f,
	1.0f,  1.0f,
	1.0f, -1.0f,

	-1.0f,  1.0f,
	1.0f,  1.0f,
	-1.0f, -1.0f,
};

static GLfloat varray[] = {
	-1.0f, -1.0f,
	 1.0f, -1.0f,
	 1.0f,  1.0f,

	-1.0f,  1.0f,
	-1.0f, -1.0f,
	 1.0f,  1.0f,
};

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int pong_init(RASPITEX_STATE *state)
{
    int rc = raspitexutil_gl_init_2_0(state);
    if (rc != 0)
       goto end;

		//gl window size could differ...
		//graphics_get_display_size(0 /* LCD */, &GScreenWidth, &GScreenHeight);
		GScreenWidth = state->width;
		GScreenHeight = state->height;

		//Call initialisation functions in c++ part (Graphics.cpp)
		InitTextures(GScreenWidth, GScreenHeight);
		InitShaders();

    rc = raspitexutil_build_shader_program(&pong_shader);
    if (rc != 0)
       goto end;

    rc = raspitexutil_build_shader_program(&pong_shader_sobel);
    if (rc != 0)
       goto end;

    rc = raspitexutil_build_shader_program(&pong_shader_hsv);
    if (rc != 0)
       goto end;

    rc = raspitexutil_build_shader_program(&pong_shader_mirror);

		//cameraBuffer = raspitexutil_create_texture_rgba(GScreenWidth, GScreenHeight, 0, NULL);
		//raspitexutil_create_framebuffer(&cameraBuffer);
end:
    return rc;
}

/* Normal/Unfiltered output of video texture */
static void pong_video1(RASPITEX_STATE *raspitex_state) {

	GLCHK(glUseProgram(pong_shader.program));

	GLCHK(glUniform1i(pong_shader.uniform_locations[0], 0));
	GLCHK(glUniform1i(pong_shader.uniform_locations[1], 1));
	GLCHK(glUniform1i(pong_shader.uniform_locations[2], 2));

	// Bind video texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->texture);

	// Bind score texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, guiBuffer.id);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, blobsBuffer.id);

	GLCHK(glEnableVertexAttribArray(pong_shader.attribute_locations[0]));

	GLCHK(glVertexAttribPointer(pong_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, varray));
	GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

	GLCHK(glDisableVertexAttribArray(pong_shader.attribute_locations[0]));
	GLCHK(glBindTexture(GL_TEXTURE_2D, 0));
	glActiveTexture(GL_TEXTURE0);

}

/* Sobel output of video texture */
static void pong_video2(RASPITEX_STATE *raspitex_state) {

	GLCHK(glUseProgram(pong_shader_sobel.program));

	GLCHK(glUniform1i(pong_shader_sobel.uniform_locations[0], 0));
	GLCHK(glUniform1i(pong_shader_sobel.uniform_locations[1], 1));
	GLCHK(glUniform1i(pong_shader_sobel.uniform_locations[2], 2));

	glUniform2f(pong_shader_sobel.uniform_locations[3],
			1.f/raspitex_state->width, 1.f/raspitex_state->height );

	// Bind video texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->texture);

	// Bind score texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, guiBuffer.id);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, blobsBuffer.id);

	GLCHK(glEnableVertexAttribArray(pong_shader_sobel.attribute_locations[0]));

	GLCHK(glVertexAttribPointer(pong_shader_sobel.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, varray));
	GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

	GLCHK(glDisableVertexAttribArray(pong_shader_sobel.attribute_locations[0]));
	GLCHK(glBindTexture(GL_TEXTURE_2D, 0));
	glActiveTexture(GL_TEXTURE0);

}

/* HSV conversion of rgb image */
static void pong_video3(RASPITEX_STATE *raspitex_state) {

	GLCHK(glUseProgram(pong_shader_hsv.program));

	GLCHK(glUniform1i(pong_shader_hsv.uniform_locations[0], 0));
	GLCHK(glUniform1i(pong_shader_hsv.uniform_locations[1], 1));
	GLCHK(glUniform1i(pong_shader_hsv.uniform_locations[2], 2));

	// Bind video texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->texture);

	// Bind score texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, guiBuffer.id);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, blobsBuffer.id);

	GLCHK(glEnableVertexAttribArray(pong_shader_hsv.attribute_locations[0]));

	GLCHK(glVertexAttribPointer(pong_shader_hsv.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, varray));
	GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

	GLCHK(glDisableVertexAttribArray(pong_shader_hsv.attribute_locations[0]));
	GLCHK(glBindTexture(GL_TEXTURE_2D, 0));
	glActiveTexture(GL_TEXTURE0);

}

/* Adaption of mirror scene shader */
static void pong_video4(RASPITEX_STATE *raspitex_state) {
	static float offset = 0.0;
	offset += 0.05;

	GLCHK(glUseProgram(pong_shader_mirror.program));

	GLCHK(glUniform1i(pong_shader_mirror.uniform_locations[0], 0));
	GLCHK(glUniform1i(pong_shader_mirror.uniform_locations[1], 1));
	GLCHK(glUniform1i(pong_shader_mirror.uniform_locations[2], 2));

	glUniform1f(pong_shader_mirror.uniform_locations[3], offset);

	// Bind video texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->texture);

	// Bind score texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, guiBuffer.id);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, blobsBuffer.id);

	GLCHK(glEnableVertexAttribArray(pong_shader_mirror.attribute_locations[0]));

	GLCHK(glVertexAttribPointer(pong_shader_mirror.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, varray));
	GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

	GLCHK(glDisableVertexAttribArray(pong_shader_mirror.attribute_locations[0]));
	GLCHK(glBindTexture(GL_TEXTURE_2D, 0));
	glActiveTexture(GL_TEXTURE0);

}


static int pong_redraw(RASPITEX_STATE *raspitex_state) {

    // Start with a clear screen
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		RedrawGui();

		int select = 0;
		// Ask pong object which shader should be used
		if( raspitex_state->ops.args[0] ){
			select = GetShader(raspitex_state->ops.args[0]);
		}
		// Shift by fixed offset.
		if( raspitex_state->ops.args[1] ){
			select = (select + raspitex_state->ops.args[1]) % SHADER_TYPE_NUM; 
		}
		
		switch( select ){
			case ShaderMirror:
				pong_video4(raspitex_state);
				break;
			case ShaderHSV:
				pong_video3(raspitex_state);
				break;
			case ShaderSobel:
				pong_video2(raspitex_state);
				break;
			case ShaderNormal:
			default:
				pong_video1(raspitex_state);
				break;
		}

		GLCHK(glEnable(GL_BLEND));
		GLCHK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
		RedrawTextures();
		GLCHK(glDisable(GL_BLEND));

#if 0
		static int counter_fb = 0;
		if( ++counter_fb == 400 ){
			printf("Saving…  "); fflush(stdout);
			raspitexutil_save_framebuffer("/dev/shm/fb.png");
			printf("Saved framebuffer in frame %i.\n", counter_fb-1);
		}
#endif

		return 0;
}

int pong_open(RASPITEX_STATE *state)
{
	//use own attribs list
	//state->egl_config_attribs = attribute_list ;

	state->ops.gl_init = pong_init;
	state->ops.redraw = pong_redraw;
	state->ops.update_texture = raspitexutil_update_texture;
	return 0;
}
