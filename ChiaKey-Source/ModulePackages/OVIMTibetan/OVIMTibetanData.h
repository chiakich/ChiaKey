//
// OVIMTibetanData.h
//
// Copyright (c) 2004-2010 The OpenVanilla Project (http://openvanilla.org)
// All rights reserved.
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

#ifndef OVIMTibetanData_h
#define OVIMTibetanData_h

char ComposeKey[KEYBOARDS] = {'f', 'h', 'h', 'a'};
char SpaceKey[KEYBOARDS] = {'.', '-', '-', '-'};

char SymbolKeys[KEYBOARDS][SYMBOL_NUM] = {
    {'!', ',', '#', '$', '(', ')', '@', ':', ';', '-', '|'},
    {'V', 'C', '@', '#', '(', ')', '|', ':', 'K', '{', '&'},
    {'A', '\\', '!', '@', '(', ')', '$', ':', '%', '+', '|'},
    {'A', '\\', '!', '@', '(', ')', '$', ':', '%', '+', '|'}};

int htransform = 28;

int htransformKey[5] = {2, 10, 14, 18, 32};

char ConsonantKeys[KEYBOARDS][CONSONAT_NUM] = {
    {'k', 'K', 'g', 'G', 'c', 'C', 'j', 'N', 't', 'T', 'd',  'n',
     'p', 'P', 'b', 'm', 'x', 'X', 'D', 'w', 'Z', 'z', '\'', 'y',
     'r', 'l', 'S', 's', 'h', 'A', 'q', 'Q', 'v', 'V', 'B'},
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[', ']',
     'a', 's', 'd', 'f', 'k', 'l', ';', '\'', 'z', 'x', 'c', 'v',
     'm', ',', '.', '/', '>', '?', 'Q', 'W',  'E', 'R', 'T'},
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[', ']',
     'a', 's', 'd', 'f', 'k', 'l', ';', '\'', 'z', 'x', 'c', 'v',
     'm', ',', '.', '/', '>', '?', 'Q', 'W',  'E', 'R', 'T'},
    {'q', 'w', 's', 'e', 'b', 'n',  'm', ',', 'o', 'p', 'j', 'k',
     'r', '/', 'd', 'f', ';', '\'', '[', ']', 'z', 'x', 'c', 'g',
     'h', 'v', '.', 'l', 'G', 'H',  'O', 'P', 'J', 'K', '>'}};

char VowelKeys[KEYBOARDS][VOWEL_NUM] = {
    {'a', 'i', 'u', 'o', 'e', 'E', 'O', 'I'},
    {' ', 'g', 'j', 'n', 'b', 'B', 'N', 'G'},
    {' ', 'g', 'j', 'n', 'b', 'B', 'N', 'G'},
    {' ', 't', 'u', 'i', 'y', 'Y', 'I', 'T'}};

char FinalAddKeys[KEYBOARDS][VOWEL_NUM] = {
    {'%', '&'}, {'!', '%'}, {'`', '*'}, {'`', '*'}};
#endif