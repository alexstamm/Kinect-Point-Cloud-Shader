#version 400

uniform sampler2D texture;
uniform int pass;

in vec4 pos_out;
in vec4 vel_out;

//uniform int blur = 0;

out vec4 fragcolor;           
in vec2 tex_coord;

vec4 MainColor = vec4(1.0, 1.0, 1.0, 1.0);
vec4 MainColor2 = vec4(1.0, 0.0, 0.0, 1.0);
      
void main(void)
{   
	if(pass == 1)
	{
		//fragcolor = texture2D(texture, ivec2(gl_FragCoord.xy));
		//if((pos_out.y - vel_out.y) < .3){
		//	fragcolor = MainColor2;
		//}
		//else{
		//	fragcolor = MainColor;
		//}
		fragcolor = MainColor;
		
		
		
		//fragcolor = MainColor;
		
		
	}
	/*else if(pass == 2)
	{
      if(blur < 1 || blur > 2)
      {
         fragcolor = texelFetch(texture, ivec2(gl_FragCoord), 0);
      }
      fragcolor = vec4(0.0);
      for(int i = -blur; i<= +blur; i++)
      {
         for(int j = -blur; j<= +blur; j++)
         {
            fragcolor += texelFetch(texture, ivec2(gl_FragCoord)+ivec2(i,j), 0);
         }
      }
      fragcolor /= float((2*blur+1)*(2*blur+1));

	}*/
	else
	{
		fragcolor = vec4(1.0, 0.0, 1.0, 1.0); //error
	}

}




















