/** 
 * @file riftDistorF.glsl
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright © 2013, David Rowe
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
 * david@ctrlaltstudio.com
 * $/LicenseInfo$
 */

#extension GL_ARB_texture_rectangle : enable

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

VARYING vec2 vary_fragcoord;

uniform sampler2DRect diffuseRect;
uniform vec2 sample_size;
uniform vec2 scale_in;
uniform vec2 scale_out;
uniform vec2 lens_center_in;
uniform vec2 lens_center_out;
uniform vec4 warp_params;

vec2 hmdWarp(vec2 xy)
{
	vec2 theta = (xy - lens_center_in) * scale_in;  // Scales to [-1, 1]
	float rSq = theta.x * theta.x + theta.y * theta.y;
	vec2 rVector = theta * (warp_params.x + 
							warp_params.y * rSq +
							warp_params.z * rSq * rSq +
							warp_params.w * rSq * rSq * rSq);

	return lens_center_out + rVector * scale_out;
}

void main()
{
	vec2 fragcoord = hmdWarp(vary_fragcoord);

	if (all(equal(clamp(fragcoord, vec2(0,0), sample_size), fragcoord)))
	{
		frag_color = texture2DRect(diffuseRect, fragcoord);
	}
	else
	{
		frag_color = vec4(0.0);
	}

}
