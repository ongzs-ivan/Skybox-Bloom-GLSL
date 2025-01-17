#define GLFW_INCLUDE_ES2 1
#define GLFW_DLL 1
//#define GLFW_EXPOSE_NATIVE_WIN32 1
//#define GLFW_EXPOSE_NATIVE_EGL 1

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <GLFW/glfw3.h>
//#include <GLFW/glfw3native.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <fstream> 

#include "angle_util/Matrix.h"
#include "angle_util/geometry_utils.h"

#include "bitmap.h"
#include <fmod.hpp>
#include <fmod_errors.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define TEXTURE_COUNT 20

#define SPECTRUM_SIZE 128

GLint GprogramID = -1;
GLuint GtextureID[TEXTURE_COUNT];

GLuint Gframebuffer;
GLuint GdepthRenderbuffer;

GLuint GfullscreenTexture;
GLuint GblurredTexture;
GLuint GpTexture_0;
GLuint GpTexture_1;

GLFWwindow* window;

FMOD::System* m_fmodSystem;
FMOD::Sound* m_music;
FMOD::Channel* m_musicChannel;

float m_spectrumLeft[SPECTRUM_SIZE];
float m_spectrumRight[SPECTRUM_SIZE];

float spectrumAverage;

Matrix4 gPerspectiveMatrix;
Matrix4 gViewMatrix;

void ERRCHECK(FMOD_RESULT result)
{
	if (result != FMOD_OK)
	{
		printf("FMOD ERROR! (%d) %s\n", result, FMOD_ErrorString(result));
	}
}

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

GLuint LoadShader(GLenum type, const char *shaderSrc )
{
	GLuint shader;
	GLint compiled;
   
	// Create the shader object
	shader = glCreateShader ( type );

	if ( shader == 0 )
	return 0;

	// Load the shader source
	glShaderSource ( shader, 1, &shaderSrc, NULL );
   
	// Compile the shader
	glCompileShader ( shader );

	// Check the compile status
	glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );

	if ( !compiled ) 
	{
		GLint infoLen = 0;

		glGetShaderiv ( shader, GL_INFO_LOG_LENGTH, &infoLen );
      
		if ( infoLen > 1 )
		{
			char infoLog[4096];
			glGetShaderInfoLog ( shader, infoLen, NULL, infoLog );
			printf ( "Error compiling shader:\n%s\n", infoLog );            
		}

		glDeleteShader ( shader );
		return 0;
	}

	return shader;
}

GLuint LoadShaderFromFile(GLenum shaderType, std::string path)
{
    GLuint shaderID = 0;
    std::string shaderString;
    std::ifstream sourceFile( path.c_str() );

    if( sourceFile )
    {
        shaderString.assign( ( std::istreambuf_iterator< char >( sourceFile ) ), std::istreambuf_iterator< char >() );
        const GLchar* shaderSource = shaderString.c_str();

		return LoadShader(shaderType, shaderSource);
    }
    else
        printf( "Unable to open file %s\n", path.c_str() );

    return shaderID;
}

void loadTexture(const char* path, GLuint textureID)
{
	CBitmap bitmap(path);

	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Bilinear filtering.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.GetWidth(), bitmap.GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.GetBits());
}

void InitFMOD()
{
	FMOD_RESULT result;
	unsigned int version;

	result = FMOD::System_Create(&m_fmodSystem);

	result = m_fmodSystem->getVersion(&version);
	ERRCHECK(result);

	if (version < FMOD_VERSION)
	{
		printf("FMOD Error! You are using an old version of FMOD.", version, FMOD_VERSION);
	}

	//initialize fmod system
	result = m_fmodSystem->init(32, FMOD_INIT_NORMAL, 0);
	ERRCHECK(result);

	//load and set up music
	result = m_fmodSystem->createStream("../media/HotlineMiami.mp3", FMOD_SOFTWARE, 0, &m_music);
	ERRCHECK(result);

	//play the loaded mp3 music
	result = m_fmodSystem->playSound(FMOD_CHANNEL_FREE, m_music, false, &m_musicChannel);
	ERRCHECK(result);
}

