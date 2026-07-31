#pragma once
// Stands in for what '#import "msxml.dll"' generates on MSVC.
//
// StreamIO/DataTreeXML.cpp and DataTableXML.cpp drive the MSXML DOM, and the
// game's data is 7336 XML files, so this is a real parser rather than a stub.
// Only the surface those two files touch is provided, and it keeps MSVC's
// spelling: the properties ('text', 'nodeName', 'length', 'async'), the
// indexed 'item', and smart pointers that compare against 0.
//
// Lifetime follows COM: the document owns every node and node list under it,
// and each smart pointer holds a reference on that document. When the last
// pointer goes, the whole tree goes with it.
#include "bk1_win32_types.h"
#include "bk1_com_stream.h"
#include "comutil.h"

#include <string>
#include <vector>

namespace NBk1Xml {

struct SNode;
struct SNodeList;
struct SDocument;

enum ENodeType { XML_ELEMENT, XML_TEXT, XML_ATTRIBUTE, XML_PI, XML_DOCUMENT };

// ---------------------------------------------------------------------------
// Attribute values
// ---------------------------------------------------------------------------
// MSXML's setAttribute took a VARIANT and spelled the value itself. These
// reproduce that spelling, matching how the engine writes the same kinds of
// value elsewhere ("%d" for integers, "%g" for reals), so that a value written
// here parses back through the engine's own readers.
std::string Bk1XmlValueToText( const char *pszValue );
std::string Bk1XmlValueToText( char *pszValue );
std::string Bk1XmlValueToText( const std::string &szValue );
std::string Bk1XmlValueToText( bool bValue );
std::string Bk1XmlValueToText( char nValue );
std::string Bk1XmlValueToText( unsigned char nValue );
std::string Bk1XmlValueToText( short nValue );
std::string Bk1XmlValueToText( unsigned short nValue );
std::string Bk1XmlValueToText( int nValue );
std::string Bk1XmlValueToText( unsigned int nValue );
std::string Bk1XmlValueToText( long nValue );
std::string Bk1XmlValueToText( unsigned long nValue );
std::string Bk1XmlValueToText( long long nValue );
std::string Bk1XmlValueToText( float fValue );
std::string Bk1XmlValueToText( double fValue );

void AddRefDocument( SDocument *pDoc );
void ReleaseDocument( SDocument *pDoc );
SDocument *DocumentOf( SNode *pNode );
SDocument *DocumentOf( SNodeList *pList );

// ---------------------------------------------------------------------------
// Properties, standing in for MSVC's __declspec(property)
// ---------------------------------------------------------------------------
struct STextProperty
{
    SNode *pOwner;

    STextProperty() : pOwner( 0 ) {}

    operator const char *() const;
    operator _bstr_t() const;
    // 'std::string sz = node->text' is how the engine reads it, and going
    // through 'const char*' would be two user conversions, which C++ does not
    // apply in one step.
    operator std::string() const;
    std::string str() const;

    // MSXML held its text as UTF-16 and _bstr_t converted both ways through
    // the ANSI code page. The document here keeps the file's own bytes, so the
    // UTF-16 view is produced on demand through that same code page -- which
    // is the conversion Windows performed.
    const unsigned short *Utf16() const;

    STextProperty &operator=( const char *psz );
    STextProperty &operator=( const std::string &sz );
    STextProperty &operator=( const _bstr_t &str );
    STextProperty &operator=( const unsigned short *psz );
    // assigning one node's text to another's
    STextProperty &operator=( const STextProperty &other );
};

struct SNameProperty
{
    SNode *pOwner;

    SNameProperty() : pOwner( 0 ) {}

    operator const char *() const;
    operator _bstr_t() const;
    operator std::string() const;
    std::string str() const;

    bool operator==( const char *psz ) const;
    bool operator!=( const char *psz ) const;
};

}   // namespace NBk1Xml

// ---------------------------------------------------------------------------
// Smart pointers
// ---------------------------------------------------------------------------
// Every DOM pointer type is the same handle, so assigning an element where a
// node is wanted -- which the engine does throughout -- needs no conversion,
// exactly as the #import-generated _com_ptr_t types allowed.
class CBk1XmlNodePtr
{
public:
    CBk1XmlNodePtr();
    CBk1XmlNodePtr( NBk1Xml::SNode *p );
    CBk1XmlNodePtr( int );                    // the engine assigns literal 0
    CBk1XmlNodePtr( const CBk1XmlNodePtr &a );
    // 'IXMLDOMDocumentPtr xmlDocument( "Microsoft.XMLDOM" )' creates a document
    explicit CBk1XmlNodePtr( const char *pszProgID );
    ~CBk1XmlNodePtr();

