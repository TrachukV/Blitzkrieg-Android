#include "bk1_touch_panel.h"
#include "bk1_touch_pick.h"

#include "compat/bk1_d3d8_gles.h"
#include "compat/dinput.h"

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

// Bigger than they were. At 46 the shapes were cramped and the letters I tried
// putting under them came out as speckle -- illegible on a real phone, which is
// where that was reported from. A touch target wants about 9mm; on the engine's
// 1024x768 space stretched across a phone that is nearer 70 than 46.
const int BUTTON_SIZE  = 70;
const int BUTTON_GAP   = 10;
const int PANEL_MARGIN = 14;

// Where the interface already is, measured off a running mission rather than
// guessed: the engine prints its own notices top left, the objectives window and
// its scrollbar hold the top right, the minimap starts about 613 down the left
// and the command panel fills the bottom middle. Both columns are placed to
// clear all of it.
const int MINIMAP_TOP = 613;

// How long a pressed button stays lit. Long enough to be seen under a finger
// that is already lifting, short enough not to lag behind a player tapping the
// speed up three times in a row. Latched buttons stay lit until they let go,
// which is a different thing and not on a timer.
const long long PRESS_LIGHT_MS = 140;

// What a button does when pressed.
enum EKind
{
	KIND_COMMAND,		// posts an engine command id
	KIND_MODIFIER,		// latches a key down until the next order is given
	KIND_KEYSTROKE,		// presses and releases a combination once
};

struct SButton
{
	int    nX, nY, nW, nH;
	EKind  kind;
	int    nCommand;		// KIND_COMMAND
	int    nKey;			// KIND_MODIFIER: the key held down
	int    nKeys[3];		// KIND_KEYSTROKE: pressed in order, released in reverse
};

SButton g_buttons[BK1_PANEL_BUTTON_COUNT];
bool    g_bLaidOut = false;

int       g_nPressed   = BK1_PANEL_NONE;
long long g_nPressedAt = 0;
int       g_nLatched   = BK1_PANEL_NONE;
int       g_nLastSpeed = 0;

