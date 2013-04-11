/** 
 * @file alphaF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
 
#extension GL_ARB_texture_rectangle : enable

#define INDEXED 1
#define NON_INDEXED 2
#define NON_INDEXED_NO_COLOR 3

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2DRect depthMap;

#if INDEX_MODE != INDEXED
uniform sampler2D diffuseMap;
#endif

#if INDEX_MODE == INDEXED
vec4 diffuseLookup(vec2 texcoord);
#endif

uniform vec2 screen_res;

vec3 atmosLighting(vec3 light);
vec3 scaleSoftClip(vec3 light);

VARYING vec3 vary_ambient;
VARYING vec3 vary_directional;
VARYING vec3 vary_fragcoord;
VARYING vec3 vary_position;
VARYING vec3 vary_pointlight_col;

#if INDEX_MODE != NON_INDEXED_NO_COLOR
VARYING vec4 vertex_color;
#endif

VARYING vec2 vary_texcoord0;

VARYING vec3 vary_norm;
uniform mat4 inv_proj;

uniform vec4 light_position[8];
uniform vec3 light_direction[8];
uniform vec3 light_attenuation[8]; 
uniform vec3 light_diffuse[8];


uniform float shadow_offset;

float calcDirectionalLight(vec3 n, vec3 l)
{
        float a = pow(max(dot(n,l),0.0), 0.7);
        return a;
}

float calcPointLightOrSpotLight(vec3 v, vec3 n, vec4 lp, vec3 ln, float la, float fa, float is_pointlight)
{
	//get light vector
	vec3 lv = lp.xyz-v;
	
	//get distance
	float d = dot(lv,lv);
	
	float da = 0.0;

	if (d > 0.0 && la > 0.0 && fa > 0.0)
	{
		//normalize light vector
		lv = normalize(lv);
	
		//distance attenuation
		float dist2 = d/la;
		da = clamp(1.0-(dist2-1.0*(1.0-fa))/fa, 0.0, 1.0);

		// spotlight coefficient.
		float spot = max(dot(-ln, lv), is_pointlight);
		da *= spot*spot; // GL_SPOT_EXPONENT=2

		//angular attenuation
		da *= max(pow(dot(n, lv), 0.7), 0.0);		
	}

	return da;	
}

void main() 
{
	vec2 frag = vary_fragcoord.xy/vary_fragcoord.z*0.5+0.5;
	frag *= screen_res;
	
	vec4 pos = vec4(vary_position, 1.0);
	
#if INDEX_MODE == INDEXED
	vec4 diff = diffuseLookup(vary_texcoord0.xy);
#else
	vec4 diff = texture2D(diffuseMap,vary_texcoord0.xy);
#endif

#if INDEX_MODE == NON_INDEXED_NO_COLOR
	float vertex_color_alpha = 1.0;
#else
	float vertex_color_alpha = vertex_color.a;
#endif

	vec3 dlight = calcDirectionalLight(vary_norm, light_position[0].xyz) * vary_directional.rgb * vary_pointlight_col;

	vec4 col = vec4(vary_ambient + dlight, vertex_color_alpha);
	vec4 color = diff * col;
	
	color.rgb = atmosLighting(color.rgb);

	color.rgb = scaleSoftClip(color.rgb);
	vec3 light_col = vec3(0,0,0);

	for (int i = 2; i < 8; i++)
	{
		light_col += light_diffuse[i].rgb * calcPointLightOrSpotLight(pos.xyz, vary_norm, light_position[i], light_direction[i], light_attenuation[i].x, light_attenuation[i].y, light_attenuation[i].z);
	}
	
	color.rgb += diff.rgb * vary_pointlight_col * light_col;

	frag_color = color;
}