    CBk1XmlNodePtr &operator=( const CBk1XmlNodePtr &a );
    CBk1XmlNodePtr &operator=( NBk1Xml::SNode *p );
    CBk1XmlNodePtr &operator=( int );

    NBk1Xml::SNode *operator->() const { return pNode; }
    operator NBk1Xml::SNode *() const { return pNode; }

    bool operator==( int ) const { return pNode == 0; }
    bool operator!=( int ) const { return pNode != 0; }
    bool operator==( const CBk1XmlNodePtr &a ) const { return pNode == a.pNode; }
    bool operator!=( const CBk1XmlNodePtr &a ) const { return pNode != a.pNode; }
    operator bool() const { return pNode != 0; }

private:
    void Attach( NBk1Xml::SNode *p );
    void Detach();

    NBk1Xml::SNode *pNode;
};

class CBk1XmlNodeListPtr
{
public:
    CBk1XmlNodeListPtr();
    CBk1XmlNodeListPtr( NBk1Xml::SNodeList *p );
    CBk1XmlNodeListPtr( int );
    CBk1XmlNodeListPtr( const CBk1XmlNodeListPtr &a );
    ~CBk1XmlNodeListPtr();

    CBk1XmlNodeListPtr &operator=( const CBk1XmlNodeListPtr &a );
    CBk1XmlNodeListPtr &operator=( int );

    NBk1Xml::SNodeList *operator->() const { return pList; }

    bool operator==( int ) const { return pList == 0; }
    bool operator!=( int ) const { return pList != 0; }
    operator bool() const { return pList != 0; }

private:
    void Attach( NBk1Xml::SNodeList *p );
    void Detach();

    NBk1Xml::SNodeList *pList;
};

namespace NBk1Xml {

// ---------------------------------------------------------------------------
// A list of nodes, with MSXML's 'length' and indexed 'item'
// ---------------------------------------------------------------------------
struct SItemProperty
{
    SNodeList *pOwner;

    SItemProperty() : pOwner( 0 ) {}

    CBk1XmlNodePtr operator[]( int nIndex ) const;
};

struct SNodeList
{
    std::vector<SNode *> items;
    SDocument           *pDoc;
    int                  length;
    SItemProperty        item;

    explicit SNodeList( SDocument *_pDoc ) : pDoc( _pDoc ), length( 0 )
    {
        item.pOwner = this;
    }

    void Rebuild() { length = (int)items.size(); }
};

// ---------------------------------------------------------------------------
// The attribute map, reached as node->attributes->getNamedItem( name )
// ---------------------------------------------------------------------------
struct SAttrMap;

struct SAttrItemProperty
{
    SAttrMap *pOwner;

    SAttrItemProperty() : pOwner( 0 ) {}

    CBk1XmlNodePtr operator[]( int nIndex ) const;
};

struct SAttrMap
{
    SNode            *pOwner;
    int               length;
    SAttrItemProperty item;

    SAttrMap() : pOwner( 0 ), length( 0 ) { item.pOwner = this; }

    // The member is used as though it were a pointer. Forwarding to itself
    // refreshes 'length' first, so it is current at every access the way
    // MSXML's live collection was.
    SAttrMap       *operator->() { Refresh(); return this; }
    const SAttrMap *operator->() const
    {
        const_cast<SAttrMap *>( this )->Refresh();
        return this;
    }

    void Refresh();

    bool operator==( int ) const { return pOwner == 0; }
    bool operator!=( int ) const { return pOwner != 0; }

    CBk1XmlNodePtr getNamedItem( const char *pszName ) const;
    CBk1XmlNodePtr getNamedItem( const std::string &szName ) const
    {
        return getNamedItem( szName.c_str() );
    }
};

// ---------------------------------------------------------------------------
// childNodes, likewise reached through '->'
// ---------------------------------------------------------------------------
struct SChildNodes
{
    SNode *pOwner;

    SChildNodes() : pOwner( 0 ) {}

    const SNodeList *operator->() const;
    SNodeList       *operator->();

    // 'IXMLDOMNodeListPtr pNodes = node->childNodes' is how the engine takes
    // the collection, so it converts to the list handle directly.
    operator CBk1XmlNodeListPtr() const;

    bool operator==( int ) const { return pOwner == 0; }
    bool operator!=( int ) const { return pOwner != 0; }
};

// ---------------------------------------------------------------------------
// The node
// ---------------------------------------------------------------------------
struct SNode
{
    ENodeType            eType;
    std::string          szName;
    std::string          szValue;       // text for text nodes, data for attributes
    std::vector<SNode *> children;
    std::vector<SNode *> attributes_;
    SNode               *pParent;
    SDocument           *pDoc;

