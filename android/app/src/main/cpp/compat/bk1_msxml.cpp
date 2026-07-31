// The XML parser, serialiser and DOM behind bk1_msxml.h.
//
// The parser is byte-transparent. The game's XML is written in a Windows ANSI
// code page, and the engine reads it back as narrow bytes and hands it to the
// same code-page conversions as the rest of its text, so transcoding here
// would corrupt it.
#include "bk1_msxml.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace NBk1Xml {

// ---------------------------------------------------------------------------
// The document owns every node and every node list beneath it, and holds the
// reference count that the smart pointers manipulate.
// ---------------------------------------------------------------------------
struct SDocument
{
    int                     nRefCount;
    std::vector<SNode *>    owned;
    std::vector<SNodeList *> ownedLists;

    SDocument() : nRefCount( 0 ) {}

    ~SDocument()
    {
        for ( size_t i = 0; i < owned.size(); ++i )
            delete owned[i];
        for ( size_t i = 0; i < ownedLists.size(); ++i )
            delete ownedLists[i];
    }

    SNode *NewNode( ENodeType eType )
    {
        SNode *p = new SNode( eType, this );
        owned.push_back( p );
        return p;
    }

    SNodeList *NewList()
    {
        SNodeList *p = new SNodeList( this );
        ownedLists.push_back( p );
        return p;
    }
};

void AddRefDocument( SDocument *pDoc )
{
    if ( pDoc != 0 )
        ++pDoc->nRefCount;
}

void ReleaseDocument( SDocument *pDoc )
{
    if ( pDoc != 0 && --pDoc->nRefCount <= 0 )
        delete pDoc;
}

SDocument *DocumentOf( SNode *pNode ) { return pNode != 0 ? pNode->pDoc : 0; }
SDocument *DocumentOf( SNodeList *pList ) { return pList != 0 ? pList->pDoc : 0; }

SNode::SNode( ENodeType e, SDocument *_pDoc )
    : eType( e ), pParent( 0 ), pDoc( _pDoc ), async( false ), pChildCache( 0 )
{
    text.pOwner = this;
    nodeName.pOwner = this;
    attributes.pOwner = this;
    childNodes.pOwner = this;
}

SNode::~SNode() {}