void UpdateFMOD()
{
	m_fmodSystem->update();

	//set spectrum for left and right stereo channel
	m_musicChannel->getSpectrum(m_spectrumLeft, SPECTRUM_SIZE, 0, FMOD_DSP_FFT_WINDOW_RECT);

	m_musicChannel->getSpectrum(m_spectrumRight, SPECTRUM_SIZE, 0, FMOD_DSP_FFT_WINDOW_RECT);

	spectrumAverage = (m_spectrumLeft[0] + m_spectrumRight[0]) / 2.0f;

	//point the first audio spectrum for both left and right channels
	std::cout << m_spectrumLeft[0] << ", " << m_spectrumRight[0] << std::endl;
}

int Init ( void )
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint programObject;
	GLint linked;

	// Load Textures
	glGenTextures(TEXTURE_COUNT, GtextureID);
	loadTexture("../media/skybox-pieces/noon-skybox_top.bmp", GtextureID[0]);
	loadTexture("../media/skybox-pieces/noon-skybox_middle.bmp", GtextureID[1]);
	loadTexture("../media/skybox-pieces/noon-skybox_left.bmp", GtextureID[2]);
	loadTexture("../media/skybox-pieces/noon-skybox_middleright.bmp", GtextureID[3]);
	loadTexture("../media/skybox-pieces/noon-skybox_right.bmp", GtextureID[4]);
	loadTexture("../media/skybox-pieces/noon-skybox_bottom.bmp", GtextureID[5]);

	loadTexture("../media/cubes.bmp", GtextureID[6]);
	loadTexture("../media/DarkRainbow.bmp", GtextureID[7]);
	loadTexture("../media/roughrock.bmp", GtextureID[8]);
	loadTexture("../media/graphics-card.bmp", GtextureID[9]);
	loadTexture("../media/lava-rock.bmp", GtextureID[10]);

	// Initialize FMOD
	//InitFMOD();

	//=========================================================================================
	//Set up frame buffer, render buffer, and create an empty texture for blurring purpose.
	//Create a new FBO.
	glGenFramebuffers(1, &Gframebuffer);

	//Create a new empty texture for rendering original scene
	glGenTextures(1, &GfullscreenTexture);
	glBindTexture(GL_TEXTURE_2D, GfullscreenTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Create a new empty texture for blurring
	glGenTextures(1, &GblurredTexture);
	glBindTexture(GL_TEXTURE_2D, GblurredTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Create first empty texture to process first pass processing
	glGenTextures(1, &GpTexture_0);
	glBindTexture(GL_TEXTURE_2D, GpTexture_0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Create second empty texture to process first pass processing
	glGenTextures(1, &GpTexture_1);
	glBindTexture(GL_TEXTURE_2D, GpTexture_1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	//Create and bind renderbuffer, and create a 16-bit depth buffer
	glGenRenderbuffers(1, &GdepthRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, GdepthRenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, WINDOW_WIDTH, WINDOW_HEIGHT);
	//=========================================================================================

	vertexShader = LoadShaderFromFile(GL_VERTEX_SHADER, "../vertexShader0.vert");
	fragmentShader = LoadShaderFromFile(GL_FRAGMENT_SHADER, "../BloomEffectShader2.frag");

	// Create the program object
	programObject = glCreateProgram ( );
   
	if ( programObject == 0 )
		return 0;

	glAttachShader ( programObject, fragmentShader );
	glAttachShader ( programObject, vertexShader );

	// Bind vPosition to attribute 0   
	glBindAttribLocation ( programObject, 0, "vPosition" );
	glBindAttribLocation ( programObject, 1, "vColor" );
	glBindAttribLocation ( programObject, 2, "vTexCoord" );

	// Link the program
	glLinkProgram ( programObject );

	// Check the link status
	glGetProgramiv ( programObject, GL_LINK_STATUS, &linked );

	if ( !linked ) 
	{
		GLint infoLen = 0;

		glGetProgramiv ( programObject, GL_INFO_LOG_LENGTH, &infoLen );
      
		if ( infoLen > 1 )
		{
			//char* infoLog = malloc (sizeof(char) * infoLen );
			char infoLog[512];
			glGetProgramInfoLog ( programObject, infoLen, NULL, infoLog );
			printf ( "Error linking program:\n%s\n", infoLog );            
         
			//free ( infoLog );
		}

		glDeleteProgram ( programObject );
		return 0;
	}

	// Store the program object
	GprogramID = programObject;

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Initialize matrices
	gPerspectiveMatrix = Matrix4::perspective(70.0f, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.5f, 90.0f);
	gViewMatrix = Matrix4::translate(Vector3(0.0f, 0.0f, -2.0f));

	return 1;
}

void UpdateCamera(void)
{
	static float yaw = 0.0f;
	static float pitch = 0.0f;
	static float distanceX = 0.0f;
	static float distanceY = 0.0f;
	static float distanceZ = -2.5f;

	if (glfwGetKey(window, 'J'))
	{
		pitch -= 1.0f;
	}
	if (glfwGetKey(window, 'L'))
	{
		pitch += 1.0f;
	}
	if (glfwGetKey(window, 'I'))
	{
		yaw -= 1.0f;
	}
	if (glfwGetKey(window, 'K'))
	{
		yaw += 1.0f;
	}
	if (glfwGetKey(window, 'D'))
	{
		distanceX -= 0.1f;
	}
	if (glfwGetKey(window, 'A'))
	{
		distanceX += 0.1f;
	}
	if (glfwGetKey(window, 'X'))
	{
		distanceY += 0.1f;
	}
	if (glfwGetKey(window, 'Z'))
	{
		distanceY -= 0.1f;
	}
	if (glfwGetKey(window, 'S'))
	{
		distanceZ -= 0.1f;
	}
	if (glfwGetKey(window, 'W'))
	{
		distanceZ += 0.1f;
	}

	gViewMatrix =
		Matrix4::rotate(yaw, Vector3(1.0f, 0.0f, 0.0f)) *
		Matrix4::translate(Vector3(distanceX, distanceY, distanceZ)) *
		Matrix4::rotate(pitch, Vector3(0.0f, 1.0f, 0.0f));
}

void DrawCube(GLuint texture, float sizeX = 1.0f, float sizeY = 1.0f, float sizeZ = 1.0f)
{
	sizeX /= 2.0f;
	sizeY /= 2.0f;
	sizeZ /= 2.0f;

	GLfloat vVertices[] =
	{
		// Negative Square Z
		-sizeX,  sizeY, -sizeZ,
		-sizeX, -sizeY, -sizeZ,
		 sizeX, -sizeY, -sizeZ,

		-sizeX,  sizeY, -sizeZ,
		 sizeX,  sizeY, -sizeZ,
		 sizeX, -sizeY, -sizeZ,

		// Positive Square Z
		-sizeX,  sizeY,  sizeZ,
		-sizeX, -sizeY,  sizeZ,
		 sizeX, -sizeY,  sizeZ,
						 
		-sizeX,  sizeY,  sizeZ,
		 sizeX,  sizeY,  sizeZ,
		 sizeX, -sizeY,  sizeZ,

		// Negative Square Y
		 sizeX, -sizeY, -sizeZ,
		-sizeX, -sizeY, -sizeZ,
		-sizeX, -sizeY,  sizeZ,

		 sizeX, -sizeY, -sizeZ,
		 sizeX, -sizeY,  sizeZ,
		-sizeX, -sizeY,  sizeZ,

		// Positive Square Y
		 sizeX,  sizeY, -sizeZ,
		-sizeX,  sizeY, -sizeZ,
		-sizeX,  sizeY,  sizeZ,
				 
		 sizeX,  sizeY, -sizeZ,
		 sizeX,  sizeY,  sizeZ,
		-sizeX,  sizeY,  sizeZ,

		// Negative Square X
		-sizeX, -sizeY,  sizeZ,
		-sizeX, -sizeY, -sizeZ,
		-sizeX,  sizeY, -sizeZ,

		-sizeX, -sizeY,  sizeZ,
		-sizeX,  sizeY,  sizeZ,
		-sizeX,  sizeY, -sizeZ,

		// Positive Square X
		 sizeX, -sizeY,  sizeZ,
		 sizeX, -sizeY, -sizeZ,
		 sizeX,  sizeY, -sizeZ,
		 
		 sizeX, -sizeY,  sizeZ,
		 sizeX,  sizeY,  sizeZ,
		 sizeX,  sizeY, -sizeZ,
	};

	GLfloat vColors[] =
	{
		//Negative Z
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,

		//Positive Z
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,

		//Negative Y
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,

		//Positive Y
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,

		//Negative X
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,

		//Positive X
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};

	GLfloat vTexCoords[] =
	{
		//-z
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,

		//+z
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,

		//-y
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,

		//+y
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,

		//-x
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,

		//+x
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
	};

	glBindTexture(GL_TEXTURE_2D, texture);

	// Use the program object
	glUseProgram(GprogramID);

	// Load the vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, vColors);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, vTexCoords);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glDrawArrays(GL_TRIANGLES, 0, 36);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
}

void DrawSquare(GLuint texture, float sizeX = 1.0f, float sizeY = 1.0f)
{
	GLfloat vVertices[] =
	{
		-sizeX,  sizeY, 0.0f,
		-sizeX, -sizeY, 0.0f,
		 sizeX, -sizeY, 0.0f,
		-sizeX,  sizeY, 0.0f,
		 sizeX,  sizeY, 0.0f,
		 sizeX, -sizeY, 0.0f,
	};

	GLfloat vColors[] =
	{
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};

	GLfloat vTexCoords[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
	};

	glBindTexture(GL_TEXTURE_2D, texture);

	// Use the program object
	//glUseProgram(GprogramID);

	// Load the vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, vColors);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, vTexCoords);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
}

void DrawPyramid(GLuint texture, float sizeX = 1.0f, float sizeY = 1.0f, float sizeZ = 1.0f)
{
	GLfloat vVertices[] =
	{
		  //+z
		  0.0f,   sizeY,   0.0f,
		 -sizeX, -sizeY, sizeZ,
		  sizeX, -sizeY, sizeZ,
		  //-z
		  0.0f,   sizeY,   0.0f,
		 -sizeX, -sizeY, -sizeZ,
		  sizeX, -sizeY, -sizeZ,
		  //+x
		  0.0f,   sizeY,   0.0f,
		  sizeX, -sizeY, -sizeZ,
		  sizeX, -sizeY,  sizeZ,
		  //-x
		  0.0f,   sizeY,   0.0f,
		 -sizeX, -sizeY, -sizeZ,
		 -sizeX, -sizeY,  sizeZ,
		  //bottom square
		  sizeX, -sizeY,  sizeZ,
		 -sizeX, -sizeY,  sizeZ,
		 -sizeX, -sizeY, -sizeZ,

		  sizeX, -sizeY,  sizeZ,
		  sizeX, -sizeY, -sizeZ,
		 -sizeX, -sizeY, -sizeZ
	};

	GLfloat vColors[] =
	{
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};

	GLfloat vTexCoords[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,

		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,

		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,

		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,

		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,

		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
	};

	glBindTexture(GL_TEXTURE_2D, texture);

	// Use the program object
	//glUseProgram(GprogramID);

	// Load the vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, vColors);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, vTexCoords);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glDrawArrays(GL_TRIANGLES, 0, 18);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
}

//void DrawPyramid(float sizeX = 1.0f, float sizeY = 1.0f, float sizeZ = 1.0f,
//	float offsetX = 0.0f, float offsetY = 0.0f, float offsetZ = 0.0f, int textureType = 0)
//{
//	Matrix4 modelMatrix, mvpMatrix;
//
//	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY, offsetZ)) *
//		Matrix4::rotate(90, Vector3(1.0f, 0.0f, 0.0f));
//
//	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
//	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
//	DrawSquare(GtextureID[textureType], sizeX, sizeY); 
//	//-Z Triangle
//	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY, offsetZ - sizeZ * 0.5f)) *
//		Matrix4::rotate(45.0f, Vector3(1.0f, 0.0f, 0.0f));
//
//	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
//	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
//	DrawTriangle(GtextureID[textureType], sizeX, sizeY); 
//	//+Z Triangle
//	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY, offsetZ + sizeZ * 0.5f)) *
//		Matrix4::rotate(180.0f, Vector3(0.0f, 1.0f, 0.0f)) *
//		Matrix4::rotate(45.0f, Vector3(1.0f, 0.0f, 0.0f));
//
//	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
//	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
//	DrawTriangle(GtextureID[textureType], sizeX, sizeY);
//	//-X Triangle
//	modelMatrix = Matrix4::translate(Vector3(offsetX - sizeX * 0.5f, offsetY, offsetZ)) *
//		Matrix4::rotate(90.0f, Vector3(0.0f, 1.0f, 0.0f)) *
//		Matrix4::rotate(45.0f, Vector3(1.0f, 0.0f, 0.0f));
//
//	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
//	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
//	DrawTriangle(GtextureID[textureType], sizeX, sizeY);
//	//+X Triangle
//	modelMatrix = Matrix4::translate(Vector3(offsetX + sizeX * 0.5f, offsetY, offsetZ)) *
//		Matrix4::rotate(270.0f, Vector3(0.0f, 1.0f, 0.0f)) *
//		Matrix4::rotate(45.0f, Vector3(1.0f, 0.0f, 0.0f));
//
//	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
//	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
//	DrawTriangle(GtextureID[textureType], sizeX, sizeY);
//}

void DrawGraphicsSample(float rotation = 0.0f)
{
	Matrix4 modelMatrix, mvpMatrix;
	//====================
	modelMatrix = Matrix4::translate(Vector3(-1.0f, 0.0f, 0.0f)) *
		Matrix4::rotate(rotation, Vector3(0.0f, 1.0f, 0.0f));
	
	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[9]); //Draw first rectangle, image based on number.
	//====================
	modelMatrix = Matrix4::translate(Vector3(1.0f, 0.0f, 0.0f)) *
		Matrix4::rotate(-rotation, Vector3(0.0f, 1.0f, 0.0f));
	
	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[9]); //Draw second rectangle, image based on number.
	//====================
}

void DrawSkyBox(float sizeX = 1.0f, float sizeY = 1.0f, float sizeZ = 1.0f)
{
	Matrix4 modelMatrix, mvpMatrix;

	modelMatrix = Matrix4::translate(Vector3(0.0f, sizeY, 0.0f)) * 
		Matrix4::rotate(90, Vector3(1.0f, 0.0f, 0.0f));
	
	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[0], sizeX, sizeY); //Top texture

	modelMatrix = Matrix4::translate(Vector3(0.0f, 0.0f, -sizeZ)) * 
		Matrix4::rotate(0, Vector3(0.0f, 0.0f, 0.0f));
	
	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[1], sizeX, sizeY); //Middle texture

	modelMatrix = Matrix4::translate(Vector3(-sizeX, 0.0f, 0.0f)) * 
		Matrix4::rotate(90, Vector3(0.0f, 1.0f, 0.0f));
	
	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[2], sizeX, sizeY); //Left texture

	modelMatrix = Matrix4::translate(Vector3(sizeX, 0.0f, 0.0f)) * 
		Matrix4::rotate(90, Vector3(0.0f, -1.0f, 0.0f));
	
	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[3], sizeX, sizeY); //Middle-right texture

	modelMatrix = Matrix4::translate(Vector3(0.0f, 0.0f, sizeZ)) * 
		Matrix4::rotate(180, Vector3(0.0f, 1.0f, 0.0f));
	
	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[4], sizeX, sizeY); //Right texture

	modelMatrix = Matrix4::translate(Vector3(0.0f, -sizeY, 0.0f)) * 
		Matrix4::rotate(90, Vector3(1.0f, 0.0f, 0.0f));
	
	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[5], sizeX, sizeY); //Bottom texture
}

