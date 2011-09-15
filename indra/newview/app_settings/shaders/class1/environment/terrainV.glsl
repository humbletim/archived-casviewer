/** 
 * @file terrainV.glsl
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

uniform mat3 normal_matrix;
uniform mat4 texture_matrix0;
uniform mat4 texture_matrix1;
uniform mat4 texture_matrix2;
uniform mat4 texture_matrix3;
uniform mat4 modelview_matrix;
uniform mat4 modelview_projection_matrix;

attribute vec3 position;
attribute vec3 normal;
attribute vec4 diffuse_color;
attribute vec2 texcoord0;
attribute vec2 texcoord1;
attribute vec2 texcoord2;
attribute vec2 texcoord3;


vec4 calcLighting(vec3 pos, vec3 norm, vec4 color, vec4 baseCol);

vec4 texgen_object(vec4  vpos, vec4 tc, mat4 mat, vec4 tp0, vec4 tp1)
{
	vec4 tcoord;
	
	tcoord.x = dot(vpos, tp0);
	tcoord.y = dot(vpos, tp1);
	tcoord.z = tc.z;
	tcoord.w = tc.w;
	
	tcoord = mat * tcoord; 
	
	return tcoord; 
}

void main()
{
	//transform vertex
	gl_Position = modelview_projection_matrix * vec4(position.xyz, 1.0);
			
	vec4 pos = modelview_matrix * position;
	vec3 norm = normalize(normal_matrix * normal);
	
	vec4 color = calcLighting(pos.xyz, norm, vec4(1,1,1,1), diffuse_color);
	
	gl_FrontColor = color;
	
	gl_TexCoord[0] = texgen_object(vec4(position.xyz, 1.0),vec4(texcoord0,0,1),texture_matrix0,gl_ObjectPlaneS[0],gl_ObjectPlaneT[0]);
	gl_TexCoord[1] = texture_matrix1*vec4(texcoord1,0,1);
	gl_TexCoord[2] = texgen_object(vec4(position.xyz, 1.0),vec4(texcoord2,0,1),texture_matrix2,gl_ObjectPlaneS[2],gl_ObjectPlaneT[2]);
	gl_TexCoord[3] = texture_matrix3*vec4(texcoord3,0,1);
}
