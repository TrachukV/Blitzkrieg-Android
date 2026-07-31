#include "StdAfx.h"

#include "FileSystem.h"
#include "ZipFileSystem.h"
#include "MemFileSystem.h"
#include "CommonFileSystem.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// These duplicate CSaveLoadSystem::OpenStorage and ::CreateStorage, minus
// STORAGE_TYPE_MOD, and no header declares them, so nothing outside this file
// can call them. On x86 MSVC they coexisted with the inline forwarders in
// StructureSaver.h because __stdcall made them a different type; arm64 has no
// calling conventions to tell them apart, so there they are a redefinition.
#if defined( _MSC_VER ) && defined( _M_IX86 )
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IDataStorage* STDCALL OpenStorage( const char *pszName, DWORD dwAccessMode, DWORD type )
{
	switch ( type )
	{
		case STORAGE_TYPE_COMMON:
			return new CCommonFileSystem( pszName, dwAccessMode );
		case STORAGE_TYPE_FILE:
			return new CFileSystem( pszName, dwAccessMode, false );
		case STORAGE_TYPE_ZIP:
			return new CZipFileSystem( pszName, dwAccessMode );
		case STORAGE_TYPE_MEM:
			return new CMemFileSystem( dwAccessMode );
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IDataStorage* STDCALL CreateStorage( const char *pszName, DWORD dwAccessMode, DWORD type )
{
	switch ( type )
	{
		case STORAGE_TYPE_FILE:
			return new CFileSystem( pszName, dwAccessMode, true );
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
