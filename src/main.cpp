#include <SDL2/SDL.h>

#include <tinyxml2.h>
#include <iostream>

#include "UniSampler.h"

// prototype for our audio callback
// see the implementation for more information
void ProcessSampler( void *userdata, Uint8 *stream, int len );

// Global sampler instance
std::mutex g_muSampler;
UniSampler g_Sampler;

// Playstate struct
struct PlayState
{
	float fCurrentPos{ 0 };
	uint32_t uTempo{ 120 };
} g_PlayState;

// Global audio spec
SDL_AudioSpec g_AudioSpec;

// Map of key presses to notes
struct midikeynote_t
{
	uint8_t iKey;
	uint8_t iVel;
	uint8_t iChan;
	bool bDown;
	midikeynote_t( int key = 64, int vel = 64, int chan = 0 ) :iKey( key ), iVel( vel ), iChan( chan ), bDown( false ) {}
};
std::map<SDL_Keycode, midikeynote_t> g_mapKeyPresses;

// Map of key presses to sequences
struct midikeyclip_t
{
	int iChan;
	bool bDown;
	UniSampler::Clip_t clip;
	midikeyclip_t( int chan = 0, UniSampler::Clip_t c = {} ) : iChan( chan ), clip( c ), bDown( false ) {}
};
std::map<SDL_Keycode, midikeyclip_t> g_mapKeySequences;

// Defined below
bool handleKey( bool bKeyDown, SDL_Keycode keySym );
void setupKeyboard( tinyxml2::XMLDocument * pDoc );

// Expects XML file as first argument
int main( int argc, char* argv[] )
{
	// We'll load in an XML file from the command line
	if ( argc < 2 )
	{
		std::cout << "Missing XML file arg" << std::endl;
		return -1;
	}

	// Load XML file (relative path)
	std::string strCurPath = argv[0];
#if _WIN32
	strCurPath = strCurPath.substr( 0, strCurPath.find_last_of( '\\' ) );
#else
	strCurPath = strCurPath.substr( 0, strCurPath.find_last_of( '/' ) );
#endif

	// Make sure it's valid
	std::string strFilePath = strCurPath + "/" + argv[1];
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError xErr = doc.LoadFile( strFilePath.c_str() );
	if ( xErr != tinyxml2::XML_SUCCESS )
	{
		TRACE( "Invalid XML file ", strFilePath );
		return -1;
	}

	// Load program
	if ( g_Sampler.LoadFile( strFilePath.c_str() ) == false )
	{
		TRACE( "Unable to load program file", strFilePath );
		return -1;
	}

	// Get keyboard input setup
	setupKeyboard( &doc );

	// Initialize SDL.
	if ( SDL_Init( SDL_INIT_AUDIO ) < 0 )
	{
		TRACE( "Unable to open audio device", SDL_GetError() );
		return -1;
	}

	// Config audio
	SDL_AudioSpec desired{ 0 }, received{ 0 };
	desired.callback = ProcessSampler;
	desired.userdata = &g_Sampler;
	desired.freq = 48000;
	desired.samples = 1024;
	desired.channels = 2;
	desired.format = AUDIO_F32;

	/* Open the audio device */
	int dev = SDL_OpenAudio( &desired, &received );
	if ( dev < 0 )
	{
		TRACE( "Couldn't open audio", SDL_GetError() );
		return -1;
	}
	// TRACE( "Opened", SDL_GetAudioDeviceName( dev, 0 ) );

	// Check wav spec
	if ( desired.channels != received.channels )
	{
		TRACE( "Invalid channel config", received.channels );
		return -1;
	}

	// Cache received spec
	g_AudioSpec = received;

	// Init sampler static vars
	UniSampler::InitStatic( 256 );

	// Init sampler
	g_Sampler.Init( (float) received.freq, 1.f, -1.f, 1.f );
	g_Sampler.Enable( true );

	// Create window... we need it for keyboard input. I might disable this later on. 
	SDL_Window * pWindow = SDL_CreateWindow( "SDL2UniSampler", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 500, 500, SDL_WINDOW_OPENGL );
	if ( pWindow == nullptr )
	{
		TRACE( "Unable to open SDL window" );
		return -1;
	}

	/* Start playing */
	SDL_PauseAudio( 0 );

	// Event loop
	SDL_Event e{ 0 };
	auto keySym = [&e] () { return e.key.keysym.sym; };
	bool bRun = true;
	while ( bRun )
	{
		while ( SDL_PollEvent( &e ) )
		{
			if ( e.type == SDL_QUIT )
			{
				bRun = false;
				break;
			}
			switch ( e.type )
			{
				case SDL_KEYDOWN:
				case SDL_KEYUP:
					bRun = handleKey( e.type == SDL_KEYDOWN, keySym() );
					break;
			}
		}
	}

	// shut everything down
	SDL_DestroyWindow( pWindow );
	SDL_CloseAudio();

	return EXIT_SUCCESS;
}