void DrawCubeMove(float sizeX = 1.0f, float sizeY = 1.0f, float sizeZ = 1.0f,
	float offsetX = 0.0f, float offsetY = 0.0f, float offsetZ = 0.0f, int textureType = 0)
{
	Matrix4 modelMatrix, mvpMatrix;

	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY + sizeY, offsetZ)) * 
		Matrix4::rotate(90, Vector3(1.0f, 0.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[textureType], sizeX, sizeY); //Top texture
	//
	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY, offsetZ - sizeZ)) * 
		Matrix4::rotate(0, Vector3(0.0f, 0.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[textureType], sizeX, sizeY); //Middle texture
	//
	modelMatrix = Matrix4::translate(Vector3(offsetX - sizeX, offsetY, offsetZ)) * 
		Matrix4::rotate(90, Vector3(0.0f, 1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[textureType], sizeX, sizeY); //Left texture
	//
	modelMatrix = Matrix4::translate(Vector3(offsetX + sizeX, offsetY, offsetZ)) * 
		Matrix4::rotate(90, Vector3(0.0f, -1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[textureType], sizeX, sizeY); //Middle-right texture
	//
	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY, offsetZ + sizeZ)) * 
		Matrix4::rotate(180, Vector3(0.0f, 1.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[textureType], sizeX, sizeY); //Right texture
	//
	modelMatrix = Matrix4::translate(Vector3(offsetX, offsetY - sizeY, offsetZ)) * 
		Matrix4::rotate(90, Vector3(1.0f, 0.0f, 0.0f));

	mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
	DrawSquare(GtextureID[textureType], sizeX, sizeY); //Bottom texture
}

// Variables For Music
float factor0 = 0.0f;

const float PI = 3.142f;

float move0 = 0.0f;
float move1 = 0.0f;

void ShaderPass(GLuint &tex0, GLuint &tex1, GLuint &texFullScr);

void Draw(void)
{
	factor0 += 0.1f;

	move0 += 0.05f;
	move1 = 5.0f * sinf(move0);

	GLint factor0Loc = glGetUniformLocation(GprogramID, "Factor0");
	if (factor0Loc != -1)
	{
		glUniform1f(factor0Loc, factor0);
	}

	// Use the program object, it's possible that you have multiple shader programs and switch it accordingly
	glUseProgram(GprogramID);

	// Set the sampler2D varying variable to the first texture unit(index 0)
	glUniform1i(glGetUniformLocation(GprogramID, "sampler2D"), 0);

	//=============================================
	//Pass texture size to shader.
	glUniform1f(glGetUniformLocation(GprogramID, "uTextureW"), (GLfloat)WINDOW_WIDTH);
	glUniform1f(glGetUniformLocation(GprogramID, "uTextureH"), (GLfloat)WINDOW_HEIGHT);
	//=============================================

	//UpdateFMOD();
	UpdateCamera();

	// Set the viewport
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	//=============================================
	//First pass, render entire screen as texture
	//=============================================

	//Bind the framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Specify texture as color attachment
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GfullscreenTexture, 0);

	//Specify depth_renderbufer as depth attachment
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GdepthRenderbuffer);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Set to normal
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);
		
		//Set to no blur.
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

		DrawGraphicsSample(move0 * 5.0f);

		DrawSkyBox(20.0f, 20.0f, 20.0f);

		DrawCubeMove(1.0f, 1.0f, 1.0f, -3.0f + move1 , 8.0f, 5.0f, 8);

		DrawCubeMove(0.5f, 0.5f, 0.5f, 0.0f, 0.0f + move1 * 0.25, -5.0f, 10);

		DrawCubeMove(0.75f, 0.75f, 0.75f, 5.0f, 0.0f, 0.0f + (5.0f * sinf(move1 * 0.5)), 6);

		Matrix4 modelMatrix, mvpMatrix;

		modelMatrix = Matrix4::translate(Vector3(5.0f, 5.0f + move1, 0.0f));

		mvpMatrix = gPerspectiveMatrix * gViewMatrix * modelMatrix;
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, mvpMatrix.data);
		DrawPyramid(GtextureID[6], 1.0f, 1.0f, 1.0f);
	}
	else
	{
		printf("Line 796 : Frame buffer is not ready!\n");
	}
	/*
	for (int i = 0; i < 2; ++i)
	{
		ShaderPass(GpTexture_0, GpTexture_1, GfullscreenTexture);
	}
	*/

	for (int i = 0; i < 3; ++i)
	{
		ShaderPass(GpTexture_0, GpTexture_1, GfullscreenTexture);

		if (!(++i < 3))
		{
			break;
		}

		ShaderPass(GpTexture_0, GfullscreenTexture, GpTexture_1);
	}

	//=============================================
	//Draw the texture
	//=============================================

	// This time, render directly to Windows System Framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Clear the buffers (Clear the screen basically)
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

	// Set state to normal
	glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);

	// Set to no blur
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

	// Draw the textures
	DrawSquare(GfullscreenTexture);

	/*
	//=============================================
	//Second pass, apply high pass filter on texture
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Change the render target to GtextureBlurred
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GpTexture_0, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		//Set state to high pass filter;
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 0);

		//Tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

		DrawSquare(GfullscreenTexture);
	}
	else
	{
		printf("Line 828 : Frame buffer is not ready!\n");
	}
	
	//=============================================
	//Third pass, horizontal blur
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GpTexture_1, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		// Clear the buffers (Clear the screen basically)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		// Set state to gaussian blur
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 1);

		// Draw the texture that has been screen captured, apply Horizontal blurring
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), 0);

		DrawSquare(GpTexture_0);
	}
	else
	{
		printf("Line 859 : Framebuffer is not ready!\n");
	}
	
	//=============================================
	//Fourth pass, vertical blur
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Change the render target to GtextureBlurred
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, GpTexture_0, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		//Set state to high pass filter;
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 1);

		//Tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), 1);

		DrawSquare(GpTexture_1);
	}
	else
	{
		printf("Line 891 : Frame buffer is not ready!\n");
	}

	//=============================================
	//Final pass, combine all textures
	//=============================================

	// This time, render directly to Windows System Framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Clear the buffers (Clear the screen basically)
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
	glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

	// Set state to normal
	glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);

	// Set to no blur
	glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

	// Draw the texturesla
	glDepthMask(GL_FALSE);

	DrawSquare(GfullscreenTexture);

	//Using additive blending.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	DrawSquare(GpTexture_0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDepthMask(GL_TRUE);
	*/
}

