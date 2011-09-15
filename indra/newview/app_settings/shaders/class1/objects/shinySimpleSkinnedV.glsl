/** 
 * @file shinySimpleSkinnedV.glsl
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

uniform mat4 projection_matrix;
uniform mat4 texture_matrix0;
uniform mat4 texture_matrix1;
uniform mat4 modelview_matrix;

attribute vec3 position;
attribute vec3 normal;
attribute vec4 diffuse_color;
attribute vec2 texcoord0;

vec4 calcLighting(vec3 pos, vec3 norm, vec4 color, vec4 baseCol);
void calcAtmospherics(vec3 inPositionEye);
mat4 getObjectSkinnedTransform();

void main()
{
	mat4 mat = getObjectSkinnedTransform();
	
	mat = modelview_matrix * mat;
	vec3 pos = (mat*vec4(position.xyz, 1.0)).xyz;
	
	vec4 norm = vec4(position.xyz, 1.0);
	norm.xyz += normal.xyz;
	norm.xyz = (mat*norm).xyz;
	norm.xyz = normalize(norm.xyz-pos.xyz);
		
	vec3 ref = reflect(pos.xyz, -norm.xyz);

	gl_TexCoord[0] = texture_matrix0 * vec4(texcoord0,0,1);
	gl_TexCoord[1] = texture_matrix1*vec4(ref,1.0);

	calcAtmospherics(pos.xyz);

	vec4 color = calcLighting(pos.xyz, norm.xyz, diffuse_color, vec4(0.));
	gl_FrontColor = color;
	
	gl_Position = projection_matrix*vec4(pos, 1.0);
}
