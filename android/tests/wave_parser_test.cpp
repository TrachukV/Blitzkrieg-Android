// The WAV parser out of bk1_fmod_audio.cpp, on the host.
//
// Samples arrive as a memory block the engine read out of the game archives,
// and this is what turns them into audio. It runs on the audio path for every
// sound in the game, so its failure modes are silence, noise, or a read past
// the end of a buffer -- none of which the device would report clearly.
//
// It links against the real bk1_wave.cpp -- the unit the APK ships -- so a
// change to the parser cannot pass here and fail on the device.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../app/src/main/cpp/compat/bk1_wave.h"

namespace {

int g_nFailures = 0;

void Check( bool bCondition, const char *pszWhat )
{
    if ( !bCondition )
    {
        printf( "  FAIL  %s\n", pszWhat );
        ++g_nFailures;
    }
}

void PutU32( std::vector<unsigned char> *pOut, unsigned int n )
{
    pOut->push_back( (unsigned char)( n & 0xFF ) );
    pOut->push_back( (unsigned char)( ( n >> 8 ) & 0xFF ) );
    pOut->push_back( (unsigned char)( ( n >> 16 ) & 0xFF ) );
    pOut->push_back( (unsigned char)( ( n >> 24 ) & 0xFF ) );
}

void PutU16( std::vector<unsigned char> *pOut, unsigned int n )
{
    pOut->push_back( (unsigned char)( n & 0xFF ) );
    pOut->push_back( (unsigned char)( ( n >> 8 ) & 0xFF ) );
}

void PutTag( std::vector<unsigned char> *pOut, const char *pszTag )
{
    for ( int i = 0; i < 4; ++i )
        pOut->push_back( (unsigned char)pszTag[i] );
}

// Builds a wave file. bExtraChunk puts a LIST between fmt and data, which real
// files carry and a parser that assumes a fixed layout would trip on.
std::vector<unsigned char> BuildWave( int nBits, int nChannels, int nRate,
                                      const std::vector<unsigned char> &audio,
                                      bool bExtraChunk )
{
    std::vector<unsigned char> body;

    PutTag( &body, "fmt " );
    PutU32( &body, 16 );
    PutU16( &body, 1 );                                    // PCM
    PutU16( &body, nChannels );
    PutU32( &body, nRate );
    PutU32( &body, nRate * nChannels * ( nBits / 8 ) );     // byte rate
    PutU16( &body, nChannels * ( nBits / 8 ) );             // block align
    PutU16( &body, nBits );

    if ( bExtraChunk )
    {
        PutTag( &body, "LIST" );
        PutU32( &body, 5 );                                 // odd, so it pads
        for ( int i = 0; i < 5; ++i )
            body.push_back( 'x' );
        body.push_back( 0 );                                // the pad byte
    }

    PutTag( &body, "data" );
    PutU32( &body, (unsigned int)audio.size() );
    body.insert( body.end(), audio.begin(), audio.end() );

    std::vector<unsigned char> file;
    PutTag( &file, "RIFF" );
    PutU32( &file, (unsigned int)( body.size() + 4 ) );
    PutTag( &file, "WAVE" );
    file.insert( file.end(), body.begin(), body.end() );
    return file;
}

void TestSixteenBitMono()
{
    printf( "16-bit mono\n" );
    std::vector<unsigned char> audio;
    const short expected[] = { 0, 1000, -1000, 32767, -32768 };
    for ( int i = 0; i < 5; ++i )
        PutU16( &audio, (unsigned short)expected[i] );

    const std::vector<unsigned char> file = BuildWave( 16, 1, 22050, audio, false );
    SWaveData wave;
    Check( ParseWave( &file[0], file.size(), &wave ), "parses" );
    Check( wave.nChannels == 1, "one channel" );
    Check( wave.nRate == 22050, "rate carried through" );
    Check( wave.samples.size() == 5, "five frames" );
    for ( size_t i = 0; i < wave.samples.size() && i < 5; ++i )
        Check( wave.samples[i] == expected[i], "sample value preserved exactly" );
}

void TestEightBitIsCentred()
{
    printf( "8-bit is unsigned with 128 as silence\n" );
    std::vector<unsigned char> audio;
    audio.push_back( 128 );      // silence
    audio.push_back( 255 );      // full positive
    audio.push_back( 0 );        // full negative

    const std::vector<unsigned char> file = BuildWave( 8, 1, 44100, audio, false );
    SWaveData wave;
    Check( ParseWave( &file[0], file.size(), &wave ), "parses" );
    Check( wave.samples.size() == 3, "three frames" );
    if ( wave.samples.size() == 3 )
    {
        Check( wave.samples[0] == 0, "128 becomes silence, not a DC offset" );
        Check( wave.samples[1] > 30000, "255 is loud positive" );
        Check( wave.samples[2] < -30000, "0 is loud negative" );
    }
}

void TestStereoInterleaving()
{
    printf( "stereo stays interleaved and in order\n" );
    std::vector<unsigned char> audio;
    // L R L R, distinguishable so a swapped channel is visible.
    const short expected[] = { 100, -100, 200, -200 };
    for ( int i = 0; i < 4; ++i )
        PutU16( &audio, (unsigned short)expected[i] );

    const std::vector<unsigned char> file = BuildWave( 16, 2, 44100, audio, false );
    SWaveData wave;
    Check( ParseWave( &file[0], file.size(), &wave ), "parses" );
    Check( wave.nChannels == 2, "two channels" );
    Check( wave.samples.size() == 4, "four values, two frames" );
    for ( size_t i = 0; i < wave.samples.size() && i < 4; ++i )
        Check( wave.samples[i] == expected[i], "left and right not swapped" );
}

void TestChunkWalk()
{
    printf( "a chunk between fmt and data, with odd-size padding\n" );
    std::vector<unsigned char> audio;
    for ( int i = 0; i < 8; ++i )
        PutU16( &audio, (unsigned short)(short)( i * 1000 ) );

    const std::vector<unsigned char> file = BuildWave( 16, 1, 11025, audio, true );
    SWaveData wave;
    Check( ParseWave( &file[0], file.size(), &wave ), "parses past the extra chunk" );
    Check( wave.nRate == 11025, "still found fmt" );
    Check( wave.samples.size() == 8, "still found data" );
    if ( wave.samples.size() == 8 )
        Check( wave.samples[7] == 7000, "data not shifted by the pad byte" );
}

void TestTruncatedFileDoesNotOverrun()
{
    printf( "a truncated file is rejected, not read past the end\n" );
    std::vector<unsigned char> audio;
    for ( int i = 0; i < 64; ++i )
        PutU16( &audio, 1234 );
    std::vector<unsigned char> file = BuildWave( 16, 1, 44100, audio, false );

    // The header still claims the full length; the file no longer has it. A
    // parser that trusts the declared size reads off the end of the buffer.
    const size_t nFull = file.size();
    file.resize( nFull - 60 );

    SWaveData wave;
    const bool bParsed = ParseWave( &file[0], file.size(), &wave );
    // Either answer is defensible -- what must not happen is reading beyond
    // what was handed in, so the check is that it stayed inside.
    if ( bParsed )
        Check( wave.samples.size() * 2 <= file.size(), "read no more than the buffer holds" );
    else
        Check( true, "rejected" );
}

void TestGarbageIsRejected()
{
    printf( "not a wave at all\n" );
    SWaveData wave;
    const unsigned char szGarbage[] = "this is not a RIFF file at all, not even close";
    Check( !ParseWave( szGarbage, sizeof( szGarbage ), &wave ), "rejects non-RIFF" );
    Check( !ParseWave( 0, 0, &wave ), "rejects null" );

    const unsigned char szShort[] = { 'R', 'I', 'F', 'F' };
    Check( !ParseWave( szShort, sizeof( szShort ), &wave ), "rejects a stub" );

    // RIFF and WAVE, but no data chunk.
    std::vector<unsigned char> file;
    PutTag( &file, "RIFF" );
    PutU32( &file, 4 );
    PutTag( &file, "WAVE" );
    Check( !ParseWave( &file[0], file.size(), &wave ), "rejects a wave with no data" );
}

}   // anonymous namespace

int main()
{
    printf( "wave parser\n\n" );
    TestSixteenBitMono();
    TestEightBitIsCentred();
    TestStereoInterleaving();
    TestChunkWalk();
    TestTruncatedFileDoesNotOverrun();
    TestGarbageIsRejected();

    printf( "\n%s\n", g_nFailures == 0 ? "all passed" : "FAILURES" );
    return g_nFailures == 0 ? 0 : 1;
}
