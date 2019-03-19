#include <windows.h>

#include <GL/glew.h>

#include <GL/freeglut.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>

#include "InitShader.h"
#include "LoadMesh.h"
#include "LoadTexture.h"
#include "imgui_impl_glut.h"

#include <Windows.h>
#include <Ole2.h>

#include <Kinect.h>


static const std::string vertex_shader("fbo_vs.glsl");
static const std::string geometry_shader("fbo_gs.glsl");
static const std::string fragment_shader("fbo_fs.glsl");

GLuint shader_program = -1;
GLuint texture_id = -1; //Texture map for fish

GLuint quad_vao = -1;
GLuint quad_vbo = -1;

GLuint fbo_id = -1;       // Framebuffer object,
GLuint rbo_id = -1;       // and Renderbuffer (for depth buffering)
GLuint fbo_texture = -1;  // Texture rendered into.

static const std::string mesh_name = "Amago0.obj";
static const std::string texture_name = "AmagoT.bmp";
MeshData mesh_data;
float time_sec = 0.0f;
float angle = 40.5f;
float scale = 1.0f;
float speed = 1.0f;
float pivot = 1.0f;
float dirX = 0.0f;
float dirY = 0.0f;
//glm::vec4 colors = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
//float colorsFloat[4] = { 0.0, 1.0, 1.0, 1.0 };

bool isRotating = true;
bool reverseDirection = false;

bool check_framebuffer_status();

ImTextureID texID;


#define width 512
#define height 424

#define colorwidth 1920
#define colorheight 1080

GLfloat time = 0.0;

// OpenGL Variables
GLuint KtextureId;             
GLubyte data[colorwidth*colorheight * 4];  // BGRA array containing the texture data

GLuint vboId[2];
GLuint cboId;

int readIndex = 0;
int writeIndex = 1;

const int dataSize = width*height * 3 * 4 * 4;

// Intermediate Buffers
unsigned char rgbimage[colorwidth*colorheight * 4];    // Stores RGB color image
ColorSpacePoint depth2rgb[width*height];             // Maps depth pixels to rgb pixels
CameraSpacePoint depth2xyz[width*height];			 // Maps depth pixels to 3d coordinates

// Kinect Variables
IKinectSensor* sensor;             // Kinect sensor
IMultiSourceFrameReader* reader;   // Kinect data source
ICoordinateMapper* mapper;         // Converts between depth, color, and 3d coordinates



bool initKinect() {
	if (FAILED(GetDefaultKinectSensor(&sensor))) {
		return false;
	}
	if (sensor) {
		sensor->get_CoordinateMapper(&mapper);

		sensor->Open();
		sensor->OpenMultiSourceFrameReader(
			FrameSourceTypes::FrameSourceTypes_Depth | FrameSourceTypes::FrameSourceTypes_Color,
			&reader);
		return reader;
	}
	else {
		return false;
	}
}

void getDepthData(IMultiSourceFrame* frame, GLubyte* dest) {
	IDepthFrame* depthframe;
	IDepthFrameReference* frameref = NULL;
	frame->get_DepthFrameReference(&frameref);
	frameref->AcquireFrame(&depthframe);
	if (frameref) frameref->Release();

	if (!depthframe) return;

	// Get data from frame
	unsigned int sz;
	unsigned short* buf;
	depthframe->AccessUnderlyingBuffer(&sz, &buf);

	// Write vertex coordinates
	mapper->MapDepthFrameToCameraSpace(width*height, buf, width*height, depth2xyz);
	float* fdest = (float*)dest;
	for (int i = 0; i < sz; i++) {
		*fdest++ = depth2xyz[i].X;
		*fdest++ = depth2xyz[i].Y;
		*fdest++ = depth2xyz[i].Z;
	}

	// Fill in depth2rgb map
	mapper->MapDepthFrameToColorSpace(width*height, buf, width*height, depth2rgb);
	if (depthframe) depthframe->Release();
}

void getRgbData(IMultiSourceFrame* frame, GLubyte* dest) {
	IColorFrame* colorframe;
	IColorFrameReference* frameref = NULL;
	frame->get_ColorFrameReference(&frameref);
	frameref->AcquireFrame(&colorframe);
	if (frameref) frameref->Release();

	if (!colorframe) return;

	// Get data from frame
	colorframe->CopyConvertedFrameDataToArray(colorwidth*colorheight * 4, rgbimage, ColorImageFormat_Rgba);

	// Write color array for vertices
	float* fdest = (float*)dest;
	for (int i = 0; i < width*height; i++) {
		ColorSpacePoint p = depth2rgb[i];
		// Check if color pixel coordinates are in bounds
		if (p.X < 0 || p.Y < 0 || p.X > colorwidth || p.Y > colorheight) {
			*fdest++ = 0;
			*fdest++ = 0;
			*fdest++ = 0;
		}
		else {
			int idx = (int)p.X + colorwidth*(int)p.Y;
			*fdest++ = rgbimage[4 * idx + 0] / 255.;
			*fdest++ = rgbimage[4 * idx + 1] / 255.;
			*fdest++ = rgbimage[4 * idx + 2] / 255.;
		}
		// Don't copy alpha channel
	}

	if (colorframe) colorframe->Release();
}

