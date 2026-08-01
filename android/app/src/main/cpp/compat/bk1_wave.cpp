// WAV, decoded from memory. Declared in bk1_wave.h, which says why it is its
// own unit rather than part of the audio device.
#include "bk1_wave.h"

#include <string.h>

namespace {

uint32_t ReadU32( const unsigned char *p ) { return p[0] | ( p[1] << 8 ) | ( p[2] << 16 ) | ( (uint32_t)p[3] << 24 ); }
uint16_t ReadU16( const unsigned char *p ) { return (uint16_t)( p[0] | ( p[1] << 8 ) ); }

// Walks the RIFF chunks rather than assuming fmt comes first and data second,
// because plenty of tools write a LIST or a fact chunk in between.
bool ParseWaveInternal( const unsigned char *pData, size_t nLength, SWaveData *pOut )
{
    if ( pData == 0 || nLength < 12 )
        return false;
    if ( memcmp( pData, "RIFF", 4 ) != 0 || memcmp( pData + 8, "WAVE", 4 ) != 0 )
        return false;

    int nFormat = 0, nChannels = 0, nRate = 0, nBits = 0;
    const unsigned char *pAudio = 0;
    size_t nAudioBytes = 0;

    size_t nOffset = 12;
    while ( nOffset + 8 <= nLength )
    {
        const uint32_t nChunkSize = ReadU32( pData + nOffset + 4 );
        const unsigned char *pBody = pData + nOffset + 8;
        // A chunk claiming more than the file holds is a truncated file; take
        // what is actually there rather than reading past the end.
        const size_t nAvailable = nLength - ( nOffset + 8 );
        const size_t nUsable = ( nChunkSize <= nAvailable ) ? nChunkSize : nAvailable;

        if ( memcmp( pData + nOffset, "fmt ", 4 ) == 0 && nUsable >= 16 )
        {
            nFormat = ReadU16( pBody );
            nChannels = ReadU16( pBody + 2 );
            nRate = (int)ReadU32( pBody + 4 );
            nBits = ReadU16( pBody + 14 );
        }
        else if ( memcmp( pData + nOffset, "data", 4 ) == 0 )
        {
            pAudio = pBody;
            nAudioBytes = nUsable;
        }

        // Chunks are word-aligned; an odd size is followed by a pad byte.
        nOffset += 8 + nChunkSize + ( nChunkSize & 1 );
        if ( nChunkSize > nAvailable )
            break;
    }

    if ( pAudio == 0 || nChannels <= 0 || nChannels > 2 || nRate <= 0 )
        return false;
    // 1 is PCM, 0xFFFE is WAVE_FORMAT_EXTENSIBLE, which for these files is
    // still PCM with a longer header.
    if ( nFormat != 1 && nFormat != 0xFFFE )
    {
        return false;
    }

    pOut->nChannels = nChannels;
    pOut->nRate = nRate;

    if ( nBits == 16 )
    {
        const size_t nCount = nAudioBytes / 2;
        pOut->samples.resize( nCount );
        for ( size_t i = 0; i < nCount; ++i )
            pOut->samples[i] = (int16_t)ReadU16( pAudio + i * 2 );
    }
    else if ( nBits == 8 )
    {
        // 8-bit wave is unsigned with 128 as silence.
        pOut->samples.resize( nAudioBytes );
        for ( size_t i = 0; i < nAudioBytes; ++i )
            pOut->samples[i] = (int16_t)( ( (int)pAudio[i] - 128 ) << 8 );
    }
    else if ( nBits == 24 )
    {
        const size_t nCount = nAudioBytes / 3;
        pOut->samples.resize( nCount );
        for ( size_t i = 0; i < nCount; ++i )
            pOut->samples[i] = (int16_t)( pAudio[i * 3 + 1] | ( pAudio[i * 3 + 2] << 8 ) );
    }
    else
    {
        return false;
    }
    return true;
}

}   // anonymous namespace

bool ParseWave( const unsigned char *pData, size_t nLength, SWaveData *pOut )
{
    return ParseWaveInternal( pData, nLength, pOut );
}