// Handle XML opcodes for keyboard mapping
void setupKeyboard( tinyxml2::XMLDocument * pDoc )
{
	for ( tinyxml2::XMLElement * pKeyMap = pDoc->FirstChildElement( "keymap" ); pKeyMap; pKeyMap = pKeyMap->NextSiblingElement( "keymap" ) )
	{
		// Single notes
		for ( tinyxml2::XMLElement * pKey = pKeyMap->FirstChildElement( "key" ); pKey; pKey = pKey->NextSiblingElement( "key" ) )
		{
			if ( const char * szKey = pKey->Attribute( "key" ) )
			{
				SDL_Keycode keyCode = (SDL_Keycode) szKey[0];

				int iNote = 64;
				int iVel = 64;
				int iChan = 0;
				if ( const char * szNote = pKey->Attribute( "note" ) )
					iNote = atoi( szNote );
				if ( const char * szVel = pKey->Attribute( "vel" ) )
					iVel = atoi( szVel );
				if ( const char * szChan = pKey->Attribute( "chan" ) )
					iChan = atoi( szChan );
				g_mapKeyPresses[keyCode] = midikeynote_t( iNote, iVel, iChan );
			}
		}

		// Sequences
		for ( tinyxml2::XMLElement * pSeq = pKeyMap->FirstChildElement( "sequence" ); pSeq; pSeq = pSeq->NextSiblingElement( "sequence" ) )
		{
			if ( const char * szKey = pSeq->Attribute( "key" ) )
			{
				SDL_Keycode keyCode = (SDL_Keycode) szKey[0];

				int iChan = 0;
				if ( const char * szChan = pSeq->Attribute( "chan" ) )
					iChan = atoi( szChan );
				float fStepDur = 1.f;
				if ( const char * szStepDur = pSeq->Attribute( "stepdur" ) )
					fStepDur = atof( szStepDur );

				UniSampler::Clip_t clip;

				for ( tinyxml2::XMLElement * pNote = pSeq->FirstChildElement( "note" ); pNote; pNote = pNote->NextSiblingElement( "note" ) )
				{
					if ( const char * szData = pNote->Attribute( "data" ) )
					{
						UniSampler::NoteSeq_t nS;
						nS.seq.SetStepDur( fStepDur );
						if ( const char * szNote = pNote->Attribute( "note" ) )
							nS.note = atoi( szNote );
						std::string strData = szData;
						strData.erase( std::remove( strData.begin(), strData.end(), ' ' ), strData.end() );
						for ( char c : strData )
							nS.seq.Add( c - '0' );
						clip.push_back( nS );
					}
				}
				g_mapKeySequences[keyCode] = midikeyclip_t( iChan, clip );
			}
		}
	}
}

// Handle keyboard input
bool handleKey( bool bKeyDown, SDL_Keycode keySym )
{
	// Quit if escape
	if ( keySym == SDLK_ESCAPE )
		return false;

	// Otherwise map key to action
	auto itKeyNote = g_mapKeyPresses.find( keySym );
	if ( itKeyNote != g_mapKeyPresses.end() )
	{
		MIDI_STATUS eStatus = bKeyDown ? MIDI_STATUS::NOTEON : MIDI_STATUS::NOTEOFF;
		g_Sampler.PostMidiEvent( { (uint8_t) eStatus, itKeyNote->second.iKey, itKeyNote->second.iVel } );
	}
	auto itKeySeq = g_mapKeySequences.find( keySym );
	if ( itKeySeq != g_mapKeySequences.end() )
	{
		midikeyclip_t& mcs = itKeySeq->second;
		if ( mcs.bDown )
		{
			g_Sampler.ClearSequence( mcs.iChan );
		}
		else
		{
			g_Sampler.SetSequence( mcs.iChan, mcs.clip );
		}
		mcs.bDown = !mcs.bDown;
	}

	return true;
}

// audio callback function
// here you have to copy the data of your audio buffer into the
// requesting audio buffer (stream)
// you should only copy as much as the requested length (len)
void ProcessSampler( void *pCustomData, Uint8 *pData, int iLengthInBytes )
{
	static std::vector<float> s_vDeInterleaveBuf;
	if ( UniSampler * pSampler = (UniSampler *) pCustomData )
	{
		// Compute sample length, create deinterleave buf
		int iLength = iLengthInBytes / (sizeof( float ) * g_AudioSpec.channels);
		s_vDeInterleaveBuf.resize( iLength );

		// Compute transport vars
		float fSecPerBuf = iLength / (float) g_Sampler.GetSampleRate();
		float fBeatsPerBuf = (fSecPerBuf / 60.f) * g_PlayState.uTempo;
		float fBeatEnd = g_PlayState.fCurrentPos + fBeatsPerBuf;

		// Process
		pSampler->Process( g_PlayState.fCurrentPos, fBeatEnd, g_PlayState.uTempo, s_vDeInterleaveBuf.data(), iLength, EPlayState::PLAYING );
		
		// Interleave data
		float * pfOutData = (float *) pData;
		for ( int i = 0; i < iLength; i++ )
			for ( int c = 0; c < g_AudioSpec.channels; c++ )
				*pfOutData++ = s_vDeInterleaveBuf[i];

		// Update transport pos
		g_PlayState.fCurrentPos = fBeatEnd;
	}
}