void getKinectData() {
	IMultiSourceFrame* frame = NULL;
	if (SUCCEEDED(reader->AcquireLatestFrame(&frame))) {
		GLubyte* ptr;
		glBindBuffer(GL_ARRAY_BUFFER, vboId[writeIndex]);
		ptr = (GLubyte*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		if (ptr) {
			getDepthData(frame, ptr);
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, cboId);
		ptr = (GLubyte*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		if (ptr) {
			getRgbData(frame, ptr);
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
	if (frame) frame->Release();
}

//void draw_pass_1()
//{
//	const int pass = 1;
//
//	glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f))*glm::scale(glm::vec3(mesh_data.mScaleFactor));
//	glm::mat4 V = glm::lookAt(glm::vec3(0.0f, 1.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
//	glm::mat4 P = glm::perspective(3.141592f / 4.0f, 1.0f, 0.1f, 100.0f);
//
//	int pass_loc = glGetUniformLocation(shader_program, "pass");
//	if (pass_loc != -1)
//	{
//		glUniform1i(pass_loc, pass);
//	}
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, texture_id);
//	int PVM_loc = glGetUniformLocation(shader_program, "PVM");
//	if (PVM_loc != -1)
//	{
//		glm::mat4 PVM = P*V*M;
//		glUniformMatrix4fv(PVM_loc, 1, false, glm::value_ptr(PVM));
//	}
//
//	int tex_loc = glGetUniformLocation(shader_program, "texture");
//	if (tex_loc != -1)
//	{
//		glUniform1i(tex_loc, 0); // we bound our texture to texture unit 0
//	}
//
//	glBindVertexArray(mesh_data.mVao);
//	glDrawElements(GL_TRIANGLES, mesh_data.mNumIndices, GL_UNSIGNED_INT, 0);
//
//}
//
//void draw_pass_2()
//{
//	const int pass = 2;
//	int pass_loc = glGetUniformLocation(shader_program, "pass");
//	if (pass_loc != -1)
//	{
//		glUniform1i(pass_loc, pass);
//	}
//
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, fbo_texture);
//
//	int tex_loc = glGetUniformLocation(shader_program, "texture");
//	if (tex_loc != -1)
//	{
//		glUniform1i(tex_loc, 0); // we bound our texture to texture unit 0
//	}
//
//	glBindVertexArray(quad_vao);
//	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//
//}

void draw_gui()
{
	ImGui_ImplGlut_NewFrame();

	//create a slider to change the angle variables
	ImGui::SliderFloat("View angle", &angle, -3.141592f, +3.141592f);


	if (ImGui::BeginMenu("Blur Mode"))
	{
		static int blur;
		ImGui::RadioButton("None", &blur, 0);
		ImGui::RadioButton("3x3", &blur, 1);
		ImGui::RadioButton("5x5", &blur, 2);

		int blur_loc = glGetUniformLocation(shader_program, "blur");
		glUniform1i(blur_loc, blur);
		ImGui::EndMenu();
	}

	static bool show = false;
	ImGui::ShowTestWindow(&show);
	ImGui::Render();
}

void rotateCamera() {
	static double angle = 0.;
	static double radius = 3.;
	double x = radius*sin(angle);
	double z = radius*(1 - cos(angle)) - radius / 2;
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(x, 0, z, 0, 0, radius / 2, 0, 1, 0);
	if (isRotating) {
		if (reverseDirection) {
			angle += 0.002 * speed;
		}
		else {
			angle -= 0.002 * speed;
		}		
	}	
}

void drawKinectData() {
	getKinectData();
	rotateCamera();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	time += 0.02f;
	if (time > 25.0f) {
		time = 0.0f;
	}

	glm::mat4 M = glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f))*glm::scale(glm::vec3(3*scale));
	glm::mat4 V = glm::lookAt(glm::vec3(0.0f, pivot - 1.0f, 2.0f), glm::vec3(0.0, 0.0, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 P = glm::perspective(3.141592f / 4.0f, 1.0f, 0.1f, 7.0f);

	const int pass = 1;
	int pass_loc = glGetUniformLocation(shader_program, "pass");
	if (pass_loc != -1)
	{
		glUniform1i(pass_loc, pass);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	int PVM_loc = glGetUniformLocation(shader_program, "PVM");
	if (PVM_loc != -1)
	{
		glm::mat4 PVM = P*V*M;
		glUniformMatrix4fv(PVM_loc, 1, false, glm::value_ptr(PVM));
	}

	int tex_loc = glGetUniformLocation(shader_program, "texture");
	if (tex_loc != -1)
	{
		glUniform1i(tex_loc, 0); // we bound our texture to texture unit 0
	}

	int time_loc = glGetUniformLocation(shader_program, "time");
	if (time_loc != -1)
	{
		glUniform1f(time_loc, time); // we bound our texture to texture unit 0
	}

	//add uniforms here. bind color into texture

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	//Get Data with Transform Feedback
	glBindBuffer(GL_ARRAY_BUFFER, vboId[readIndex]);
	//capture positions
	glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vboId[writeIndex], 0, dataSize);
	//capture velocities
	glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 1, vboId[writeIndex], dataSize, dataSize);

	glVertexPointer(3, GL_FLOAT, 0, NULL);

	glBindBuffer(GL_ARRAY_BUFFER, cboId);
	glColorPointer(3, GL_FLOAT, 0, NULL);

	glPointSize(1.f);
	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, width*height);
	glEndTransformFeedback();

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	//glDisableVertexArrayAttrib(pos_loc);

	readIndex = 1 - readIndex;
	writeIndex = 1 - writeIndex;
}