void ShaderPass(GLuint &tex0, GLuint &tex1, GLuint &texFullScr)
{
	//=============================================
	//Second pass, apply high pass filter on texture
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Change the render target to GtextureBlurred
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0, 0);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		//Set state to high pass filter;
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 0);

		//Tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

		DrawSquare(texFullScr);
	}
	else
	{
		printf("Line 828 : Frame buffer is not ready!\n");
	}

	//=============================================
	//Third pass, horizontal blur
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		// Clear the buffers (Clear the screen basically)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		// Set state to gaussian blur
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 1);

		// Draw the texture that has been screen captured, apply Horizontal blurring
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), 0);

		DrawSquare(tex0);
	}
	else
	{
		printf("Line 859 : Framebuffer is not ready!\n");
	}

	//=============================================
	//Fourth pass, vertical blur
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Change the render target to GtextureBlurred
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		//Set state to high pass filter;
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), 1);

		//Tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), 1);

		DrawSquare(tex1);
	}
	else
	{
		printf("Line 891 : Frame buffer is not ready!\n");
	}

	//=============================================
	//Final pass, combine all textures
	//=============================================

	//Bind the framebuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Change the render target to GtextureBlurred
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Reset the mvpMatrix to identity matrix so that it renders fully on texture in normalized device coordinates
		glUniformMatrix4fv(glGetUniformLocation(GprogramID, "uMvpMatrix"), 1, GL_FALSE, Matrix4::identity().data);

		//Set state to high pass filter;
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);

		//Tell the shader to apply horizontal blurring, for details please check the "uBlurDirection" flag in the shader code
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

		// Draw the textures
		glDepthMask(GL_FALSE);

		DrawSquare(texFullScr);

		//Using additive blending.
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		DrawSquare(tex0);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glDepthMask(GL_TRUE);
	}
	else
	{
		printf("Line 891 : Frame buffer is not ready!\n");
	}
	/*
	//=============================================
	//First pass, render entire screen as texture
	//=============================================

	//Bind the framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, Gframebuffer);

	//Specify texture as color attachment
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texFullScr, 0);

	//Specify depth_renderbufer as depth attachment
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GdepthRenderbuffer);

	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Set to normal
		glUniform1i(glGetUniformLocation(GprogramID, "uState"), -1);

		//Set to no blur.
		glUniform1i(glGetUniformLocation(GprogramID, "uBlurDirection"), -1);

		DrawSquare(tex1);
	}
	else
	{
		printf("Line 796 : Frame buffer is not ready!\n");
	}
	*/
}

int main(void)
{
	glfwSetErrorCallback(error_callback);

	// Initialize GLFW library
	if (!glfwInit())
	return -1;

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	// Create and open a window
	window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Shader Test", NULL, NULL);

	if (!window)
	{
		glfwTerminate();
		printf("glfwCreateWindow Error\n");
		exit(1); 
	}

	glfwMakeContextCurrent(window);
	
	Init();

	// Repeat Update Function
	while (!glfwWindowShouldClose(window)) 
	{
		Draw();
		glfwSwapBuffers(window);
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE))
		{
			break;
		}
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	exit(EXIT_SUCCESS);
}
