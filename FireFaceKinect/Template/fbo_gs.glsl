#version 400            
layout (points) in;
layout (points, max_vertices = 10) out;

uniform float time;

in vec4 pos_out[];
in vec4 vel_out[];


const float TwoPi = 6.28318530718;
const float InverseMaxInt = 1.0 / 4294967295.0;

float randhash(uint seed, float b){
	uint i = (seed^12345391u)*2654435769u;
	i^=2654435769u;
	i+=(i<<5u)^(i>>12u);
	return float(b * i) * InverseMaxInt;
}

vec4 storePos;

void main(void)
{
		storePos = pos_out[0];
		for(int i = 0; i < 10; i++){		
			vec4 seed = vec4(.017*sin(.15*randhash(i, pos_out[0].x)),max(0.5,.1*(i+1)*randhash(i, time)), .013*cos(randhash(i, pos_out[0].z)), 0.0);
			gl_Position = gl_in[0].gl_Position+ abs(seed * vec4(pos_out[0].xyz, 0.0)) + vec4(abs(.5*vel_out[0].xyz), 0.0);	

			if((gl_Position.y - storePos.y) > -5.0 && (gl_Position.y - storePos.y) < 3.0){
				if((gl_Position.x - storePos.x) > -10.0 && (gl_Position.x - storePos.x) < 3.0){
					if((gl_Position.z - storePos.z) > -10.0){
						EmitVertex();
					}
				}
			//EndPrimitive();		
			}			
		}
				
		

	EndPrimitive();
}