void draw() {
	drawKinectData();
	draw_gui();
	glutSwapBuffers();
}

void reload_shader()
{
	GLuint new_shader = InitShader(vertex_shader.c_str(), geometry_shader.c_str(),fragment_shader.c_str());

	if (new_shader == -1) // loading failed
	{
		glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
	}
	else
	{
		glClearColor(0.35f, 0.35f, 0.35f, 0.0f);

		if (shader_program != -1)
		{
			glDeleteProgram(shader_program);
		}
		shader_program = new_shader;

		if (mesh_data.mVao != -1)
		{
			BufferIndexedVerts(mesh_data);
		}
	}
}

void printGlInfo()
{
	std::cout << "Vendor: " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
	std::cout << "Version: " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

void initOpenGl()
{
	glewInit();

	glEnable(GL_DEPTH_TEST);

	reload_shader();

	//mesh and texture for pass 1
	mesh_data = LoadMesh(mesh_name);
	texture_id = LoadTexture(texture_name.c_str());

	//mesh for pass 2 (full screen quadrilateral)
	glGenVertexArrays(1, &quad_vao);
	glBindVertexArray(quad_vao);

	float vertices[] = { 1.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f };

	//create vertex buffers for vertex coords
	glGenBuffers(1, &quad_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	int pos_loc = glGetAttribLocation(shader_program, "pos_attrib");
	if (pos_loc >= 0)
	{
		glEnableVertexAttribArray(pos_loc);
		glVertexAttribPointer(pos_loc, 3, GL_FLOAT, false, 0, 0);
	}

	//create texture to render pass 1 into
	const int w = glutGet(GLUT_WINDOW_WIDTH);
	const int h = glutGet(GLUT_WINDOW_HEIGHT);
	glGenTextures(1, &fbo_texture);
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, &cboId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	//Create renderbuffer for depth.
	glGenRenderbuffers(1, &rbo_id);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo_id);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);

	//Create the framebuffer object
	glGenFramebuffers(1, &fbo_id);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_id); // attach depth renderbuffer

	check_framebuffer_status();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}



// glut callbacks need to send keyboard and mouse events to imgui
void keyboard(unsigned char key, int x, int y)
{
	ImGui_ImplGlut_KeyCallback(key);
	std::cout << "key : " << key << ", x: " << x << ", y: " << y << std::endl;

	switch (key)
	{
	case 'r':
	case 'R':
		reload_shader();
		break;
	case 'q':
	case 'Q':
		if (!isRotating)isRotating = true;
		else if (isRotating)isRotating = false;
		break;
	case 'w':
	case 'W':
		scale += .02f;
		break;
	case 'a':
	case 'A':
		angle -= .02f;
		break;
	case 's':
	case 'S':
		scale -= .02f;
		break;
	case 'd':
	case 'D':
		angle += .02f;
		break;
	case 'f':
	case 'F':
		pivot += .02f;
		break;
	case 'v':
	case 'V':
		pivot -= .02f;
		break;
	case 'i':
	case 'I':
		dirY += .02;
		break;
	case 'k':
	case 'K':
		dirY -= .02;
		break;
	case 'j':
	case 'J':
		dirX -= .02;
		break;
	case 'l':
	case 'L':
		dirX += .02;
		break;
	}
}

