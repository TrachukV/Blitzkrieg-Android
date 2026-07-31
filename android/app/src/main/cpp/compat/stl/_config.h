#pragma once
// The engine was built against STLport, whose config header every StdAfx.h
// pulls in before anything else. On the NDK the standard library is libc++,
// so this stands in for STLport's configuration and contributes nothing:
// neutralising it here is what lets the original sources keep their includes.
