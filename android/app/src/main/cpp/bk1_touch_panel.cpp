#include "bk1_touch_panel.h"
#include "bk1_touch_pick.h"

#include "compat/bk1_d3d8_gles.h"

#include <GLES3/gl3.h>

#include <android/log.h>

#include <string.h>

#define LOGI( ... ) __android_log_print( ANDROID_LOG_INFO, "blitzkrieg", __VA_ARGS__ )
#define LOGE( ... ) __android_log_print( ANDROID_LOG_ERROR, "blitzkrieg", __VA_ARGS__ )

namespace
{
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The engine's own space. Every position below is in it, so the panel sits in
// the same place whatever the device's resolution, and a finger reported in
// this space can be tested against the buttons with no conversion.
const float ENGINE_WIDTH  = 1024.0f;
const float ENGINE_HEIGHT = 768.0f;

// Right edge, half way down, stacked. Measured against a running mission rather
// than guessed: the top left is where the engine prints its own notices -- the
// speed change says so itself -- the top right is the objectives window and its
// scrollbar, the bottom left is the minimap and the bottom middle the command
// panel, with the objectives button in the bottom right corner. The middle of
// the right edge is the one strip that is map in every mission, and on a phone
// held in landscape it is under the thumb already.
const int BUTTON_SIZE   = 46;
const int BUTTON_GAP    = 7;
const int PANEL_MARGIN  = 12;
const int BUTTON_COUNT  = 3;

// How long a pressed button stays lit. Long enough to be seen under a finger
// that is already lifting, short enough not to lag behind a player tapping the
// speed up three times in a row.
const long long PRESS_LIGHT_MS = 140;

struct SButton
{
	int nX, nY, nW, nH;
};

SButton g_buttons[BUTTON_COUNT];
bool    g_bLaidOut = false;

int       g_nPressed    = BK1_PANEL_NONE;
long long g_nPressedAt  = 0;
int       g_nLastSpeed  = 0;

GLuint g_nProgram = 0;
GLuint g_nVAO     = 0;
GLuint g_nVBO     = 0;
bool   g_bTried   = false;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LayOut()
{
	if ( g_bLaidOut )
		return;
	const int nTotal = BUTTON_COUNT * BUTTON_SIZE + ( BUTTON_COUNT - 1 ) * BUTTON_GAP;
	const int nLeft  = int( ENGINE_WIDTH ) - PANEL_MARGIN - BUTTON_SIZE;
	const int nTop   = ( int( ENGINE_HEIGHT ) - nTotal ) / 2;
	// Faster at the top, slower at the bottom, pause between them: the order a
	// speed control has everywhere, so it needs no learning.
	static const int nOrder[BUTTON_COUNT] = { BK1_PANEL_SPEED_UP, BK1_PANEL_PAUSE, BK1_PANEL_SPEED_DOWN };
	for ( int i = 0; i < BUTTON_COUNT; ++i )
	{
		SButton &b = g_buttons[nOrder[i]];
		b.nX = nLeft;
		b.nY = nTop + i * ( BUTTON_SIZE + BUTTON_GAP );
		b.nW = BUTTON_SIZE;
		b.nH = BUTTON_SIZE;
	}
	g_bLaidOut = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *pszVertex =
	"#version 300 es\n"
	"layout(location = 0) in vec2 aPos;\n"
	"layout(location = 1) in vec4 aColour;\n"
	"out vec4 vColour;\n"
	"void main()\n"
	"{\n"
	// Engine pixels straight to clip space: x right, y down, which is the way
	// the engine and the finger both count.
	"    vec2 ndc = vec2( aPos.x / 512.0 - 1.0, 1.0 - aPos.y / 384.0 );\n"
	"    gl_Position = vec4( ndc, 0.0, 1.0 );\n"
	"    vColour = aColour;\n"
	"}\n";

const char *pszFragment =
	"#version 300 es\n"
	"precision mediump float;\n"
	"in vec4 vColour;\n"
	"out vec4 oColour;\n"
	"void main()\n"
	"{\n"
	"    oColour = vColour;\n"
	"}\n";
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GLuint Compile( GLenum kind, const char *pszSource )
{
	const GLuint nShader = glCreateShader( kind );
	glShaderSource( nShader, 1, &pszSource, 0 );
	glCompileShader( nShader );
	GLint nOk = 0;
	glGetShaderiv( nShader, GL_COMPILE_STATUS, &nOk );
	if ( !nOk )
	{
		char szLog[512];
		glGetShaderInfoLog( nShader, sizeof( szLog ), 0, szLog );
		LOGE( "touch panel shader failed: %s", szLog );
		glDeleteShader( nShader );
		return 0;
	}
	return nShader;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool EnsureGL()
{
	if ( g_nProgram != 0 )
		return true;
	// One attempt. A failure here is a broken driver or a lost context, and
	// retrying it every frame would fill the log and slow the game down for
	// nothing.
	if ( g_bTried )
		return false;
	g_bTried = true;

	const GLuint nVS = Compile( GL_VERTEX_SHADER, pszVertex );
	const GLuint nFS = Compile( GL_FRAGMENT_SHADER, pszFragment );
	if ( nVS == 0 || nFS == 0 )
		return false;

	g_nProgram = glCreateProgram();
	glAttachShader( g_nProgram, nVS );
	glAttachShader( g_nProgram, nFS );
	glLinkProgram( g_nProgram );
	glDeleteShader( nVS );
	glDeleteShader( nFS );

	GLint nOk = 0;
	glGetProgramiv( g_nProgram, GL_LINK_STATUS, &nOk );
	if ( !nOk )
	{
		char szLog[512];
		glGetProgramInfoLog( g_nProgram, sizeof( szLog ), 0, szLog );
		LOGE( "touch panel link failed: %s", szLog );
		glDeleteProgram( g_nProgram );
		g_nProgram = 0;
		return false;
	}

	glGenVertexArrays( 1, &g_nVAO );
	glGenBuffers( 1, &g_nVBO );
	LOGI( "touch panel: ready" );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vertices are built into this each frame: six per rectangle, position and
// colour interleaved. The panel is a handful of rectangles, so one buffer and
// one draw call is the whole of it.
const int MAX_RECTS    = 24;
const int FLOATS_PER_V = 6;
float g_vertices[MAX_RECTS * 6 * FLOATS_PER_V];
int   g_nVertices = 0;

void AddRect( float x, float y, float w, float h, float r, float g, float b, float a )
{
	if ( g_nVertices + 6 > MAX_RECTS * 6 )
		return;
	const float corners[6][2] =
	{
		{ x,     y     }, { x + w, y     }, { x + w, y + h },
		{ x,     y     }, { x + w, y + h }, { x,     y + h },
	};
	for ( int i = 0; i < 6; ++i )
	{
		float *pv = &g_vertices[( g_nVertices + i ) * FLOATS_PER_V];
		pv[0] = corners[i][0];
		pv[1] = corners[i][1];
		pv[2] = r;
		pv[3] = g;
		pv[4] = b;
		pv[5] = a;
	}
	g_nVertices += 6;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The icons. Drawn from rectangles rather than set in a font: the engine's own
// text goes through its texture and its own draw path, and reaching into that
// from an overlay would tie the panel to the state cache it is trying not to
// disturb. A minus, two bars and a cross read the same in every language, which
// a word would not.
void AddIcon( const SButton &b, int nButton, float r, float g, float bl, float a )
{
	const float cx = float( b.nX ) + float( b.nW ) * 0.5f;
	const float cy = float( b.nY ) + float( b.nH ) * 0.5f;
	const float fArm  = float( b.nW ) * 0.28f;		// half the length of a stroke
	const float fThin = float( b.nW ) * 0.09f;		// half its thickness

	switch ( nButton )
	{
	case BK1_PANEL_SPEED_DOWN:
		AddRect( cx - fArm, cy - fThin, fArm * 2.0f, fThin * 2.0f, r, g, bl, a );
		break;
	case BK1_PANEL_PAUSE:
		AddRect( cx - fArm * 0.75f, cy - fArm, fThin * 1.6f, fArm * 2.0f, r, g, bl, a );
		AddRect( cx + fArm * 0.75f - fThin * 1.6f, cy - fArm, fThin * 1.6f, fArm * 2.0f, r, g, bl, a );
		break;
	case BK1_PANEL_SPEED_UP:
		AddRect( cx - fArm, cy - fThin, fArm * 2.0f, fThin * 2.0f, r, g, bl, a );
		AddRect( cx - fThin, cy - fArm, fThin * 2.0f, fArm * 2.0f, r, g, bl, a );
		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // anonymous namespace
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" int Bk1TouchPanelHitTest( int nX, int nY )
{
	if ( !Bk1IsMissionActive() )
		return BK1_PANEL_NONE;
	LayOut();
	for ( int i = 0; i < BUTTON_COUNT; ++i )
	{
		const SButton &b = g_buttons[i];
		if ( nX >= b.nX && nX < b.nX + b.nW && nY >= b.nY && nY < b.nY + b.nH )
			return i;
	}
	return BK1_PANEL_NONE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" void Bk1TouchPanelPress( int nButton, long long nNowMs )
{
	static const int nCommands[BUTTON_COUNT] =
	{
		BK1_CMD_GAME_SPEED_DEC,
		BK1_CMD_GAME_PAUSE,
		BK1_CMD_GAME_SPEED_INC,
	};
	if ( nButton < 0 || nButton >= BUTTON_COUNT )
		return;
	g_nPressed   = nButton;
	g_nPressedAt = nNowMs;
	Bk1SendGameCommand( nCommands[nButton] );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" void Bk1TouchPanelDraw( int nSurfaceWidth, int nSurfaceHeight, long long nNowMs )
{
	if ( !Bk1IsMissionActive() )
		return;
	if ( !EnsureGL() )
		return;
	LayOut();

	// The press light has to go out on its own. Expired here rather than on the
	// next press, or a button tapped once would stay lit until another was.
	if ( g_nPressed != BK1_PANEL_NONE && nNowMs - g_nPressedAt > PRESS_LIGHT_MS )
		g_nPressed = BK1_PANEL_NONE;

	// The engine's own notice says the speed changed but not to what, and it is
	// drawn in the corner for a moment. Reported here so a run leaves a number
	// behind: the button posts a command and this is what the timer made of it.
	{
		const int nSpeed = Bk1GetGameSpeed();
		if ( nSpeed != g_nLastSpeed )
		{
			LOGI( "touch panel: game speed %+d -> %+d", g_nLastSpeed, nSpeed );
			g_nLastSpeed = nSpeed;
		}
	}

	g_nVertices = 0;
	for ( int i = 0; i < BUTTON_COUNT; ++i )
	{
		const SButton &b = g_buttons[i];
		const bool bLit = ( i == g_nPressed );
		// Dark, mostly transparent plate so the map stays readable under it,
		// with a lighter edge so the button has a shape against snow as well as
		// against forest.
		AddRect( float( b.nX ), float( b.nY ), float( b.nW ), float( b.nH ),
		         bLit ? 0.55f : 0.06f, bLit ? 0.45f : 0.07f, bLit ? 0.16f : 0.06f, bLit ? 0.85f : 0.55f );
		const float fEdge = 2.0f;
		AddRect( float( b.nX ), float( b.nY ), float( b.nW ), fEdge, 0.72f, 0.68f, 0.52f, 0.55f );
		AddRect( float( b.nX ), float( b.nY + b.nH ) - fEdge, float( b.nW ), fEdge, 0.30f, 0.28f, 0.22f, 0.55f );
		AddIcon( b, i, 0.94f, 0.92f, 0.80f, 0.95f );
	}

	// The engine keeps a cache of what it has set, so anything changed here has
	// to be declared lost afterwards or the next frame will skip re-setting it
	// and draw with the panel's state. That is what ForgetGLState is for.
	glBindVertexArray( g_nVAO );
	glBindBuffer( GL_ARRAY_BUFFER, g_nVBO );
	glBufferData( GL_ARRAY_BUFFER, g_nVertices * FLOATS_PER_V * sizeof( float ), g_vertices, GL_STREAM_DRAW );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_V * sizeof( float ), (void *)0 );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, FLOATS_PER_V * sizeof( float ), (void *)( 2 * sizeof( float ) ) );

	glUseProgram( g_nProgram );
	glViewport( 0, 0, nSurfaceWidth, nSurfaceHeight );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_STENCIL_TEST );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glDrawArrays( GL_TRIANGLES, 0, g_nVertices );

	glDisableVertexAttribArray( 0 );
	glDisableVertexAttribArray( 1 );
	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	NBk1D3D::ForgetGLState();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" void Bk1TouchPanelRelease( void )
{
	// The context is already gone by the time this is called on a surface loss,
	// so the names are only cleared, not deleted through a dead context.
	g_nProgram = 0;
	g_nVAO     = 0;
	g_nVBO     = 0;
	g_bTried   = false;
	g_nPressed = BK1_PANEL_NONE;
}
