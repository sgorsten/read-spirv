#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform per_scene
{
    mat4 u_view_proj_matrix;
	vec3 u_eye_position;
};
layout(set = 0, binding = 1) uniform sampler2DShadow shadow_maps[4];
layout(set = 0, binding = 5) uniform sampler2D spotlight_cookie;

layout(set = 1, binding = 1) uniform sampler2D albedo_tex;
layout(set = 1, binding = 2) uniform sampler2D normal_tex;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec3 bitangent;
layout(location = 4) in vec3 normal;

layout(location = 0) out vec4 f_color;

void main() 
{
	vec3 ts_normal = texture(normal_tex, texcoord).xyz*2-1;
	vec3 normal_vec = normalize(tangent * ts_normal.x + bitangent * ts_normal.y + normal * ts_normal.z);
	vec3 eye_vec = normalize(u_eye_position - position);

	vec3 light = dot(normal_vec, vec3(0,1,0)) * vec3(1,1,0.5);

	f_color = texture(albedo_tex, texcoord) * vec4(light, 1);
}
