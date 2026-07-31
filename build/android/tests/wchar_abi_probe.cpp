#include <string>
std::wstring Probe()
{
    std::wstring s = L"probe";
    s.append( L"more" );
    s.reserve( 64 );
    s += L'x';
    return s + L"tail";
}
int ProbeSize() { return (int)Probe().size(); }