    // the properties the #import wrappers expose
    STextProperty text;
    SNameProperty nodeName;
    SAttrMap      attributes;
    SChildNodes   childNodes;
    bool          async;

    // scratch, so that a 'const char*' read of 'text' outlives its expression
    mutable std::string szTextCache;
    // the same for the UTF-16 view of that text
    mutable std::vector<unsigned short> wideTextCache;
    // scratch for childNodes, rebuilt on each access
    mutable SNodeList  *pChildCache;

    SNode( ENodeType e, SDocument *_pDoc );
    ~SNode();

    // --- tree ---
    CBk1XmlNodePtr appendChild( const CBk1XmlNodePtr &child );
    CBk1XmlNodePtr removeChild( const CBk1XmlNodePtr &child );
    CBk1XmlNodePtr selectSingleNode( const char *pszPath ) const;
    CBk1XmlNodePtr selectSingleNode( const std::string &szPath ) const
    {
        return selectSingleNode( szPath.c_str() );
    }
    CBk1XmlNodeListPtr selectNodes( const char *pszPath ) const;
    CBk1XmlNodeListPtr selectNodes( const std::string &szPath ) const
    {
        return selectNodes( szPath.c_str() );
    }

    // --- attributes ---
    // MSXML's setAttribute took a VARIANT, so the engine hands it whatever the
    // caller's template parameter happens to be. The value is spelled the same
    // way the engine spells its own numbers elsewhere, so that what is written
    // here reads back through the same parsers.
    void setAttributeText( const char *pszName, const std::string &szValue );

    template <class TValue>
    void setAttribute( const char *pszName, const TValue &value )
    {
        setAttributeText( pszName, Bk1XmlValueToText( value ) );
    }

    CBk1XmlNodePtr getAttributeNode( const char *pszName ) const;

    // --- document factory calls ---
    // Kept on the one node type so the pointer types stay interchangeable, the
    // way the generated _com_ptr_t types were.
    CBk1XmlNodePtr createElement( const char *pszName );
    CBk1XmlNodePtr createElement( const std::string &szName )
    {
        return createElement( szName.c_str() );
    }
    CBk1XmlNodePtr createTextNode( const char *pszText );
    CBk1XmlNodePtr createTextNode( const std::string &szText )
    {
        return createTextNode( szText.c_str() );
    }
    // The engine writes its wide strings out through this one; the text is
    // stored in the document's own encoding, as MSXML would have written it.
    CBk1XmlNodePtr createTextNode( const unsigned short *pwzText );
    // wchar_t is the same width here but a distinct type, and the engine's
    // wide strings arrive spelled both ways.
    CBk1XmlNodePtr createTextNode( const wchar_t *pwzText )
    {
        return createTextNode( reinterpret_cast<const unsigned short *>( pwzText ) );
    }
    CBk1XmlNodePtr createProcessingInstruction( const char *pszTarget,
                                                const char *pszData );

    // --- serialisation ---
    VARIANT_BOOL load( IStream *pStream );
    VARIANT_BOOL loadXML( const char *pszXml );
    void save( IStream *pStream ) const;
    std::string xml() const;

    // --- helpers ---
    std::string GatherText() const;
    void SetText( const char *psz );
    SDocument *Doc() const { return pDoc; }
};

}   // namespace NBk1Xml

// The names the engine uses. All of them are the one handle type, as the
// generated wrappers effectively were once assigned across.
typedef CBk1XmlNodePtr     IXMLDOMNodePtr;
typedef CBk1XmlNodePtr     IXMLDOMElementPtr;
typedef CBk1XmlNodePtr     IXMLDOMDocumentPtr;
typedef CBk1XmlNodePtr     IXMLDOMTextPtr;
typedef CBk1XmlNodePtr     IXMLDOMCharacterDataPtr;
typedef CBk1XmlNodePtr     IXMLDOMProcessingInstructionPtr;
typedef CBk1XmlNodePtr     IXMLDOMAttributePtr;
typedef CBk1XmlNodeListPtr IXMLDOMNodeListPtr;
typedef CBk1XmlNodePtr     IXMLDOMNamedNodeMapPtr;

// COM apartment calls, which have nothing to initialise here.
inline HRESULT CoInitialize( void * ) { return S_OK; }
inline HRESULT CoInitializeEx( void *, DWORD ) { return S_OK; }
inline void CoUninitialize() {}
