#version 450
#extension GL_ARB_separate_shader_objects : enable

struct bone_info
{
    mat4 model_matrix;
	mat4 normal_matrix;
};

layout(set = 0, binding = 0) uniform per_scene
{
    mat4 u_view_proj_matrix;
	vec3 u_eye_position;
};

layout(set = 1, binding = 0) uniform per_object
{
	bone_info u_bones[64];
};

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texcoord;
layout(location = 2) in vec3 v_tangent;
layout(location = 3) in vec3 v_bitangent;
layout(location = 4) in vec3 v_normal;
layout(location = 5) in vec4 v_bone_weights;
layout(location = 6) in uvec4 v_bone_indices;

layout(location = 0) out vec3 position;
layout(location = 1) out vec2 texcoord;
layout(location = 2) out vec3 tangent;
layout(location = 3) out vec3 bitangent;
layout(location = 4) out vec3 normal;

void main() 
{
	mat4 model_matrix = u_bones[v_bone_indices.x].model_matrix * v_bone_weights.x
		+ u_bones[v_bone_indices.y].model_matrix * v_bone_weights.y
		+ u_bones[v_bone_indices.z].model_matrix * v_bone_weights.z
		+ u_bones[v_bone_indices.w].model_matrix * v_bone_weights.w;

	mat4 normal_matrix = u_bones[v_bone_indices.x].normal_matrix * v_bone_weights.x
		+ u_bones[v_bone_indices.y].normal_matrix * v_bone_weights.y
		+ u_bones[v_bone_indices.z].normal_matrix * v_bone_weights.z
		+ u_bones[v_bone_indices.w].normal_matrix * v_bone_weights.w;

	position  = (model_matrix  * vec4(v_position, 1)).xyz; // Transform position as a point
	tangent   = (model_matrix  * vec4(v_tangent,  0)).xyz; // Transform tangent as a vector
	bitangent = (model_matrix  * vec4(v_bitangent,0)).xyz; // Transform bitangent as a vector
	normal    = (normal_matrix * vec4(v_normal,   0)).xyz; // Transform normal as a bivector
	texcoord  = v_texcoord;

    gl_Position = u_view_proj_matrix * vec4(position, 1);
}
