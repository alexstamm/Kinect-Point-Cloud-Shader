#version 400            
uniform mat4 PVM;
uniform int pass;
uniform float time;

in vec3 pos_attrib;
in vec3 vel_attrib;

out vec4 pos_out;
out vec4 vel_out;
//in vec2 tex_coord_attrib;
//in vec3 normal_attrib;

//out vec2 tex_coord;  


void main(void)
{
	vel_out = vec4(normalize(pos_attrib)*.1, 1.0);
	pos_out = vec4(pos_attrib, 1.0);
	gl_Position = PVM*pos_out;

}