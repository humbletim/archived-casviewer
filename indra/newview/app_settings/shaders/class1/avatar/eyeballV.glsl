/** 
 * @file eyeballV.glsl
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
 


vec4 calcLightingSpecular(vec3 pos, vec3 norm, vec4 color, inout vec4 specularColor, vec4 baseCol);
void calcAtmospherics(vec3 inPositionEye);

void main()
{
	//transform vertex
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
	
	vec3 pos = (gl_ModelViewMatrix * gl_Vertex).xyz;
	vec3 norm = normalize(gl_NormalMatrix * gl_Normal);
		
	calcAtmospherics(pos.xyz);

	vec4 specular = vec4(1.0);
	vec4 color = calcLightingSpecular(pos, norm, gl_Color, specular, vec4(0.0));	
	gl_FrontColor = color;

}