void keyboard_up(unsigned char key, int x, int y)
{
	ImGui_ImplGlut_KeyUpCallback(key);
}

void special_up(int key, int x, int y)
{
	ImGui_ImplGlut_SpecialUpCallback(key);
}

void passive(int x, int y)
{
	ImGui_ImplGlut_PassiveMouseMotionCallback(x, y);
}

void special(int key, int x, int y)
{
	ImGui_ImplGlut_SpecialCallback(key);
}

void motion(int x, int y)
{
	ImGui_ImplGlut_MouseMotionCallback(x, y);
}

void mouse(int button, int state, int x, int y)
{
	ImGui_ImplGlut_MouseButtonCallback(button, state);
}

void cameraSetup() {
	
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45, width / (GLdouble)height, 0.1, 1000);
	glMatrixMode(GL_MODELVIEW);
	glutKeyboardFunc(keyboard);
	glLoadIdentity();
	gluLookAt(0, 0, 0, 0, 0, 1, 0, 1, 0);
}

void initKinectGl() {
	glewInit();

	glEnable(GL_DEPTH_TEST);

	reload_shader();

	// OpenGL setup
	glClearColor(0, 0, 0, 0);
	glClearDepth(1.0f);

	// Set up array buffers

	//const int dataSizeColor = colorwidth*colorheight * 3 * 4;

	glGenVertexArrays(1, &quad_vao);
	glBindVertexArray(quad_vao);

	//create vertex buffers for vertex coords
	glGenBuffers(2, vboId); //generate ping pong buffers
	glBindBuffer(GL_ARRAY_BUFFER, vboId[readIndex]);
	glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vboId[writeIndex]);
	glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_STREAM_COPY);
	glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vboId[writeIndex], 0, dataSize); //variable 0 is associated with the first half of the VBO (positions).
	glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 1, vboId[writeIndex], dataSize, dataSize); //variable 1 is associated with the second half of the VBO (velocities).

	int pos_loc = glGetAttribLocation(shader_program, "pos_attrib");
	if (pos_loc >= 0)
	{
		glEnableVertexAttribArray(pos_loc);
		glVertexAttribPointer(pos_loc, 3, GL_FLOAT, false, 0, 0);
	}

	int vel_loc = glGetAttribLocation(shader_program, "vel_attrib");
	if (vel_loc >= 0) {
		glEnableVertexAttribArray(vel_loc);
		glVertexAttribPointer(vel_loc, 3, GL_FLOAT, false, 0, 0); 
	}



	//create color buffer for color coords
	glGenBuffers(1, &cboId);
	glBindBuffer(GL_ARRAY_BUFFER, cboId);
	glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

	//create texture to render color data
	const int w = glutGet(GLUT_WINDOW_WIDTH);
	const int h = glutGet(GLUT_WINDOW_HEIGHT);
	glGenTextures(1, &fbo_texture);
	glBindTexture(GL_TEXTURE_2D, fbo_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, &cboId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	////Create renderbuffer for depth.
	//glGenRenderbuffers(1, &rbo_id);
	//glBindRenderbuffer(GL_RENDERBUFFER, rbo_id);
	//glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);

	//Create the framebuffer object
	glGenFramebuffers(1, &fbo_id);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);
	//glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_id); // attach depth renderbuffer

	check_framebuffer_status();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	cameraSetup();

}

bool init(int argc, char* argv[]) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(width, height);
	glutCreateWindow("CGT 596 - Alex Stamm");
	glutDisplayFunc(draw);
	glutKeyboardFunc(keyboard);
	glutIdleFunc(draw);
	//initOpenGl();
	initKinectGl();
	glewInit();
	return true;
}

int main(int argc, char* argv[]) {
	if (!init(argc, argv)) return 1;
	if (!initKinect()) return 1;

	// Main loop
	glutMainLoop();
	return 0;
}

bool check_framebuffer_status()
{
	GLenum status;
	status = (GLenum)glCheckFramebufferStatus(GL_FRAMEBUFFER);
	switch (status) {
	case GL_FRAMEBUFFER_COMPLETE:
		return true;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		printf("Framebuffer incomplete, incomplete attachment\n");
		return false;
	case GL_FRAMEBUFFER_UNSUPPORTED:
		printf("Unsupported framebuffer format\n");
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		printf("Framebuffer incomplete, missing attachment\n");
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		printf("Framebuffer incomplete, missing draw buffer\n");
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		printf("Framebuffer incomplete, missing read buffer\n");
		return false;
	}
	return false;
}
