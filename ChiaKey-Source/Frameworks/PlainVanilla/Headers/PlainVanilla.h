//
// PlainVanilla.h
//
// Copyright (c) 2007-2010 Lukhnos D. Liu (http://lukhnos.org)
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef PlainVanilla_h
#define PlainVanilla_h

// PlainVanilla implements these classes to provide a basic loader:
// * OVKeyImpl (for OVKey)
// * OVLoaderService
// * OVKeyValueMap
// * OVTextBuffer
// * OVCandidateService (and one-dimensional candidate lists)

// Note these loader-related classes are not implemented
// * OVDatabaseService

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#include <PlainVanilla/PVBasicKeyValueMapImpl.h>
#include <PlainVanilla/PVBundleLoadingSystem.h>
#include <PlainVanilla/PVCandidate.h>
#include <PlainVanilla/PVContextSandwich.h>
#include <PlainVanilla/PVKeyImpl.h>
#include <PlainVanilla/PVLoaderPolicy.h>
#include <PlainVanilla/PVLoaderService.h>
#include <PlainVanilla/PVLoaderSystem.h>
#include <PlainVanilla/PVModulePackageSystem.h>
#include <PlainVanilla/PVModuleSystem.h>
#include <PlainVanilla/PVPlistValue.h>
#include <PlainVanilla/PVPropertyList.h>
#include <PlainVanilla/PVTextBuffer.h>
#else
#include "OpenVanilla.h"
#include "PVBasicKeyValueMapImpl.h"
#include "PVCandidate.h"
#include "PVContextSandwich.h"
#include "PVKeyImpl.h"
#include "PVLoaderPolicy.h"
#include "PVLoaderService.h"
#include "PVLoaderSystem.h"
#include "PVModulePackageSystem.h"
#include "PVModuleSystem.h"
#include "PVPlistValue.h"
#include "PVPropertyList.h"
#include "PVTextBuffer.h"
#endif

#if defined(WIN32)
#include "PVDLLLoadingSystem.h"
#endif

#endif
