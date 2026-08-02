#ifndef __SAVELOADCOMMON_H__
#define __SAVELOADCOMMON_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma ONCE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "..\Misc\FileUtils.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLoadFileDesc
{
	std::string szFileName;
	FILETIME time;
	int nSize;
	//
	SLoadFileDesc() {  }
	SLoadFileDesc( const std::string &_szFileName, const FILETIME &_time, const int _nSize )
		: szFileName( _szFileName ), time( _time ), nSize( _nSize ) {  }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CGetFiles2Load
{
	std::vector<SLoadFileDesc> &files;
	std::string szPath;
public:
	CGetFiles2Load( std::vector<SLoadFileDesc> &_files, const std::string &_szPath )
		: files( _files ), szPath( _szPath ) {  }
	//
	bool operator()( const NFile::CFileIterator &it )
	{
		if ( !it.IsDirectory() && (it.GetLength() > 1024) )
		{
			std::string szFileName = it.GetFilePath();
#ifndef _MSC_VER
			// Cutting the directory off by its length assumes the path handed back
			// matches the one asked for character for character. On Android it does
			// not -- the host resolves separators and folds case -- and the name
			// came out with a stray separator in front of it: "\GERMAN Mission..."
			// where the list should read "GERMAN Mission...".
			//
			// The last separator does not care about either, so use that instead.
			{
				const size_t nCut = NFile::FindLastSeparator( szFileName );
				if ( nCut != std::string::npos )
					szFileName = szFileName.substr( nCut + 1 );
			}
#else
			NI_ASSERT_T( szFileName.size() > szPath.size(), "Wrong name size" );
			// ������� ��������� ����
			szFileName = szFileName.substr( szPath.size() );
#endif
/*
			// ������� extension
			szFileName = szFileName.substr( 0, szFileName.rfind('.') );
*/
			//
			files.push_back( SLoadFileDesc(szFileName, it.GetLastWriteTime(), it.GetLength()) );
		}
		return true;
	}
};
struct SLoadFileLessFunctional
{
	bool operator()( const SLoadFileDesc &f1, const SLoadFileDesc &f2 ) const { return CompareFileTime( &f1.time, &f2.time ) == 1; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __SAVELOADCOMMON_H__