namespace {

// ---------------------------------------------------------------------------
// Lexical helpers
// ---------------------------------------------------------------------------
bool IsSpace( char c ) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

void SkipSpace( const std::string &s, size_t *pi )
{
    while ( *pi < s.size() && IsSpace( s[*pi] ) )
        ++( *pi );
}

void AppendCodePoint( std::string *pOut, unsigned int cp )
{
    // A single-byte code page is what the data is written in, so a reference
    // that fits in one byte is emitted as that byte rather than as UTF-8.
    if ( cp < 0x100 )
    {
        pOut->push_back( (char)(unsigned char)cp );
        return;
    }
    if ( cp < 0x800 )
    {
        pOut->push_back( (char)( 0xC0 | ( cp >> 6 ) ) );
        pOut->push_back( (char)( 0x80 | ( cp & 0x3F ) ) );
        return;
    }
    pOut->push_back( (char)( 0xE0 | ( cp >> 12 ) ) );
    pOut->push_back( (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
    pOut->push_back( (char)( 0x80 | ( cp & 0x3F ) ) );
}

std::string DecodeEntities( const std::string &s )
{
    if ( s.find( '&' ) == std::string::npos )
        return s;                       // the common case, untouched

    std::string res;
    res.reserve( s.size() );
    for ( size_t i = 0; i < s.size(); ++i )
    {
        if ( s[i] != '&' )
        {
            res.push_back( s[i] );
            continue;
        }
        const size_t nEnd = s.find( ';', i );
        if ( nEnd == std::string::npos || nEnd - i > 12 )
        {
            res.push_back( s[i] );
            continue;
        }
        const std::string szName = s.substr( i + 1, nEnd - i - 1 );
        if ( szName == "amp" )
            res.push_back( '&' );
        else if ( szName == "lt" )
            res.push_back( '<' );
        else if ( szName == "gt" )
            res.push_back( '>' );
        else if ( szName == "quot" )
            res.push_back( '"' );
        else if ( szName == "apos" )
            res.push_back( '\'' );
        else if ( szName.size() > 1 && szName[0] == '#' )
        {
            const unsigned int cp =
                ( szName[1] == 'x' || szName[1] == 'X' )
                    ? (unsigned int)strtoul( szName.c_str() + 2, 0, 16 )
                    : (unsigned int)strtoul( szName.c_str() + 1, 0, 10 );
            AppendCodePoint( &res, cp );
        }
        else
        {
            // an entity this document did not declare: left as written
            res.push_back( s[i] );
            continue;
        }
        i = nEnd;
    }
    return res;
}

std::string EncodeEntities( const std::string &s, bool bAttribute )
{
    std::string res;
    res.reserve( s.size() );
    for ( size_t i = 0; i < s.size(); ++i )
    {
        switch ( s[i] )
        {
        case '&': res += "&amp;"; break;
        case '<': res += "&lt;";  break;
        case '>': res += "&gt;";  break;
        case '"': if ( bAttribute ) res += "&quot;"; else res.push_back( s[i] ); break;
        default:  res.push_back( s[i] ); break;
        }
    }
    return res;
}

bool IsNameChar( char c )
{
    return !IsSpace( c ) && c != '>' && c != '/' && c != '=' && c != '<' &&
           c != '"' && c != '\'' && c != 0;
}

std::string ReadName( const std::string &s, size_t *pi )
{
    const size_t nStart = *pi;
    while ( *pi < s.size() && IsNameChar( s[*pi] ) )
        ++( *pi );
    return s.substr( nStart, *pi - nStart );
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------
SNode *ParseElement( SDocument *pDoc, const std::string &s, size_t *pi );

// Handles whatever follows a '<'. Returns the node produced, or 0 when the
// construct produces none; *pbClose is set when it was a closing tag.
SNode *ParseMarkup( SDocument *pDoc, const std::string &s, size_t *pi, bool *pbClose )
{
    *pbClose = false;
    if ( *pi >= s.size() )
        return 0;

    if ( s.compare( *pi, 3, "!--" ) == 0 )
    {
        const size_t nEnd = s.find( "-->", *pi );
        *pi = ( nEnd == std::string::npos ) ? s.size() : nEnd + 3;
        return 0;
    }
    if ( s[*pi] == '!' )
    {
        // DOCTYPE and friends; CDATA is handled by the content loop
        const size_t nEnd = s.find( '>', *pi );
        *pi = ( nEnd == std::string::npos ) ? s.size() : nEnd + 1;
        return 0;
    }
    if ( s[*pi] == '?' )
    {
        ++( *pi );
        const size_t nEnd = s.find( "?>", *pi );
        const std::string szBody =
            s.substr( *pi, ( nEnd == std::string::npos ) ? 0 : nEnd - *pi );
        *pi = ( nEnd == std::string::npos ) ? s.size() : nEnd + 2;

        SNode *pPI = pDoc->NewNode( XML_PI );
        const size_t nSpace = szBody.find_first_of( " \t\r\n" );
        if ( nSpace == std::string::npos )
        {
            pPI->szName = szBody;
        }
        else
        {
            pPI->szName = szBody.substr( 0, nSpace );
            pPI->szValue = szBody.substr( nSpace + 1 );
        }
        return pPI;
    }
    if ( s[*pi] == '/' )
    {
        const size_t nEnd = s.find( '>', *pi );
        *pi = ( nEnd == std::string::npos ) ? s.size() : nEnd + 1;
        *pbClose = true;
        return 0;
    }
    return ParseElement( pDoc, s, pi );
}

SNode *ParseElement( SDocument *pDoc, const std::string &s, size_t *pi )
{
    SNode *pElem = pDoc->NewNode( XML_ELEMENT );
    pElem->szName = ReadName( s, pi );

    // --- attributes ---
    for ( ;; )
    {
        SkipSpace( s, pi );
        if ( *pi >= s.size() || s[*pi] == '>' || s[*pi] == '/' )
            break;

        const std::string szAttr = ReadName( s, pi );
        if ( szAttr.empty() )
        {
            ++( *pi );          // nothing consumable here; step past it
            continue;
        }

        SkipSpace( s, pi );
        std::string szValue;
        if ( *pi < s.size() && s[*pi] == '=' )
        {
            ++( *pi );
            SkipSpace( s, pi );
            if ( *pi < s.size() && ( s[*pi] == '"' || s[*pi] == '\'' ) )
            {
                const char chQuote = s[*pi];
                const size_t nStart = ++( *pi );
                const size_t nEnd = s.find( chQuote, nStart );
                szValue = s.substr( nStart,
                                    ( nEnd == std::string::npos ) ? 0 : nEnd - nStart );
                *pi = ( nEnd == std::string::npos ) ? s.size() : nEnd + 1;
            }
            else
            {
                szValue = ReadName( s, pi );
            }
        }

        SNode *pAttr = pDoc->NewNode( XML_ATTRIBUTE );
        pAttr->szName = szAttr;
        pAttr->szValue = DecodeEntities( szValue );
        pAttr->pParent = pElem;
        pElem->attributes_.push_back( pAttr );
    }

    if ( *pi < s.size() && s[*pi] == '/' )
    {
        const size_t nEnd = s.find( '>', *pi );
        *pi = ( nEnd == std::string::npos ) ? s.size() : nEnd + 1;
        return pElem;                       // empty element
    }
    if ( *pi < s.size() && s[*pi] == '>' )
        ++( *pi );

    // --- content ---
    std::string szText;
    while ( *pi < s.size() )
    {
        if ( s[*pi] != '<' )
        {
            const size_t nStart = *pi;
            while ( *pi < s.size() && s[*pi] != '<' )
                ++( *pi );
            szText += DecodeEntities( s.substr( nStart, *pi - nStart ) );
            continue;
        }

        // CDATA contributes to this element's text verbatim
        if ( s.compare( *pi + 1, 8, "![CDATA[" ) == 0 )
        {
            const size_t nStart = *pi + 9;
            const size_t nEnd = s.find( "]]>", nStart );
            szText += s.substr( nStart, ( nEnd == std::string::npos ) ? 0 : nEnd - nStart );
            *pi = ( nEnd == std::string::npos ) ? s.size() : nEnd + 3;
            continue;
        }

        ++( *pi );
        bool bClose = false;
        SNode *pChild = ParseMarkup( pDoc, s, pi, &bClose );
        if ( bClose )
            break;
        if ( pChild != 0 )
        {
            pChild->pParent = pElem;
            pElem->children.push_back( pChild );
        }
    }

    if ( !szText.empty() )
    {
        SNode *pText = pDoc->NewNode( XML_TEXT );
        pText->szName = "#text";
        pText->szValue = szText;
        pText->pParent = pElem;
        pElem->children.push_back( pText );
    }
    return pElem;
}

// ---------------------------------------------------------------------------
// Serialising
// ---------------------------------------------------------------------------
void Serialise( const SNode *pNode, std::string *pOut )
{
    switch ( pNode->eType )
    {
    case XML_TEXT:
        *pOut += EncodeEntities( pNode->szValue, false );
        return;

    case XML_PI:
        *pOut += "<?";
        *pOut += pNode->szName;
        if ( !pNode->szValue.empty() )
        {
            *pOut += " ";
            *pOut += pNode->szValue;
        }
        *pOut += "?>\n";
        return;

    case XML_DOCUMENT:
        for ( size_t i = 0; i < pNode->children.size(); ++i )
            Serialise( pNode->children[i], pOut );
        return;

    case XML_ATTRIBUTE:
        return;                             // written by the owning element

    case XML_ELEMENT:
    default:
        break;
    }

    *pOut += "<";
    *pOut += pNode->szName;
    for ( size_t i = 0; i < pNode->attributes_.size(); ++i )
    {
        *pOut += " ";
        *pOut += pNode->attributes_[i]->szName;
        *pOut += "=\"";
        *pOut += EncodeEntities( pNode->attributes_[i]->szValue, true );
        *pOut += "\"";
    }
    if ( pNode->children.empty() )
    {
        *pOut += "/>";
        return;
    }
    *pOut += ">";
    for ( size_t i = 0; i < pNode->children.size(); ++i )
        Serialise( pNode->children[i], pOut );
    *pOut += "</";
    *pOut += pNode->szName;
    *pOut += ">";
}

// ---------------------------------------------------------------------------
// Path lookup. MSXML takes XPath; the engine only ever passes a chain of child
// element names, such as "chunk" or "chunk/item".
// ---------------------------------------------------------------------------
void SplitPath( const char *pszPath, std::vector<std::string> *pParts )
{
    if ( pszPath == 0 )
        return;
    const std::string szPath = pszPath;
    size_t nStart = 0;
    for ( ;; )
    {
        const size_t nSlash = szPath.find( '/', nStart );
        const std::string szPart = szPath.substr(
            nStart, ( nSlash == std::string::npos ) ? std::string::npos : nSlash - nStart );
        if ( !szPart.empty() && szPart != "." )
            pParts->push_back( szPart );
        if ( nSlash == std::string::npos )
            break;
        nStart = nSlash + 1;
    }
}

void CollectMatches( const SNode *pNode, const std::vector<std::string> &parts,
                     size_t nDepth, std::vector<SNode *> *pOut )
{
    if ( nDepth >= parts.size() )
        return;
    const bool bLast = ( nDepth + 1 == parts.size() );
    for ( size_t i = 0; i < pNode->children.size(); ++i )
    {
        SNode *pChild = pNode->children[i];
        if ( pChild->eType != XML_ELEMENT || pChild->szName != parts[nDepth] )
            continue;
        if ( bLast )
            pOut->push_back( pChild );
        else
            CollectMatches( pChild, parts, nDepth + 1, pOut );
    }
}

}   // anonymous namespace

// ---------------------------------------------------------------------------
// Text and name properties
// ---------------------------------------------------------------------------
std::string SNode::GatherText() const
{
    if ( eType == XML_TEXT || eType == XML_ATTRIBUTE || eType == XML_PI )
        return szValue;
    std::string res;
    for ( size_t i = 0; i < children.size(); ++i )
        res += children[i]->GatherText();
    return res;
}

void SNode::SetText( const char *psz )
{
    if ( eType == XML_ATTRIBUTE || eType == XML_TEXT || eType == XML_PI )
    {
        szValue = ( psz != 0 ) ? psz : "";
        return;
    }
    // an element's text replaces its character data, as MSXML's does
    children.clear();
    SNode *pText = pDoc->NewNode( XML_TEXT );
    pText->szName = "#text";
    pText->szValue = ( psz != 0 ) ? psz : "";
    pText->pParent = this;
    children.push_back( pText );
}

std::string STextProperty::str() const
{
    return ( pOwner == 0 ) ? std::string() : pOwner->GatherText();
}

STextProperty::operator const char *() const
{
    if ( pOwner == 0 )
        return "";
    // cached on the node so the pointer outlives the expression reading it
    pOwner->szTextCache = pOwner->GatherText();
    return pOwner->szTextCache.c_str();
}

STextProperty::operator _bstr_t() const
{
    return _bstr_t( str().c_str() );
}

STextProperty::operator std::string() const
{
    return str();
}

const unsigned short *STextProperty::Utf16() const
{
    static const unsigned short EMPTY[1] = { 0 };
    if ( pOwner == 0 )
        return EMPTY;

    const std::string szNarrow = pOwner->GatherText();
    const UINT nPage = GetACP();
    const int nUnits = Bk1AnsiToUtf16( nPage, szNarrow.c_str(), (int)szNarrow.size(),
                                       0, 0 );
    pOwner->wideTextCache.assign( (size_t)nUnits + 1, 0 );
    if ( nUnits > 0 )
    {
        Bk1AnsiToUtf16( nPage, szNarrow.c_str(), (int)szNarrow.size(),
                        &pOwner->wideTextCache[0], nUnits );
    }
    pOwner->wideTextCache[nUnits] = 0;
    return &pOwner->wideTextCache[0];
}

STextProperty &STextProperty::operator=( const unsigned short *psz )
{
    if ( pOwner == 0 )
        return *this;

    const UINT nPage = GetACP();
    const int nLen = Bk1Utf16Len( psz );
    const int nBytes = Bk1Utf16ToAnsi( nPage, psz, nLen, 0, 0 );
    std::string szNarrow;
    szNarrow.resize( (size_t)nBytes );
    if ( nBytes > 0 )
        Bk1Utf16ToAnsi( nPage, psz, nLen, &szNarrow[0], nBytes );
    pOwner->SetText( szNarrow.c_str() );
    return *this;
}

STextProperty &STextProperty::operator=( const char *psz )
{
    if ( pOwner != 0 )
        pOwner->SetText( psz );
    return *this;
}

STextProperty &STextProperty::operator=( const std::string &sz )
{
    return operator=( sz.c_str() );
}

STextProperty &STextProperty::operator=( const _bstr_t &str )
{
    return operator=( (const char *)str );
}

STextProperty &STextProperty::operator=( const STextProperty &other )
{
    if ( this != &other )
        operator=( other.str().c_str() );
    return *this;
}

std::string SNameProperty::str() const
{
    return ( pOwner == 0 ) ? std::string() : pOwner->szName;
}

SNameProperty::operator const char *() const
{
    return ( pOwner == 0 ) ? "" : pOwner->szName.c_str();
}

SNameProperty::operator _bstr_t() const
{
    return _bstr_t( ( pOwner == 0 ) ? "" : pOwner->szName.c_str() );
}

SNameProperty::operator std::string() const
{
    return str();
}

bool SNameProperty::operator==( const char *psz ) const
{
    return psz != 0 && str() == psz;
}

bool SNameProperty::operator!=( const char *psz ) const
{
    return !operator==( psz );
}

// ---------------------------------------------------------------------------
// Node lists, attribute maps, child lists
// ---------------------------------------------------------------------------
CBk1XmlNodePtr SItemProperty::operator[]( int nIndex ) const
{
    if ( pOwner == 0 || nIndex < 0 || nIndex >= (int)pOwner->items.size() )
        return CBk1XmlNodePtr();
    return CBk1XmlNodePtr( pOwner->items[nIndex] );
}

CBk1XmlNodePtr SAttrMap::getNamedItem( const char *pszName ) const
{
    if ( pOwner == 0 || pszName == 0 )
        return CBk1XmlNodePtr();
    for ( size_t i = 0; i < pOwner->attributes_.size(); ++i )
    {
        if ( pOwner->attributes_[i]->szName == pszName )
            return CBk1XmlNodePtr( pOwner->attributes_[i] );
    }
    return CBk1XmlNodePtr();
}

void SAttrMap::Refresh()
{
    length = ( pOwner == 0 ) ? 0 : (int)pOwner->attributes_.size();
}

CBk1XmlNodePtr SAttrItemProperty::operator[]( int nIndex ) const
{
    if ( pOwner == 0 || pOwner->pOwner == 0 )
        return CBk1XmlNodePtr();
    const std::vector<SNode *> &attrs = pOwner->pOwner->attributes_;
    if ( nIndex < 0 || nIndex >= (int)attrs.size() )
        return CBk1XmlNodePtr();
    return CBk1XmlNodePtr( attrs[nIndex] );
}

SNodeList *SChildNodes::operator->()
{
    if ( pOwner == 0 )
        return 0;
    if ( pOwner->pChildCache == 0 )
        pOwner->pChildCache = pOwner->pDoc->NewList();
    pOwner->pChildCache->items.assign( pOwner->children.begin(), pOwner->children.end() );
    pOwner->pChildCache->Rebuild();
    return pOwner->pChildCache;
}

const SNodeList *SChildNodes::operator->() const
{
    return const_cast<SChildNodes *>( this )->operator->();
}

SChildNodes::operator CBk1XmlNodeListPtr() const
{
    // A list of its own, so the caller's handle keeps working even after the
    // node's cached child list is rebuilt by a later access.
    if ( pOwner == 0 )
        return CBk1XmlNodeListPtr();
    SNodeList *pList = pOwner->pDoc->NewList();
    pList->items.assign( pOwner->children.begin(), pOwner->children.end() );
    pList->Rebuild();
    return CBk1XmlNodeListPtr( pList );
}

// ---------------------------------------------------------------------------
// Tree operations
// ---------------------------------------------------------------------------
CBk1XmlNodePtr SNode::appendChild( const CBk1XmlNodePtr &child )
{
    SNode *p = (SNode *)child;
    if ( p != 0 )
    {
        p->pParent = this;
        children.push_back( p );
    }
    return child;
}

CBk1XmlNodePtr SNode::removeChild( const CBk1XmlNodePtr &child )
{
    SNode *p = (SNode *)child;
    for ( size_t i = 0; i < children.size(); ++i )
    {
        if ( children[i] == p )
        {
            children.erase( children.begin() + i );
            if ( p != 0 )
                p->pParent = 0;
            break;
        }
    }
    return child;
}

CBk1XmlNodePtr SNode::selectSingleNode( const char *pszPath ) const
{
    std::vector<std::string> parts;
    SplitPath( pszPath, &parts );
    if ( parts.empty() )
        return CBk1XmlNodePtr();
    std::vector<SNode *> found;
    CollectMatches( this, parts, 0, &found );
    return found.empty() ? CBk1XmlNodePtr() : CBk1XmlNodePtr( found[0] );
}

CBk1XmlNodeListPtr SNode::selectNodes( const char *pszPath ) const
{
    // The list belongs to the document, so it lives exactly as long as the
    // tree the engine is walking.
    SNodeList *pList = pDoc->NewList();
    std::vector<std::string> parts;
    SplitPath( pszPath, &parts );
    if ( !parts.empty() )
        CollectMatches( this, parts, 0, &pList->items );
    pList->Rebuild();
    return CBk1XmlNodeListPtr( pList );
}

// ---------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------
void SNode::setAttributeText( const char *pszName, const std::string &szValue )
{
    if ( pszName == 0 )
        return;
    for ( size_t i = 0; i < attributes_.size(); ++i )
    {
        if ( attributes_[i]->szName == pszName )
        {
            attributes_[i]->szValue = szValue;
            return;
        }
    }
    SNode *pAttr = pDoc->NewNode( XML_ATTRIBUTE );
    pAttr->szName = pszName;
    pAttr->szValue = szValue;
    pAttr->pParent = this;
    attributes_.push_back( pAttr );
}

// ---------------------------------------------------------------------------
// Attribute value spelling
// ---------------------------------------------------------------------------
namespace {

std::string FormatValue( const char *pszFormat, ... )
{
    char buff[64];
    va_list va;
    va_start( va, pszFormat );
    vsnprintf( buff, sizeof( buff ), pszFormat, va );
    va_end( va );
    return buff;
}

}   // anonymous namespace

std::string Bk1XmlValueToText( const char *pszValue )
{
    return ( pszValue != 0 ) ? std::string( pszValue ) : std::string();
}

std::string Bk1XmlValueToText( char *pszValue )
{
    return Bk1XmlValueToText( (const char *)pszValue );
}

std::string Bk1XmlValueToText( const std::string &szValue ) { return szValue; }

// The engine writes booleans as 1 and 0 and reads them back with the integer
// readers, so they are spelled that way rather than as "true"/"false".
std::string Bk1XmlValueToText( bool bValue ) { return bValue ? "1" : "0"; }

std::string Bk1XmlValueToText( char nValue ) { return FormatValue( "%d", (int)nValue ); }
std::string Bk1XmlValueToText( unsigned char nValue ) { return FormatValue( "%u", (unsigned)nValue ); }
std::string Bk1XmlValueToText( short nValue ) { return FormatValue( "%d", (int)nValue ); }
std::string Bk1XmlValueToText( unsigned short nValue ) { return FormatValue( "%u", (unsigned)nValue ); }
std::string Bk1XmlValueToText( int nValue ) { return FormatValue( "%d", nValue ); }
std::string Bk1XmlValueToText( unsigned int nValue ) { return FormatValue( "%u", nValue ); }
std::string Bk1XmlValueToText( long nValue ) { return FormatValue( "%ld", nValue ); }
std::string Bk1XmlValueToText( unsigned long nValue ) { return FormatValue( "%lu", nValue ); }
std::string Bk1XmlValueToText( long long nValue ) { return FormatValue( "%lld", nValue ); }
std::string Bk1XmlValueToText( float fValue ) { return FormatValue( "%g", (double)fValue ); }
std::string Bk1XmlValueToText( double fValue ) { return FormatValue( "%g", fValue ); }

CBk1XmlNodePtr SNode::getAttributeNode( const char *pszName ) const
{
    return attributes.getNamedItem( pszName );
}

// ---------------------------------------------------------------------------
// Document factory calls
// ---------------------------------------------------------------------------
CBk1XmlNodePtr SNode::createElement( const char *pszName )
{
    SNode *p = pDoc->NewNode( XML_ELEMENT );
    p->szName = ( pszName != 0 ) ? pszName : "";
    return CBk1XmlNodePtr( p );
}

CBk1XmlNodePtr SNode::createTextNode( const char *pszText )
{
    SNode *p = pDoc->NewNode( XML_TEXT );
    p->szName = "#text";
    p->szValue = ( pszText != 0 ) ? pszText : "";
    return CBk1XmlNodePtr( p );
}

CBk1XmlNodePtr SNode::createTextNode( const unsigned short *pwzText )
{
    const UINT nPage = GetACP();
    const int nLen = Bk1Utf16Len( pwzText );
    const int nBytes = Bk1Utf16ToAnsi( nPage, pwzText, nLen, 0, 0 );
    std::string szNarrow;
    szNarrow.resize( (size_t)nBytes );
    if ( nBytes > 0 )
        Bk1Utf16ToAnsi( nPage, pwzText, nLen, &szNarrow[0], nBytes );
    return createTextNode( szNarrow.c_str() );
}

CBk1XmlNodePtr SNode::createProcessingInstruction( const char *pszTarget,
                                                   const char *pszData )
{
    SNode *p = pDoc->NewNode( XML_PI );
    p->szName = ( pszTarget != 0 ) ? pszTarget : "";
    p->szValue = ( pszData != 0 ) ? pszData : "";
    return CBk1XmlNodePtr( p );
}

// ---------------------------------------------------------------------------
// Loading and saving
// ---------------------------------------------------------------------------
VARIANT_BOOL SNode::loadXML( const char *pszXml )
{
    if ( pszXml == 0 )
        return VARIANT_FALSE;

    std::string szText = pszXml;
    size_t i = 0;
    // a byte-order mark, if the file carries one
    if ( szText.size() >= 3 && (unsigned char)szText[0] == 0xEF &&
         (unsigned char)szText[1] == 0xBB && (unsigned char)szText[2] == 0xBF )
        i = 3;

    children.clear();
    pChildCache = 0;
    while ( i < szText.size() )
    {
        if ( szText[i] != '<' )
        {
            ++i;                            // whitespace between top-level nodes
            continue;
        }
        ++i;
        bool bClose = false;
        SNode *pChild = ParseMarkup( pDoc, szText, &i, &bClose );
        if ( pChild != 0 )
        {
            pChild->pParent = this;
            children.push_back( pChild );
        }
    }

    // a document with only a declaration in it did not parse
    for ( size_t k = 0; k < children.size(); ++k )
    {
        if ( children[k]->eType == XML_ELEMENT )
            return VARIANT_TRUE;
    }
    return VARIANT_FALSE;
}

VARIANT_BOOL SNode::load( IStream *pStream )
{
    if ( pStream == 0 )
        return VARIANT_FALSE;

    std::string szText;
    char buff[16384];
    for ( ;; )
    {
        ULONG nRead = 0;
        const HRESULT hr = pStream->Read( buff, sizeof( buff ), &nRead );
        if ( FAILED( hr ) || nRead == 0 )
            break;
        szText.append( buff, nRead );
    }
    if ( szText.empty() )
        return VARIANT_FALSE;
    return loadXML( szText.c_str() );
}

std::string SNode::xml() const
{
    std::string szOut;
    Serialise( this, &szOut );
    return szOut;
}

void SNode::save( IStream *pStream ) const
{
    if ( pStream == 0 )
        return;
    const std::string szOut = xml();
    ULONG nWritten = 0;
    if ( !szOut.empty() )
        pStream->Write( szOut.data(), (ULONG)szOut.size(), &nWritten );
}

}   // namespace NBk1Xml

// ---------------------------------------------------------------------------
// CBk1XmlNodePtr
// ---------------------------------------------------------------------------
void CBk1XmlNodePtr::Attach( NBk1Xml::SNode *p )
{
    pNode = p;
    NBk1Xml::AddRefDocument( NBk1Xml::DocumentOf( p ) );
}

void CBk1XmlNodePtr::Detach()
{
    NBk1Xml::ReleaseDocument( NBk1Xml::DocumentOf( pNode ) );
    pNode = 0;
}

CBk1XmlNodePtr::CBk1XmlNodePtr() : pNode( 0 ) {}

CBk1XmlNodePtr::CBk1XmlNodePtr( NBk1Xml::SNode *p ) : pNode( 0 ) { Attach( p ); }

CBk1XmlNodePtr::CBk1XmlNodePtr( int ) : pNode( 0 ) {}

CBk1XmlNodePtr::CBk1XmlNodePtr( const CBk1XmlNodePtr &a ) : pNode( 0 )
{
    Attach( a.pNode );
}

CBk1XmlNodePtr::CBk1XmlNodePtr( const char * ) : pNode( 0 )
{
    // 'Microsoft.XMLDOM' -- the progid is not consulted; there is one document
    // implementation here. The document starts with the single reference this
    // pointer holds, and dies with the last one.
    NBk1Xml::SDocument *pDoc = new NBk1Xml::SDocument();
    NBk1Xml::SNode *pRoot = pDoc->NewNode( NBk1Xml::XML_DOCUMENT );
    pRoot->szName = "#document";
    Attach( pRoot );
}

CBk1XmlNodePtr::~CBk1XmlNodePtr() { Detach(); }

CBk1XmlNodePtr &CBk1XmlNodePtr::operator=( const CBk1XmlNodePtr &a )
{
    if ( this != &a && pNode != a.pNode )
    {
        NBk1Xml::SNode *pNew = a.pNode;
        NBk1Xml::AddRefDocument( NBk1Xml::DocumentOf( pNew ) );
        Detach();
        pNode = pNew;
    }
    return *this;
}

CBk1XmlNodePtr &CBk1XmlNodePtr::operator=( NBk1Xml::SNode *p )
{
    if ( pNode != p )
    {
        NBk1Xml::AddRefDocument( NBk1Xml::DocumentOf( p ) );
        Detach();
        pNode = p;
    }
    return *this;
}

CBk1XmlNodePtr &CBk1XmlNodePtr::operator=( int )
{
    Detach();
    return *this;
}

// ---------------------------------------------------------------------------
// CBk1XmlNodeListPtr
// ---------------------------------------------------------------------------
void CBk1XmlNodeListPtr::Attach( NBk1Xml::SNodeList *p )
{
    pList = p;
    NBk1Xml::AddRefDocument( NBk1Xml::DocumentOf( p ) );
}

void CBk1XmlNodeListPtr::Detach()
{
    NBk1Xml::ReleaseDocument( NBk1Xml::DocumentOf( pList ) );
    pList = 0;
}

CBk1XmlNodeListPtr::CBk1XmlNodeListPtr() : pList( 0 ) {}

CBk1XmlNodeListPtr::CBk1XmlNodeListPtr( NBk1Xml::SNodeList *p ) : pList( 0 )
{
    Attach( p );
}

CBk1XmlNodeListPtr::CBk1XmlNodeListPtr( int ) : pList( 0 ) {}

CBk1XmlNodeListPtr::CBk1XmlNodeListPtr( const CBk1XmlNodeListPtr &a ) : pList( 0 )
{
    Attach( a.pList );
}

CBk1XmlNodeListPtr::~CBk1XmlNodeListPtr() { Detach(); }

CBk1XmlNodeListPtr &CBk1XmlNodeListPtr::operator=( const CBk1XmlNodeListPtr &a )
{
    if ( this != &a && pList != a.pList )
    {
        NBk1Xml::SNodeList *pNew = a.pList;
        NBk1Xml::AddRefDocument( NBk1Xml::DocumentOf( pNew ) );
        Detach();
        pList = pNew;
    }
    return *this;
}

CBk1XmlNodeListPtr &CBk1XmlNodeListPtr::operator=( int )
{
    Detach();
    return *this;
}