GLuint g_nProgram = 0;
GLuint g_nVAO     = 0;
GLuint g_nVBO     = 0;
bool   g_bTried   = false;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Set( int nButton, int nX, int nY, EKind kind, int nCommand, int nKey,
          int nKey0 = 0, int nKey1 = 0, int nKey2 = 0 )
{
	SButton &b = g_buttons[nButton];
	b.nX = nX;
	b.nY = nY;
	b.nW = BUTTON_SIZE;
	b.nH = BUTTON_SIZE;
	b.kind = kind;
	b.nCommand = nCommand;
	b.nKey = nKey;
	b.nKeys[0] = nKey0;
	b.nKeys[1] = nKey1;
	b.nKeys[2] = nKey2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LayOut()
{
	if ( g_bLaidOut )
		return;

	// Time, right edge, centred: faster at the top, slower at the bottom, pause
	// between them -- the order a speed control has everywhere, so it needs no
	// learning.
	{
		const int nCount = 3;
		const int nTotal = nCount * BUTTON_SIZE + ( nCount - 1 ) * BUTTON_GAP;
		const int nLeft  = int( ENGINE_WIDTH ) - PANEL_MARGIN - BUTTON_SIZE;
		const int nTop   = ( int( ENGINE_HEIGHT ) - nTotal ) / 2;
		const int nStep  = BUTTON_SIZE + BUTTON_GAP;
		Set( BK1_PANEL_SPEED_UP,   nLeft, nTop,             KIND_COMMAND, BK1_CMD_GAME_SPEED_INC, 0 );
		Set( BK1_PANEL_PAUSE,      nLeft, nTop + nStep,     KIND_COMMAND, BK1_CMD_GAME_PAUSE,     0 );
		Set( BK1_PANEL_SPEED_DOWN, nLeft, nTop + nStep * 2, KIND_COMMAND, BK1_CMD_GAME_SPEED_DEC, 0 );
	}

	// Orders, left edge, centred in the space above the minimap.
	{
		const int nCount = 4;
		const int nTotal = nCount * BUTTON_SIZE + ( nCount - 1 ) * BUTTON_GAP;
		const int nLeft  = PANEL_MARGIN;
		const int nTop   = ( MINIMAP_TOP - nTotal ) / 2;
		const int nStep  = BUTTON_SIZE + BUTTON_GAP;
		// The keys these hold are the ones config.cfg binds: LCTRL forces an
		// attack on the point, LALT moves ready to fight, LSHIFT adds to the
		// order queue. Pressed as keys rather than sent as commands, so that a
		// player who rebinds them in the options screen rebinds these with them.
		Set( BK1_PANEL_FORCE_ATTACK,  nLeft, nTop,             KIND_MODIFIER, 0, DIK_LCONTROL );
		Set( BK1_PANEL_AGGRESSIVE,    nLeft, nTop + nStep,     KIND_MODIFIER, 0, DIK_LMENU );
		Set( BK1_PANEL_QUEUE,         nLeft, nTop + nStep * 2, KIND_MODIFIER, 0, DIK_LSHIFT );
		Set( BK1_PANEL_CENTRE_CAMERA, nLeft, nTop + nStep * 3, KIND_KEYSTROKE, 0, 0,
		     DIK_LCONTROL, DIK_LSHIFT, DIK_C );
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
// colour interleaved. The panel is a few dozen rectangles, so one buffer and one
// draw call is the whole of it.
// Seven buttons, each a plate, two edges, a shape and now a letter of up to
// fifteen cells. AddRect drops anything past this without a word, which would
// draw half a letter and look like a rendering fault rather than a full buffer,
// so the room is counted rather than guessed: 7 * (3 + 5 + 15) = 161.
const int MAX_RECTS    = 192;
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
// The icons, drawn from rectangles rather than set in a font: the engine's text
// goes through its own texture and draw path, and reaching into that from an
// overlay would tie the panel to the state cache it is trying not to disturb.
// Shapes rather than words also read the same in every language the game ships.
// A letter, drawn from the same rectangles as everything else.
//
// The shapes alone were not enough: a target, an arrow and three bars are only
// obvious once you already know what they do, which is no help to the player
// meeting them. A letter next to the shape says which order it is, and the
// letters are the ones the game's own instructions use for these commands, so a
// sentence that mentions the attack order and the button agree with each other.
//
// Five rows of three, one bit per cell. Enough for the four letters needed and
// no more; a real font belongs to the engine and reaching into it from an
// overlay would tie this to the state cache it stays clear of.
struct SGlyph
{
	char ch;
	unsigned char rows[5];		// low three bits, left to right
};

const SGlyph GLYPHS[] =
{
	{ 'A', { 0x2, 0x5, 0x7, 0x5, 0x5 } },		// .#. #.# ### #.# #.#
	{ 'M', { 0x5, 0x7, 0x7, 0x5, 0x5 } },		// #.# ### ### #.# #.#
	{ 'Q', { 0x2, 0x5, 0x5, 0x7, 0x3 } },		// .#. #.# #.# ### ..##
	{ 'C', { 0x3, 0x4, 0x4, 0x4, 0x3 } },		// .## #.. #.. #.. .##
};

void AddLetter( char ch, float x, float y, float fCell, float r, float g, float b, float a )
{
	for ( int i = 0; i < (int)( sizeof( GLYPHS ) / sizeof( GLYPHS[0] ) ); ++i )
	{
		if ( GLYPHS[i].ch != ch )
			continue;
		for ( int nRow = 0; nRow < 5; ++nRow )
		{
			const unsigned char nBits = GLYPHS[i].rows[nRow];
			for ( int nCol = 0; nCol < 3; ++nCol )
			{
				if ( ( nBits >> ( 2 - nCol ) ) & 1 )
					AddRect( x + nCol * fCell, y + nRow * fCell, fCell, fCell, r, g, b, a );
			}
		}
		return;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AddIcon( const SButton &b, int nButton, float r, float g, float bl, float a )
{
	const float cx    = float( b.nX ) + float( b.nW ) * 0.5f;
	const float cy    = float( b.nY ) + float( b.nH ) * 0.5f;
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
	// A cross of two diagonals would need triangles; a square cross turned into
	// a target reads as "attack this spot" and is built from the same rectangles
	// as everything else: a ring drawn as four bars, with a dot at the centre.
	// Force attack: a big crosshair. Four bars and a centre dot, drawn across
	// most of the button so it reads at arm's length on a phone.
	case BK1_PANEL_FORCE_ATTACK:
	{
		const float fR = float( b.nW ) * 0.34f;
		const float fT = float( b.nW ) * 0.075f;
		AddRect( cx - fR, cy - fT * 0.5f, fR * 2.0f, fT, r, g, bl, a );
		AddRect( cx - fT * 0.5f, cy - fR, fT, fR * 2.0f, r, g, bl, a );
		AddRect( cx - fR, cy - fR, fR * 0.7f, fT, r, g, bl, a );
		AddRect( cx + fR - fR * 0.7f, cy - fR, fR * 0.7f, fT, r, g, bl, a );
		AddRect( cx - fR, cy + fR - fT, fR * 0.7f, fT, r, g, bl, a );
		AddRect( cx + fR - fR * 0.7f, cy + fR - fT, fR * 0.7f, fT, r, g, bl, a );
		AddRect( cx - fR, cy - fR, fT, fR * 0.7f, r, g, bl, a );
		AddRect( cx - fR, cy + fR - fR * 0.7f, fT, fR * 0.7f, r, g, bl, a );
		AddRect( cx + fR - fT, cy - fR, fT, fR * 0.7f, r, g, bl, a );
		AddRect( cx + fR - fT, cy + fR - fR * 0.7f, fT, fR * 0.7f, r, g, bl, a );
		break;
	}
	// Aggressive move: one big arrow pointing right, shaft and head.
	case BK1_PANEL_AGGRESSIVE:
	{
		const float fR = float( b.nW ) * 0.34f;
		const float fT = float( b.nW ) * 0.10f;
		AddRect( cx - fR, cy - fT * 0.5f, fR * 1.5f, fT, r, g, bl, a );
		for ( int i = 0; i < 4; ++i )
		{
			const float fStep = fT * 0.55f;
			AddRect( cx + fR * 0.5f + i * fStep, cy - fT * ( 2.0f - i * 0.5f ),
			         fStep, fT * ( 4.0f - i * 1.0f ), r, g, bl, a );
		}
		break;
	}
	// Queue: three thick bars, one order after another.
	case BK1_PANEL_QUEUE:
	{
		const float fR = float( b.nW ) * 0.34f;
		const float fT = float( b.nW ) * 0.11f;
		AddRect( cx - fR, cy - fR, fR * 2.0f, fT, r, g, bl, a );
		AddRect( cx - fR, cy - fT * 0.5f, fR * 2.0f, fT, r, g, bl, a );
		AddRect( cx - fR, cy + fR - fT, fR * 2.0f, fT, r, g, bl, a );
		break;
	}
	// Centre camera: four corner brackets around a dot.
	case BK1_PANEL_CENTRE_CAMERA:
	{
		const float fR = float( b.nW ) * 0.34f;
		const float fT = float( b.nW ) * 0.085f;
		const float fL = fR * 0.75f;
		AddRect( cx - fR, cy - fR, fL, fT, r, g, bl, a );
		AddRect( cx + fR - fL, cy - fR, fL, fT, r, g, bl, a );
		AddRect( cx - fR, cy + fR - fT, fL, fT, r, g, bl, a );
		AddRect( cx + fR - fL, cy + fR - fT, fL, fT, r, g, bl, a );
		AddRect( cx - fR, cy - fR, fT, fL, r, g, bl, a );
		AddRect( cx - fR, cy + fR - fL, fT, fL, r, g, bl, a );
		AddRect( cx + fR - fT, cy - fR, fT, fL, r, g, bl, a );
		AddRect( cx + fR - fT, cy + fR - fL, fT, fL, r, g, bl, a );
		AddRect( cx - fT * 0.8f, cy - fT * 0.8f, fT * 1.6f, fT * 1.6f, r, g, bl, a );
		break;
	}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PressKey( int nKey, bool bDown )
{
	Bk1PushInputEvent( BK1_INPUT_KEYBOARD, (DWORD)nKey, bDown ? 0x80 : 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Unlatch()
{
	if ( g_nLatched == BK1_PANEL_NONE )
		return;
	PressKey( g_buttons[g_nLatched].nKey, false );
	g_nLatched = BK1_PANEL_NONE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // anonymous namespace
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" int Bk1TouchPanelHitTest( int nX, int nY )
{
	if ( !Bk1IsMissionActive() )
		return BK1_PANEL_NONE;
	LayOut();
	for ( int i = 0; i < BK1_PANEL_BUTTON_COUNT; ++i )
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
	if ( nButton < 0 || nButton >= BK1_PANEL_BUTTON_COUNT )
		return;
	LayOut();
	const SButton &b = g_buttons[nButton];
	g_nPressed   = nButton;
	g_nPressedAt = nNowMs;

	switch ( b.kind )
	{
	case KIND_COMMAND:
		Bk1SendGameCommand( b.nCommand );
		break;
	case KIND_MODIFIER:
		// Pressing the latched one again lets it go: a player who armed the
		// wrong modifier needs a way out that is not giving an order with it.
		if ( g_nLatched == nButton )
		{
			Unlatch();
			LOGI( "touch panel: modifier %d released by hand", nButton );
		}
		else
		{
			// Only one at a time. Holding Ctrl and Shift together is a thing a
			// keyboard can do and two thumbs cannot, and pretending otherwise
			// would leave keys stuck down.
			Unlatch();
			g_nLatched = nButton;
			PressKey( b.nKey, true );
			LOGI( "touch panel: modifier %d latched", nButton );
		}
		break;
	case KIND_KEYSTROKE:
		for ( int i = 0; i < 3; ++i )
			if ( b.nKeys[i] != 0 )
				PressKey( b.nKeys[i], true );
		for ( int i = 2; i >= 0; --i )
			if ( b.nKeys[i] != 0 )
				PressKey( b.nKeys[i], false );
		break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" void Bk1TouchPanelReleaseModifiers( void )
{
	Unlatch();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" int Bk1TouchPanelModifierLatched( void )
{
	return g_nLatched != BK1_PANEL_NONE ? 1 : 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" void Bk1TouchPanelDraw( int nSurfaceWidth, int nSurfaceHeight, long long nNowMs )
{
	if ( !Bk1IsMissionActive() )
	{
		// A mission that ends with a modifier still latched would leave that key
		// down for whatever comes next.
		Unlatch();
		return;
	}
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
	for ( int i = 0; i < BK1_PANEL_BUTTON_COUNT; ++i )
	{
		const SButton &b = g_buttons[i];
		const bool bLatched = ( i == g_nLatched );
		const bool bLit     = ( i == g_nPressed ) || bLatched;
		// Dark, mostly transparent plate so the map stays readable under it,
		// with a lighter edge so the button has a shape against snow as well as
		// against forest. A latched modifier stays lit, because the player has
		// to be able to see that the next order carries it.
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
	g_nLatched = BK1_PANEL_NONE;
}
