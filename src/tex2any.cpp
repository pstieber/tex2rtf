//*****************************************************************************
// Copyright:   (c) Julian Smart
// Author:      Julian Smart
// Modified by: Wlodzimierz ABX Skiba 2003/2004 Unicode support
//              Ron Lee
// Created:     1/1/1999
// License:     wxWindows license
// Description:
//   Utilities for Latex conversion.
//*****************************************************************************

#include "tex2any.h"

#include <wx/wxcrtvararg.h>

#include <ctype.h>
#include <cstdlib>
#include <ctime>
#include <map>

using namespace std;

//*****************************************************************************
// Variables accessible from clients.
//*****************************************************************************

TexChunk* DocumentTitle = nullptr;
TexChunk* DocumentAuthor = nullptr;
TexChunk* DocumentDate = nullptr;

// Header/footers/pagestyle
TexChunk* LeftHeaderEven = nullptr;
TexChunk* LeftFooterEven = nullptr;
TexChunk* CentreHeaderEven = nullptr;
TexChunk* CentreFooterEven = nullptr;
TexChunk* RightHeaderEven = nullptr;
TexChunk* RightFooterEven = nullptr;
TexChunk* LeftHeaderOdd = nullptr;
TexChunk* LeftFooterOdd = nullptr;
TexChunk* CentreHeaderOdd = nullptr;
TexChunk* CentreFooterOdd = nullptr;
TexChunk* RightHeaderOdd = nullptr;
TexChunk* RightFooterOdd = nullptr;
wxString PageStyle("plain");

int        DocumentStyle = LATEX_REPORT;
int        MinorDocumentStyle = 0;
wxPathList TexPathList;
wxString   BibliographyStyleString("plain");
wxString   DocumentStyleString("report");
wxString   MinorDocumentStyleString;
int        ParSkip = 0;
int        ParIndent = 0;

int normalFont = 10;
int smallFont = 8;
int tinyFont = 6;
int largeFont1 = 12;
int LargeFont2 = 14;
int LARGEFont3 = 18;
int hugeFont1 = 20;
int HugeFont2 = 24;
int HUGEFont3 = 28;

// All of these tokens MUST be found on a line by themselves (no other
// text) and must start at the first character of the line, or tex2rtf
// will fail to process them correctly (a limitation of tex2rtf, not TeX)
static const wxString syntaxTokens[] =
{
  _T("\\begin{verbatim}"),
  _T("\\begin{toocomplex}"),
  _T("\\end{verbatim}"),
  _T("\\end{toocomplex}"),
  _T("\\verb"),
  _T("\\begin{comment}"),
  _T("\\end{comment}"),
  _T("\\verbatiminput"),
//  _T("\\par"),
  _T("\\input"),
  _T("\\helpinput"),
  _T("\\include"),
  wxEmptyString
};

//*****************************************************************************
// USER-ADJUSTABLE SETTINGS
//*****************************************************************************

// Section font sizes
int             chapterFont =    12; // LARGEFont3;
int             sectionFont =    12; // LargeFont2;
int             subsectionFont = 12; // largeFont1;
int             titleFont = LARGEFont3;
int             authorFont = LargeFont2;
int             mirrorMargins = true;
bool            winHelp = false;  // Output in Windows Help format if true, linear otherwise
bool            isInteractive = false;
bool            runTwice = false;
int             convertMode = TEX_RTF;
bool            checkCurlyBraces = false;
bool            checkSyntax = false;
bool            headerRule = false;
bool            footerRule = false;
bool            compatibilityMode = false; // If true, maximum Latex compatibility
                                // (Quality of RTF generation deteriorate)
bool            generateHPJ; // Generate WinHelp Help Project file
wxString        winHelpTitle; // Windows Help title
int             defaultTableColumnWidth = 2000;

int             labelIndentTab = 18;  // From left indent to item label (points)
int             itemIndentTab = 40;   // From left indent to item (points)

bool            useUpButton = true;
int             htmlBrowseButtons = HTML_BUTTONS_TEXT;

int  winHelpVersion = 3; // WinHelp Version (3 for Windows 3.1, 4 for Win95)
bool winHelpContents = false; // Generate .cnt file for WinHelp 4
bool htmlIndex = false; // Generate .htx file for HTML
bool htmlFrameContents = false; // Use frames for HTML contents page
wxString htmlStylesheet; // Use this CSS stylesheet for HTML pages
bool useHeadingStyles = true; // Insert \s1, s2 etc.
bool useWord = true; // Insert proper Word table of contents, etc etc
int  contentsDepth = 4; // Depth of Word table of contents
bool indexSubsections = true; // Index subsections in linear RTF
// Linear RTF method of including bitmaps. Can be "includepicture", "hex"
wxString bitmapMethod = "includepicture";
bool upperCaseNames = false;
// HTML background and text colours
wxString backgroundImageString;
wxString backgroundColourString = "255;255;255";
wxString textColourString;
wxString linkColourString;
wxString followedLinkColourString;
bool combineSubSections = false;
bool htmlWorkshopFiles = false;
bool ignoreBadRefs = false;
wxString htmlFaceName;

extern int passNumber;

extern map<wxString, TexRef*> TexReferences;

//*****************************************************************************
// International support
//*****************************************************************************

// Names to help with internationalisation
wxString ContentsNameString = "Contents";
wxString AbstractNameString = "Abstract";
wxString GlossaryNameString = "Glossary";
wxString ReferencesNameString = "References";
wxString FiguresNameString = "List of Figures";
wxString TablesNameString = "List of Tables";
wxString FigureNameString = "Figure";
wxString TableNameString = "Table";
wxString IndexNameString = "Index";
wxString ChapterNameString = "chapter";
wxString SectionNameString = "section";
wxString SubsectionNameString = "subsection";
wxString SubsubsectionNameString = "subsubsection";
wxString UpNameString = "Up";

//*****************************************************************************
// Section numbering
//*****************************************************************************

int chapterNo = 0;
int sectionNo = 0;
int subsectionNo = 0;
int subsubsectionNo = 0;
int figureNo = 0;
int tableNo = 0;

//*****************************************************************************
// Other variables
//*****************************************************************************

FILE* CurrentOutput1 = nullptr;
FILE* CurrentOutput2 = nullptr;
const int NestingLimit = 15;
FILE* Inputs[NestingLimit];
unsigned long LineNumbers[NestingLimit];
wxString FileNames[NestingLimit];
int CurrentInputIndex = 0;

wxString TexFileRoot;
wxString TexBibName;         // Bibliography output file name
wxString TexTmpBibName;      // Temporary bibliography output file name
bool isSync = false;             // If true, should not yield to other processes.
bool stopRunning = false;        // If true, should abort.

static int currentColumn = 0;
wxString currentArgData;
bool haveArgData = false; // If true, we're simulating the data.
TexChunk* currentArgument = nullptr;
TexChunk* pNextChunk = nullptr;
bool isArgOptional = false;
int noArgs = 0;

TexChunk* TopLevel = nullptr;
map<wxString, TexMacroDef*> MacroDefs;
wxArrayString IgnorableInputFiles; // Ignorable \input files, e.g. psbox.tex
wxChar* BigBuffer = nullptr;  // For reading in large chunks of text
TexMacroDef SoloBlockDef(ltSOLO_BLOCK, _T("solo block"), 1, false);
TexMacroDef* VerbatimMacroDef = nullptr;

//*****************************************************************************
//*****************************************************************************
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
TexRef::TexRef(
  const wxString& label,
  const wxString& file,
  const wxString& section,
  const wxString& sectionN)
  : refLabel(label),
    refFile(file),
    sectionNumber(section),
    sectionName(sectionN)
{
}

//*****************************************************************************
//*****************************************************************************
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CustomMacro::CustomMacro(
  const wxString& Name,
  int ArgumentCount,
  const wxString Body)
  : mName(Name),
    mBody(Body),
    mArgumentCount(ArgumentCount)
{
}

//*****************************************************************************
//*****************************************************************************
void TexOutput(const wxString& s, bool ordinaryText)
{
  size_t len = s.length();

  // Update current column, but only if we're guaranteed to
  // be ordinary text (not mark-up stuff)
  size_t i;
  if (ordinaryText)
  {
    for (i = 0; i < len; ++i)
    {
      if (s[i] == 13 || s[i] == 10)
      {
        currentColumn = 0;
      }
      else
      {
        ++currentColumn;
      }
    }
  }

  if (CurrentOutput1)
  {
    wxFprintf(CurrentOutput1, _T("%s"), s);
  }
  if (CurrentOutput2)
  {
    wxFprintf(CurrentOutput2, _T("%s"), s);
  }
}

//*****************************************************************************
// Try to find a Latex macro, in one of the following forms:
// (1) \begin{} ... \end{}
// (2) \macroname{arg1}...{argn}
// (3) {\bf arg1}
//*****************************************************************************
void ForbidWarning(TexMacroDef* def)
{
  wxString informBuf;
  switch (def->forbidden)
  {
    case FORBID_WARN:
    {
      informBuf
        << "Warning: it is recommended that command "
        << def->mName << " is not used.";
      OnInform(informBuf);
      break;
    }
    case FORBID_ABSOLUTELY:
    {
      informBuf
        << "Error: command " << def->mName
        << " cannot be used and will lead to errors.";
      OnInform(informBuf);
      break;
    }
    default:
      break;
  }
}

//*****************************************************************************
//*****************************************************************************
TexMacroDef* MatchMacro(
  wxChar* buffer,
  size_t* pos,
  wxString& env,
  bool* parseToBrace)
{
  *parseToBrace = true;
  size_t i = *pos;
  TexMacroDef* def = nullptr;
  wxChar macroBuf[40];

  // First, try to find begin{thing}
  if (wxStrncmp(buffer+i, _T("begin{"), 6) == 0)
  {
    i += 6;

    size_t j = i;
    while ((isalpha(buffer[j]) || buffer[j] == '*') && ((j - i) < 39))
    {
      macroBuf[j-i] = buffer[j];
      ++j;
    }
    macroBuf[j-i] = 0;

    auto TexMacroDefIter = MacroDefs.find(macroBuf);
    if (TexMacroDefIter != MacroDefs.end())
    {
      def = TexMacroDefIter->second;
      *pos = j + 1;  // BUGBUG Should this be + 1???
      env = def->mName;
      ForbidWarning(def);
      return def;
    }
    else
    {
      return nullptr;
    }
  }

  // Failed, so try to find macro from definition list
  size_t j = i;

  // First try getting a one-character macro, but ONLY
  // if these TWO characters are not both alphabetical (could
  // be a longer macro)
  if (!(isalpha(buffer[i]) && isalpha(buffer[i+1])))
  {
    macroBuf[0] = buffer[i];
    macroBuf[1] = 0;

    auto TexMacroDefIter = MacroDefs.find(macroBuf);
    if (TexMacroDefIter != MacroDefs.end())
    {
      def = TexMacroDefIter->second;
      ++j;
    }
    else
    {
      def = nullptr;
    }
  }

  if (!def)
  {
    while ((isalpha(buffer[j]) || buffer[j] == '*') && ((j - i) < 39))
    {
      macroBuf[j-i] = buffer[j];
      ++j;
    }
    macroBuf[j - i] = 0;

    auto TexMacroDefIter = MacroDefs.find(macroBuf);
    if (TexMacroDefIter != MacroDefs.end())
    {
      def = TexMacroDefIter->second;
    }
    else
    {
      def = nullptr;
    }
  }

  if (def)
  {
    i = j;

    // We want to check whether this is a space-consuming macro
    // (e.g. {\bf word})
    // No brace, e.g. \input thing.tex instead of \input{thing};
    // or a numeric argument, such as \parindent0pt
    if (
      (def->no_args > 0) &&
      ((buffer[i] == 32) || (buffer[i] == '=') || (isdigit(buffer[i]))))
    {
      if ((buffer[i] == 32) || (buffer[i] == '='))
      {
        ++i;
      }

      *parseToBrace = false;
    }
    *pos = i;
    ForbidWarning(def);
    return def;
  }
  return nullptr;
}

//*****************************************************************************
//*****************************************************************************
void EatWhiteSpace(wxChar* buffer, size_t* pos)
{
  size_t len = wxStrlen(buffer);
  size_t j = *pos;
  bool keepGoing = true;
  bool moreLines = true;
  while (
    (j < len) && keepGoing &&
    (buffer[j] == 10 || buffer[j] == 13 || buffer[j] == ' ' || buffer[j] == 9))
  {
    ++j;
    if (j >= len)
    {
      if (moreLines)
      {
        moreLines = read_a_line(buffer);
        len = wxStrlen(buffer);
        j = 0;
      }
      else
      {
        keepGoing = false;
      }
    }
  }
  *pos = j;
}

//*****************************************************************************
//*****************************************************************************
bool FindEndEnvironment(wxChar* buffer, size_t& pos, const wxString& env)
{
  size_t i = pos;

  // Try to find end{thing}
  if (
    (wxStrncmp(buffer + i, _T("end{"), 4) == 0) &&
    (wxStrncmp(buffer + i + 4, env, env.length()) == 0))
  {
    pos = i + 5 + env.length();
    return true;
  }
  else return false;
}

//*****************************************************************************
//*****************************************************************************
bool readingVerbatim = false;

// Within a verbatim, but not nec. verbatiminput.
bool readInVerbatim = false;

unsigned long leftCurly = 0;
unsigned long rightCurly = 0;
static wxString currentFileName;

//*****************************************************************************
//*****************************************************************************
bool CheckForBufferOverrun(unsigned long BufferIndex)
{
  if (BufferIndex >= MAX_LINE_BUFFER_SIZE)
  {
    wxString ErrorMessage;
    ErrorMessage
      << "Line " << LineNumbers[CurrentInputIndex] << " of file "
      << currentFileName << " is too long.  Lines can be no longer than "
      << MAX_LINE_BUFFER_SIZE << " characters.  Truncated.";

    OnError(ErrorMessage);
    return true;
  }
  return false;
}

//*****************************************************************************
//*****************************************************************************
void ReportCurlyBraceError()
{
  wxString ErrorMessage;
    ErrorMessage
    << "An extra right Curly brace ('}') was detected at line "
    << LineNumbers[CurrentInputIndex] << " inside file "
    << currentFileName;

  OnError(ErrorMessage);
}

//*****************************************************************************
//*****************************************************************************
bool read_a_line(wxChar* buf)
{
  if (CurrentInputIndex < 0)
  {
    buf[0] = 0;
    return false;
  }

  int ch = -2;
  unsigned long BufferIndex = 0;
  buf[0] = 0;
  int lastChar;

  while (ch != EOF && ch != 10)
  {
    if (CheckForBufferOverrun(BufferIndex))
    {
      return false;
    }

    if (
      (BufferIndex == 14 && wxStrncmp(buf, _T("\\end{verbatim}"), 14) == 0) ||
      (BufferIndex == 16 && wxStrncmp(buf, _T("\\end{toocomplex}"), 16) == 0))
    {
      readInVerbatim = false;
    }

    lastChar = ch;
    ch = getc(Inputs[CurrentInputIndex]);

    if (checkCurlyBraces)
    {
      if (ch == '{' && !readInVerbatim && lastChar != _T('\\'))
      {
        ++leftCurly;
      }
      if (ch == '}' && !readInVerbatim && lastChar != _T('\\'))
      {
        ++rightCurly;
        if (rightCurly > leftCurly)
        {
          ReportCurlyBraceError();

          // Reduce the right curly brace count so the mismatch count is not
          // reported for every line that has a '}' after the first mismatch.
          --rightCurly;
        }
      }
    }

    if (ch != EOF)
    {
      // Check for 2 consecutive newlines and replace with \par
      if (ch == 10 && !readInVerbatim)
      {
        int ch1 = getc(Inputs[CurrentInputIndex]);
        if ((ch1 == 10) || (ch1 == 13))
        {
          // Eliminate newline (10) following DOS linefeed
          if (ch1 == 13)
          {
            getc(Inputs[CurrentInputIndex]);
          }
          buf[BufferIndex] = 0;
          ++LineNumbers[CurrentInputIndex];
//          wxStrcat(buf, "\\par\n");
//          i += 6;

          if (CheckForBufferOverrun(BufferIndex + 5))
          {
            return false;
          }
          wxStrcat(buf, _T("\\par"));
          BufferIndex += 5;
        }
        else
        {
          ungetc(ch1, Inputs[CurrentInputIndex]);
          if (CheckForBufferOverrun(BufferIndex))
          {
            return false;
          }

          buf[BufferIndex] = (wxChar)ch;
          ++BufferIndex;
        }
      }
      else
      {
        // Convert embedded characters to RTF equivalents
        switch(ch)
        {
          case 0xf6: // �
          case 0xe4: // �
          case 0xfc: // �
          case 0xd6: // �
          case 0xc4: // �
          case 0xdc: // �
            if (CheckForBufferOverrun(BufferIndex + 5))
            {
              return false;
            }
            buf[BufferIndex++] = '\\';
            buf[BufferIndex++] = '"';
            buf[BufferIndex++] = '{';
            switch(ch)
            {
              case 0xf6:
                // �
                buf[BufferIndex++] = 'o';
                break;
              case 0xe4:
                // �
                buf[BufferIndex++] = 'a';
                break;
              case 0xfc:
                 // �
                buf[BufferIndex++] = 'u';
                break;
              case 0xd6:
                // �
                buf[BufferIndex++] = 'O';
                break;
              case 0xc4:
                // �
                buf[BufferIndex++] = 'A';
                break;
              case 0xdc:
                // �
                buf[BufferIndex++] = 'U';
                break;
            }
            buf[BufferIndex++] = '}';
            break;
        case 0xdf:
          // �
          if (CheckForBufferOverrun(BufferIndex + 5))
          {
            return false;
          }
          buf[BufferIndex++] = '\\';
          buf[BufferIndex++] = 's';
          buf[BufferIndex++] = 's';
          buf[BufferIndex++] = '\\';
          buf[BufferIndex++] = '/';
          break;

        default:
          if (CheckForBufferOverrun(BufferIndex))
          {
            return false;
          }

          // If the current character read in is a '_', we need to check
          // whether there should be a '\' before it or not.
          if (ch != '_')
          {
            buf[BufferIndex++] = (wxChar)ch;
            break;
          }

          if (checkSyntax)
          {
            if (readInVerbatim)
            {
              // There should NOT be a '\' before the '_'
              if (
                BufferIndex > 0 &&
                buf[BufferIndex - 1] == '\\' &&
                buf[0] != '%')
              {
////              wxString ErrorMessage;
////              ErrorMessage.Printf(_T("An underscore ('_') was detected at line %lu inside file %s that should NOT have a '\\' before it."),
////                LineNumbers[CurrentInputIndex], (const wxChar*) currentFileName.c_str());
////              OnError(ErrorMessage);
              }
            }
            else
            {
              // There should be a '\' before the '_'
              if (BufferIndex == 0)
              {
                wxString ErrorMessage;
                ErrorMessage
                  << "An underscore ('_') was detected at line "
                  << LineNumbers[CurrentInputIndex]
                  << " inside file " << currentFileName
                  << " that may need a '\\' before it.";
                OnError(ErrorMessage);
              }
              else if (
                (buf[BufferIndex - 1] != '\\') && (buf[0] != '%') &&  // If it is a comment line, then no warnings
                (wxStrncmp(buf, _T("\\input"), 6))) // do not report filenames that have underscores in them
              {
                wxString ErrorMessage;
                ErrorMessage
                  << "An underscore ('_') was detected at line "
                  << LineNumbers[CurrentInputIndex]
                  << " inside file " << currentFileName
                  << " that may need a '\\' before it.";
                  OnError(ErrorMessage);
                }
              }
            }
            buf[BufferIndex++] = (wxChar)ch;
            break;
        }  // switch
      }  // else
    }
    else
    {
      buf[BufferIndex] = 0;
      fclose(Inputs[CurrentInputIndex]);
      Inputs[CurrentInputIndex] = nullptr;
      if (CurrentInputIndex > 0)
      {
         ch = ' '; // No real end of file
      }
      --CurrentInputIndex;

      if (checkCurlyBraces)
      {
        if (leftCurly != rightCurly)
        {
          wxString ErrorMessage;
          ErrorMessage
            << "Curly braces do not match inside file "
            << currentFileName << '\n'
            << leftCurly << " opens, " << rightCurly << " closes";
          OnError(ErrorMessage);
        }
        leftCurly = 0;
        rightCurly = 0;
      }

      if (readingVerbatim)
      {
        readingVerbatim = false;
        readInVerbatim = false;
        wxStrcat(buf, _T("\\end{verbatim}\n"));
        return false;
      }
    }
    if (ch == 10)
    {
      ++LineNumbers[CurrentInputIndex];
    }
  }
  buf[BufferIndex] = 0;

  // Strip out comment environment
  if (wxStrncmp(buf, _T("\\begin{comment}"), 15) == 0)
  {
    while (wxStrncmp(buf, _T("\\end{comment}"), 13) != 0)
    {
      read_a_line(buf);
    }
    return read_a_line(buf);
  }
  // Read a verbatim input file as if it were a verbatim environment
  else if (wxStrncmp(buf, _T("\\verbatiminput"), 14) == 0)
  {
    int wordLen = 14;
    wxChar* fileName = buf + wordLen + 1;

    int j = BufferIndex - 1;
    buf[j] = 0;

    // thing}\par -- eliminate the \par!
    if (wxStrncmp((buf + wxStrlen(buf)-5), _T("\\par"), 4) == 0)
    {
      j -= 5;
      buf[j] = 0;
    }

    if (buf[j - 1] == '}')
    {
      // Ignore the final brace.
      buf[j - 1] = 0;
    }

    wxString actualFile = TexPathList.FindValidPath(fileName);
    currentFileName = actualFile;
    if (actualFile.empty())
    {
      wxString ErrorMessage;
      ErrorMessage << "Could not find file: " << fileName;
      OnError(ErrorMessage);
    }
    else
    {
      wxString informStr;
      informStr.Printf(_T("Processing: %s"), actualFile);
      OnInform(informStr);
      ++CurrentInputIndex;
      if (CurrentInputIndex >= NestingLimit)
      {
        wxString ErrorMessage;
        ErrorMessage
          << "ERROR: read_a_line" << '\n'
          << "  File nesting too deep!  The limit is " << NestingLimit << '!';
        OnError(ErrorMessage);
      }

      Inputs[CurrentInputIndex] = wxFopen(actualFile, _T("r"));
      LineNumbers[CurrentInputIndex] = 1;
      if (!FileNames[CurrentInputIndex].empty())
      {
        FileNames[CurrentInputIndex] = wxEmptyString;
      }
      FileNames[CurrentInputIndex] = actualFile;

      if (!Inputs[CurrentInputIndex])
      {
        --CurrentInputIndex;
        OnError(_T("Could not open verbatiminput file."));
      }
      else
      {
        readingVerbatim = true;
        readInVerbatim = true;
        wxStrcpy(buf, _T("\\begin{verbatim}\n"));
        return false;
      }
    }
    return false;
  }
  else if (
    wxStrncmp(buf, _T("\\input"), 6) == 0 ||
    wxStrncmp(buf, _T("\\helpinput"), 10) == 0 ||
    wxStrncmp(buf, _T("\\include"), 8) == 0)
  {
    int wordLen;
    if (wxStrncmp(buf, _T("\\input"), 6) == 0)
    {
      wordLen = 6;
    }
    else
    {
      if (wxStrncmp(buf, _T("\\include"), 8) == 0)
      {
        wordLen = 8;
      }
      else
      {
        wordLen = 10;
      }
    }

    wxChar* fileName = buf + wordLen + 1;

    int j = BufferIndex - 1;
    buf[j] = 0;

    // \input{thing}\par -- eliminate the \par!
    if (wxStrncmp((buf + wxStrlen(buf)-4), _T("\\par"), 4) == 0)
    {
      j -= 4;
      buf[j] = 0;
    }

    if (buf[j - 1] == _T('}'))
    {
      // Ignore the final brace.
      buf[j - 1] = 0;
    }

    // Remove backslashes from name.
    wxString fileNameStr(fileName);
    fileNameStr.Replace(_T("\\"), _T(""));

    // Ignore some types of input files (e.g. macro definition files).
    wxString fileOnly = wxFileNameFromPath(fileNameStr);
    currentFileName = fileOnly;
    if (IgnorableInputFiles.Index(fileOnly) != wxNOT_FOUND)
    {
      return read_a_line(buf);
    }

    wxString actualFile = TexPathList.FindValidPath(fileNameStr);
    if (actualFile.empty())
    {
      const size_t buf2Size = 400;
      wxChar buf2[buf2Size];
      wxSnprintf(buf2, buf2Size, _T("%s.tex"), fileNameStr.c_str());
      actualFile = TexPathList.FindValidPath(buf2);
    }
    currentFileName = actualFile;

    if (actualFile.empty())
    {
      wxString ErrorMessage;
      ErrorMessage.Printf(_T("Could not find file: %s"),fileName);
      OnError(ErrorMessage);
    }
    else
    {
      // Ensure that if this file includes another,
      // then we look in the same directory as this one.
      TexPathList.EnsureFileAccessible(actualFile);

      wxString informStr;
      informStr << "Processing: " << actualFile;
      OnInform(informStr);
      ++CurrentInputIndex;
      if (CurrentInputIndex >= NestingLimit)
      {
        wxString ErrorMessage;
        ErrorMessage
          << "ERROR: read_a_line" << '\n'
          << "  File nesting too deep!  The limit is " << NestingLimit << '!';
        OnError(ErrorMessage);
      }

      Inputs[CurrentInputIndex] = wxFopen(actualFile, _T("r"));
      LineNumbers[CurrentInputIndex] = 1;
      if (!FileNames[CurrentInputIndex].empty())
      {
        FileNames[CurrentInputIndex] = wxEmptyString;
      }
      FileNames[CurrentInputIndex] = actualFile;

      if (!Inputs[CurrentInputIndex])
      {
        --CurrentInputIndex;
        wxString ErrorMessage;
        ErrorMessage << "Could not open include file " << actualFile;
        OnError(ErrorMessage);
      }
    }
    bool succ = read_a_line(buf);
    return succ;
  }

  if (checkSyntax)
  {
    wxString bufStr = buf;
    for (int index=0; !syntaxTokens[index].empty(); ++index)
    {
      size_t pos = bufStr.find(syntaxTokens[index]);
      if (pos != wxString::npos && pos != 0)
      {
        size_t commentStart = bufStr.find(_T("%"));
        if (commentStart == wxString::npos || commentStart > pos)
        {
          wxString ErrorMessage;
          if (syntaxTokens[index] == _T("\\verb"))
          {
            ErrorMessage
              << "'" << syntaxTokens[index]
              << "$....$' was detected at line "
              << LineNumbers[CurrentInputIndex] << " inside file "
              << currentFileName
              << ".  Please replace this form with \\tt{....}";
          }
          else
          {
            ErrorMessage
              << "'" << syntaxTokens[index]
              << "' was detected at line "
              << LineNumbers[CurrentInputIndex] << " inside file "
              << currentFileName
              << " that is not the only text on the line, starting at column one.";
          }
          OnError(ErrorMessage);
        }
      }
    }
  }  // checkSyntax

  if (
    wxStrncmp(buf, _T("\\begin{verbatim}"), 16) == 0 ||
    wxStrncmp(buf, _T("\\begin{toocomplex}"), 18) == 0)
  {
    readInVerbatim = true;
  }
  else if (
    wxStrncmp(buf, _T("\\end{verbatim}"), 14) == 0 ||
    wxStrncmp(buf, _T("\\end{toocomplex}"), 16) == 0)
  {
    readInVerbatim = false;
  }

  if (checkCurlyBraces)
  {
    if (ch == EOF && leftCurly != rightCurly)
    {
      wxString ErrorMessage;
      ErrorMessage
        << "Curly braces do not match inside file " << currentFileName
        << '\n'
        << leftCurly << " opens, " << rightCurly << " closes";
      OnError(ErrorMessage);
    }
  }

  return (ch == EOF);
} // read_a_line

//*****************************************************************************
// Parse newcommand
//*****************************************************************************
bool ParseNewCommand(wxChar* buffer, size_t* pos)
{
  if (
    wxStrncmp((buffer+(*pos)), _T("newcommand"), 10) == 0 ||
    wxStrncmp((buffer+(*pos)), _T("renewcommand"), 12) == 0)
  {
    if (wxStrncmp((buffer+(*pos)), _T("newcommand"), 10) == 0)
    {
      *pos = *pos + 12;
    }
    else
    {
      *pos = *pos + 14;
    }

    wxChar commandName[100];
    wxChar commandValue[1000];
    int noArgs = 0;
    int i = 0;
    while (buffer[*pos] != _T('}') && (buffer[*pos] != 0))
    {
      commandName[i] = buffer[*pos];
      *pos += 1;
      ++i;
    }
    commandName[i] = 0;
    i = 0;
    *pos += 1;
    if (buffer[*pos] == _T('['))
    {
      *pos += 1;
      noArgs = (int)(buffer[*pos]) - 48;
      *pos += 2; // read past argument and '['
    }
    bool end = false;
    int braceCount = 0;
    while (!end)
    {
      wxChar ch = buffer[*pos];
      if (ch == _T('{'))
      {
        ++braceCount;
      }
      else if (ch == _T('}'))
      {
        braceCount --;
        if (braceCount == 0)
          end = true;
      }
      else if (ch == 0)
      {
        end = !read_a_line(buffer);
        wxUnusedVar(end);
        *pos = 0;
        break;
      }
      commandValue[i] = ch;
      ++i;
      *pos += 1;
    }
    commandValue[i] = 0;

    CustomMacro *macro = new CustomMacro(commandName, noArgs, wxEmptyString);
    if (wxStrlen(commandValue) > 0)
    {
      macro->mBody = commandValue;
    }
    auto it = CustomMacroMap.find(commandName);
    if (it != CustomMacroMap.end())
    {
      CustomMacroMap[commandName] = macro;
      AddMacroDef(ltCUSTOM_MACRO, commandName, noArgs);
    }
    return true;
  }
  else return false;
}

//*****************************************************************************
//*****************************************************************************
void MacroError(wxChar* buffer)
{
  wxString ErrorMessage;
  wxChar macroBuf[200];
  macroBuf[0] = '\\';
  int i = 1;
  wxChar ch;
  while (((ch = buffer[i-1]) != '\n') && (ch != 0))
  {
    macroBuf[i] = ch;
    ++i;
  }
  macroBuf[i] = 0;
  if (i > 20)
    macroBuf[20] = 0;

  ErrorMessage.Printf(
    _T("Could not find macro: %s at line %d, file %s"),
    macroBuf,
    (int)(LineNumbers[CurrentInputIndex]-1),
    FileNames[CurrentInputIndex]);
  OnError(ErrorMessage);

  if (wxStrcmp(macroBuf,_T("\\end{document}")) == 0)
  {
    OnInform( _T("Halted build due to unrecoverable error.") );
    stopRunning = true;
  }
}

//*****************************************************************************
// Parse an argument.
// 'environment' specifies the name of the macro IFF if we're looking for the
// end of an environment, e.g. \end{itemize}.  Otherwise it's nullptr.
// 'parseToBrace' is true if the argument should extend to the next right
// brace, e.g. in {\bf an argument} as opposed to \vskip 30pt
//*****************************************************************************
size_t ParseArg(
  TexChunk* thisArg,
  list<TexChunk*>& children,
  wxChar* buffer,
  size_t pos,
  const wxString& environment,
  bool parseToBrace,
  TexChunk* customMacroArgs)
{
  Tex2RTFYield();
  if (stopRunning)
  {
    return pos;
  }

  bool eof = false;
  BigBuffer[0] = 0;
  size_t buf_ptr = 0;
  size_t len;

  // Consume leading brace or square bracket, but ONLY if not following
  // a space, because this could be e.g. {\large {\bf thing}} where {\bf thing}
  // is the argument of \large AS WELL as being a block in its
  // own right.
//  if (environment.empty())
//  {
//    if ((pos > 0) && (buffer[pos-1] != ' ') && buffer[pos] == '{')
//    {
//      ++pos;
//    }
//    else
//
//    if ((pos > 0) && (buffer[pos-1] != ' ') && (buffer[pos] == '[' || buffer[pos] == '('))
//    {
//      isOptional = true;
//      ++pos;
//    }
//    else if ((pos > 1) && (buffer[pos-1] != ' ') && (buffer[pos+1] == '[' || buffer[pos+1] == '('))
//    {
//      isOptional = true;
//      pos += 2;
//    }
//  }

  // If not parsing to brace, just read the next word
  // (e.g. \vskip 20pt)
  if (!parseToBrace)
  {
    int ch = buffer[pos];
    while (!eof && ch != 13 && ch != 32 && ch != 10 && ch != 0 && ch != '{')
    {
      BigBuffer[buf_ptr] = (wxChar)ch;
      ++buf_ptr;
      ++pos;
      ch = buffer[pos];
    }
    if (buf_ptr > 0)
    {
      TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
      BigBuffer[buf_ptr] = 0;
      chunk->mValue = BigBuffer;
      children.push_back(chunk);
    }
    return pos;
  }

  while (!eof)
  {
    len = wxStrlen(buffer);
    if (pos >= len)
    {
      if (customMacroArgs) return 0;

      eof = read_a_line(buffer);
      pos = 0;
      // Check for verbatim (or toocomplex, which comes to the same thing)
      wxString bufStr = buffer;
//      if (bufStr.find("\\begin{verbatim}") != wxString::npos ||
//          bufStr.find("\\begin{toocomplex}") != wxString::npos)
      if (
        wxStrncmp(buffer, _T("\\begin{verbatim}"), 16) == 0 ||
        wxStrncmp(buffer, _T("\\begin{toocomplex}"), 18) == 0)
      {
        if (buf_ptr > 0)
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
          BigBuffer[buf_ptr] = 0;
          chunk->mValue = BigBuffer;
          children.push_back(chunk);
        }
        BigBuffer[0] = 0;
        buf_ptr = 0;

        eof = read_a_line(buffer);
        while (!eof && (wxStrncmp(buffer, _T("\\end{verbatim}"), 14) != 0) &&
                       (wxStrncmp(buffer, _T("\\end{toocomplex}"), 16) != 0)
               )
        {
          wxStrcat(BigBuffer, buffer);
          buf_ptr += wxStrlen(buffer);
          eof = read_a_line(buffer);
        }
        eof = read_a_line(buffer);
        buf_ptr = 0;

        TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO, VerbatimMacroDef);
        chunk->no_args = 1;
        chunk->macroId = ltVERBATIM;
        TexChunk* arg = new TexChunk(CHUNK_TYPE_ARG, VerbatimMacroDef);
        arg->argn = 1;
        arg->macroId = ltVERBATIM;
        TexChunk* str = new TexChunk(CHUNK_TYPE_STRING);
        str->mValue = BigBuffer;

        children.push_back(chunk);
        chunk->mChildren.push_back(arg);
        arg->mChildren.push_back(str);

        // Also want to include the following newline (is always a newline
        // after a verbatim): EXCEPT in HTML
        if (convertMode != TEX_HTML)
        {
          TexMacroDef* parDef(nullptr);
          auto TexMacroDefIter = MacroDefs.find(_T("\\"));
          if (TexMacroDefIter != MacroDefs.end())
          {
            parDef = TexMacroDefIter->second;
          }
          TexChunk* parChunk = new TexChunk(CHUNK_TYPE_MACRO, parDef);
          parChunk->no_args = 0;
          parChunk->macroId = ltBACKSLASHCHAR;
          children.push_back(parChunk);
        }
      }
    }

    wxChar wxCh = buffer[pos];
    // End of optional argument -- pretend it's right brace for simplicity
    if (thisArg->optional && (wxCh == _T(']')))
      wxCh = _T('}');

    switch (wxCh)
    {
      case 0:
      case _T('}'):  // End of argument
      {
        if (buf_ptr > 0)
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
          BigBuffer[buf_ptr] = 0;
          chunk->mValue = BigBuffer;
          children.push_back(chunk);
        }
        if (wxCh == _T('}'))
        {
          ++pos;
        }
        return pos;
      }
      case _T('\\'):
      {
        if (buf_ptr > 0)  // Finish off the string we've read so far
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
          BigBuffer[buf_ptr] = 0;
          buf_ptr = 0;
          chunk->mValue = BigBuffer;
          children.push_back(chunk);
        }
        ++pos;

        // Try matching \end{environment}
        if (
          !environment.empty() &&
          FindEndEnvironment(buffer, pos, environment))
        {
          // Eliminate newline after an \end{} if possible
          if (buffer[pos] == 13)
          {
            ++pos;
            if (buffer[pos] == 10)
            {
              ++pos;
            }
          }
          return pos;
        }

        if (ParseNewCommand(buffer, &pos))
          break;

        if (wxStrncmp(buffer+pos, _T("special"), 7) == 0)
        {
          pos += 7;

          // Discard {
          ++pos;
          int noBraces = 1;

          wxTex2RTFBuffer[0] = 0;
          int i = 0;
          bool end = false;
          while (!end)
          {
            wxChar ch = buffer[pos];
            if (ch == _T('}'))
            {
              noBraces --;
              if (noBraces == 0)
              {
                wxTex2RTFBuffer[i] = 0;
                end = true;
              }
              else
              {
                wxTex2RTFBuffer[i] = _T('}');
                ++i;
              }
              ++pos;
            }
            else if (ch == _T('{'))
            {
              wxTex2RTFBuffer[i] = _T('{');
              ++i;
              ++pos;
            }
            else if (ch == _T('\\') && buffer[pos+1] == _T('}'))
            {
              wxTex2RTFBuffer[i] = _T('}');
              pos += 2;
              ++i;
            }
            else if (ch == _T('\\') && buffer[pos+1] == _T('{'))
            {
              wxTex2RTFBuffer[i] = _T('{');
              pos += 2;
              ++i;
            }
            else
            {
              wxTex2RTFBuffer[i] = ch;
              ++pos;
              ++i;
              if (ch == 0)
              {
                end = true;
              }
            }
          }
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO);
          chunk->no_args = 1;
          chunk->macroId = ltSPECIAL;
          TexMacroDef* specialDef(nullptr);
          auto TexMacroDefIter = MacroDefs.find(_T("special"));
          if (TexMacroDefIter != MacroDefs.end())
          {
            specialDef = TexMacroDefIter->second;
          }
          chunk->def = specialDef;
          TexChunk* arg = new TexChunk(CHUNK_TYPE_ARG, specialDef);
          chunk->mChildren.push_back(arg);
          arg->argn = 1;
          arg->macroId = chunk->macroId;

          // The value in the first argument.
          TexChunk* argValue = new TexChunk(CHUNK_TYPE_STRING);
          arg->mChildren.push_back(argValue);
          argValue->argn = 1;
          argValue->mValue = wxTex2RTFBuffer;

          children.push_back(chunk);
        }
        else if (wxStrncmp(buffer+pos, _T("verb"), 4) == 0)
        {
          pos += 4;
          if (buffer[pos] == _T('*'))
          {
            ++pos;
          }

          // Find the delimiter character
          wxChar ch = buffer[pos];
          ++pos;

          // Now at start of verbatim text
          size_t j = pos;
          while ((buffer[pos] != ch) && buffer[pos] != 0)
          {
            ++pos;
          }
          wxChar* val = new wxChar[pos - j + 1];
          size_t i;
          for (i = j; i < pos; ++i)
          {
            val[i - j] = buffer[i];
          }
          val[i - j] = 0;

          ++pos;

          TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO);
          chunk->no_args = 1;
          chunk->macroId = ltVERB;
          TexMacroDef* verbDef(nullptr);
          auto TexMacroDefIter = MacroDefs.find(_T("verb"));
          if (TexMacroDefIter != MacroDefs.end())
          {
            verbDef = TexMacroDefIter->second;
          }
          chunk->def = verbDef;
          TexChunk* arg = new TexChunk(CHUNK_TYPE_ARG, verbDef);
          chunk->mChildren.push_back(arg);
          arg->argn = 1;
          arg->macroId = chunk->macroId;

          // The value in the first argument.
          TexChunk* argValue = new TexChunk(CHUNK_TYPE_STRING);
          arg->mChildren.push_back(argValue);
          argValue->argn = 1;
          argValue->mValue = val;

          children.push_back(chunk);
        }
        else
        {
          wxString env;
          bool tmpParseToBrace = true;
          TexMacroDef* def = MatchMacro(buffer, &pos, env, &tmpParseToBrace);
          if (def)
          {
            CustomMacro* customMacro = FindCustomMacro(def->mName);

            TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO, def);

            chunk->no_args = def->no_args;
            chunk->macroId = def->macroId;

            if (!customMacro)
            {
              children.push_back(chunk);
            }

            // Eliminate newline after a \begin{} or a \\ if possible
            if (
              (!env.empty() || wxStrcmp(def->mName, _T("\\")) == 0) &&
              (buffer[pos] == 13))
            {
              ++pos;
              if (buffer[pos] == 10)
              {
                ++pos;
              }
            }

            pos = ParseMacroBody(
              chunk,
              chunk->no_args,
              buffer,
              pos,
              env,
              tmpParseToBrace,
              customMacroArgs);

            // If custom macro, parse the body substituting the above found
            // args.
            if (customMacro)
            {
              if (!customMacro->mBody.empty())
              {
                wxChar macroBuf[300];
//                wxStrcpy(macroBuf, _T("{"));
                wxStrcpy(macroBuf, customMacro->mBody);
                wxStrcat(macroBuf, _T("}"));
                ParseArg(
                  thisArg,
                  children,
                  macroBuf,
                  0,
                  wxEmptyString,
                  true,
                  chunk);
              }

              delete chunk; // Might delete children
            }
          }
          else
          {
            MacroError(buffer+pos);
          }
        }
        break;
      }
      // Parse constructs like {\bf thing} as if they were
      // \bf{thing}
      case _T('{'):
      {
        ++pos;
        if (buffer[pos] == _T('\\'))
        {
          if (buf_ptr > 0)
          {
            TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
            BigBuffer[buf_ptr] = 0;
            buf_ptr = 0;
            chunk->mValue = BigBuffer;
            children.push_back(chunk);
          }
          ++pos;

          wxString env;
          bool tmpParseToBrace;
          TexMacroDef* def = MatchMacro(buffer, &pos, env, &tmpParseToBrace);
          if (def)
          {
            CustomMacro* customMacro = FindCustomMacro(def->mName);

            TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO, def);
            chunk->no_args = def->no_args;
            chunk->macroId = def->macroId;
            if (!customMacro)
              children.push_back(chunk);

            pos = ParseMacroBody(
              chunk,
              chunk->no_args,
              buffer,
              pos,
              wxEmptyString,
              true,
              customMacroArgs);

            // If custom macro, parse the body substituting the above found args.
            if (customMacro)
            {
              if (!customMacro->mBody.empty())
              {
                wxChar macroBuf[300];
//                wxStrcpy(macroBuf, _T("{"));
                wxStrcpy(macroBuf, customMacro->mBody);
                wxStrcat(macroBuf, _T("}"));
                ParseArg(
                  thisArg,
                  children,
                  macroBuf,
                  0,
                  wxEmptyString,
                  true,
                  chunk);
              }

//            delete chunk; // Might delete children
            }
          }
          else
          {
            MacroError(buffer+pos);
          }
        }
        else
        {
          // If all else fails, we assume that we have a pair of braces on
          // their own, so return a `dummy' macro definition with just one
          // argument to parse.

          // Save the text so far.
          if (buf_ptr > 0)
          {
            TexChunk* chunk1 = new TexChunk(CHUNK_TYPE_STRING);
            BigBuffer[buf_ptr] = 0;
            buf_ptr = 0;
            chunk1->mValue = BigBuffer;
            children.push_back(chunk1);
          }
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO, &SoloBlockDef);
          chunk->no_args = SoloBlockDef.no_args;
          chunk->macroId = SoloBlockDef.macroId;
          children.push_back(chunk);

          TexChunk* arg = new TexChunk(CHUNK_TYPE_ARG, &SoloBlockDef);

          chunk->mChildren.push_back(arg);
          arg->argn = 1;
          arg->macroId = chunk->macroId;

          pos = ParseArg(
            arg,
            arg->mChildren,
            buffer,
            pos,
            wxEmptyString,
            true,
            customMacroArgs);
        }
        break;
      }
      case _T('$'):
      {
        if (buf_ptr > 0)
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
          BigBuffer[buf_ptr] = 0;
          buf_ptr = 0;
          chunk->mValue = BigBuffer;
          children.push_back(chunk);
        }

        ++pos;

        if (buffer[pos] == _T('$'))
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO);
          chunk->no_args = 0;
          chunk->macroId = ltSPECIALDOUBLEDOLLAR;
          children.push_back(chunk);
          ++pos;
        }
        else
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO);
          chunk->no_args = 0;
          chunk->macroId = ltSPECIALDOLLAR;
          children.push_back(chunk);
        }
        break;
      }
      case _T('~'):
      {
        if (buf_ptr > 0)
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
          BigBuffer[buf_ptr] = 0;
          buf_ptr = 0;
          chunk->mValue = BigBuffer;
          children.push_back(chunk);
        }

        ++pos;
        TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO);
        chunk->no_args = 0;
        chunk->macroId = ltSPECIALTILDE;
        children.push_back(chunk);
        break;
      }
      case _T('#'): // Either treat as a special TeX character or as a macro arg
      {
        if (buf_ptr > 0)
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
          BigBuffer[buf_ptr] = 0;
          buf_ptr = 0;
          chunk->mValue = BigBuffer;
          children.push_back(chunk);
        }

        ++pos;
        if (!customMacroArgs)
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO);
          chunk->no_args = 0;
          chunk->macroId = ltSPECIALHASH;
          children.push_back(chunk);
        }
        else
        {
          if (isdigit(buffer[pos]))
          {
            int n = buffer[pos] - 48;
            ++pos;

            int Index = 0;
            list<TexChunk*>::iterator iNode =
              customMacroArgs->mChildren.begin();
            for (
              ;
              iNode != customMacroArgs->mChildren.end();
              ++iNode, ++Index)
            {
              if (Index == n - 1)
              {
                break;
              }
            }
            if (iNode != customMacroArgs->mChildren.end())
            {
              TexChunk* argChunk = *iNode;
              children.push_back(new TexChunk(*argChunk));
            }
          }
        }
        break;
      }
      case _T('&'):
      {
        // Remove white space before and after the ampersand,
        // since this is probably a table column separator with
        // some convenient -- but useless -- white space in the text.
        while ((buf_ptr > 0) && ((BigBuffer[buf_ptr-1] == _T(' ')) || (BigBuffer[buf_ptr-1] == 9)))
          buf_ptr --;

        if (buf_ptr > 0)
        {
          TexChunk* chunk = new TexChunk(CHUNK_TYPE_STRING);
          BigBuffer[buf_ptr] = 0;
          buf_ptr = 0;
          chunk->mValue = BigBuffer;
          children.push_back(chunk);
        }

        ++pos;

        while (buffer[pos] == _T(' ') || buffer[pos] == 9)
        {
          ++pos;
        }

        TexChunk* chunk = new TexChunk(CHUNK_TYPE_MACRO);
        chunk->no_args = 0;
        chunk->macroId = ltSPECIALAMPERSAND;
        children.push_back(chunk);
        break;
      }
      // Eliminate end-of-line comment
      case _T('%'):
      {
        wxCh = buffer[pos];
        while (wxCh != 10 && wxCh != 13 && wxCh != 0)
        {
          ++pos;
          wxCh = buffer[pos];
        }
        if (buffer[pos] == 10 || buffer[pos] == 13)
        {
          ++pos;
          if (buffer[pos] == 10)
          {
            // Eliminate newline following DOS line feed
            ++pos;
          }
        }
        break;
      }
      // Eliminate tab
      case 9:
      {
        BigBuffer[buf_ptr] = _T(' ');
        BigBuffer[buf_ptr+1] = 0;
        ++buf_ptr;
        ++pos;
        break;
      }
      default:
      {
        BigBuffer[buf_ptr] = wxCh;
        BigBuffer[buf_ptr+1] = 0;
        ++buf_ptr;
        ++pos;
        break;
      }
    }
  }
  return pos;
}

//*****************************************************************************
// Consume as many arguments as the macro definition specifies
//*****************************************************************************
size_t ParseMacroBody(
  TexChunk* parent,
  int no_args,
  wxChar* buffer,
  size_t pos,
  const wxString& environment,
  bool parseToBrace,
  TexChunk* customMacroArgs)
{
  Tex2RTFYield();
  if (stopRunning)
  {
    return pos;
  }

  // Check for a first optional argument
  if (buffer[pos] == ' ' && buffer[pos+1] == '[')
  {
    // Fool following code into thinking that this is definitely
    // an optional first argument. (If a space before a non-first argument,
    // [ is interpreted as a [, not an optional argument.)
    buffer[pos] = '!';
    ++pos;
    ++no_args;
  }
  else
  {
    if (buffer[pos] == '[')
    {
      ++no_args;
    }
  }

  int maxArgs = 0;

  int i;
  for (i = 0; i < no_args; ++i)
  {
    ++maxArgs;
    TexChunk* arg = new TexChunk(CHUNK_TYPE_ARG, parent->def);

    parent->mChildren.push_back(arg);
    arg->argn = maxArgs;
    arg->macroId = parent->macroId;

    // To parse the first arg of a 2 arg \begin{thing}{arg} ... \end{thing}
    // have to fool parser into thinking this is a regular kind of block.
    wxString actualEnv;
    if ((no_args == 2) && (i == 0))
    {
//      actualEnv = wxEmptyString;
    }
    else
    {
      actualEnv = environment;
    }

    bool isOptional = false;

    // Remove the first { of the argument so it doesn't get recognized as { ... }
//    EatWhiteSpace(buffer, &pos);

    if (actualEnv.empty())
    {
      // The reason for these tests is to not consume braces that don't
      // belong to this macro.
      // E.g. {\bf {\small thing}}
      if ((pos > 0) && (buffer[pos-1] != ' ') && buffer[pos] == '{')
      {
        ++pos;
      }
      else
      if ((pos > 0) && (buffer[pos-1] != ' ') && (buffer[pos] == '['))
      {
        isOptional = true;
        ++pos;
      }
      else if ((pos > 1) && (buffer[pos-1] != ' ') && (buffer[pos+1] == '['))
      {
        isOptional = true;
        pos += 2;
      }
      else if (i > 0)
      {
        wxString ErrorMessage;
        wxString tmpBuffer(buffer);
        if (tmpBuffer.length() > 4)
        {
            if (tmpBuffer.Right(4) == _T("\\par"))
                tmpBuffer = tmpBuffer.Mid(0,tmpBuffer.length()-4);
        }
        ErrorMessage.Printf(_T("Missing macro argument in the line:\n\t%s\n"),tmpBuffer.c_str());
        OnError(ErrorMessage);
      }
    }

    arg->optional = isOptional;

    pos = ParseArg(
      arg,
      arg->mChildren,
      buffer,
      pos,
      actualEnv,
      parseToBrace,
      customMacroArgs);

    // If we've encountered an OPTIONAL argument, go another time around
    // the loop, because we've got more than we thought.
    // Hopefully optional args don't occur at the end of a macro use
    // or we might miss it.
    // Don't increment no of times round loop if the first optional arg
    // -- we already did it before the loop.
    if (arg->optional && (i > 0))
      i --;
  }
  parent->no_args = maxArgs;

  // Tell each argument how many args there are (useful when processing an arg)
  for (
    list<TexChunk*>::iterator iNode = parent->mChildren.begin();
    iNode != parent->mChildren.end();
    ++iNode)
  {
    TexChunk* chunk = *iNode;
    chunk->no_args = maxArgs;
  }
  return pos;
}

//*****************************************************************************
//*****************************************************************************
bool TexLoadFile(const wxString& FileName)
{
  static wxChar line_buffer[MAX_LINE_BUFFER_SIZE + 1];
  stopRunning = false;
  TexFileRoot = FileName;
  StripExtension(TexFileRoot);
  TexBibName.clear();
  TexBibName << TexFileRoot << ".bb";
  TexTmpBibName.clear();
  TexTmpBibName << TexFileRoot << ".bb1";

  TexPathList.EnsureFileAccessible(FileName);

  Inputs[0] = wxFopen(FileName, _T("r"));
  LineNumbers[0] = 1;
  FileNames[0] = FileName;
  if (Inputs[0])
  {
    read_a_line(line_buffer);
    ParseMacroBody(TopLevel, 1, line_buffer, 0, wxEmptyString, true);
    if (Inputs[0])
    {
      fclose(Inputs[0]);
    }
    return true;
  }

  return false;
}

//*****************************************************************************
//*****************************************************************************
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
TexMacroDef::TexMacroDef(
  int the_id,
  const wxChar* the_name,
  int n,
  bool ig,
  bool forbidLevel)
{
  mName = the_name;
  no_args = n;
  ignore = ig;
  macroId = the_id;
  forbidden = forbidLevel;
}

//*****************************************************************************
//*****************************************************************************
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
TexChunk::TexChunk(int the_type, TexMacroDef* the_def)
{
  type = the_type;
  no_args = 0;
  argn = 0;
//  name = nullptr;
  def = the_def;
  macroId = 0;
  mValue.clear();
  optional = false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
TexChunk::TexChunk(TexChunk& toCopy)
{
  type = toCopy.type;
  no_args = toCopy.no_args;
  argn = toCopy.argn;
  macroId = toCopy.macroId;

  def = toCopy.def;

  mValue = toCopy.mValue;

  optional = toCopy.optional;
  for (
    list<TexChunk*>::iterator iNode = toCopy.mChildren.begin();
    iNode != toCopy.mChildren.end();
    ++iNode)
  {
    TexChunk* child = *iNode;
    mChildren.push_back(new TexChunk(*child));
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
TexChunk::~TexChunk()
{
  for (
    list<TexChunk*>::iterator iNode = mChildren.begin();
    iNode != mChildren.end();
    ++iNode)
  {
    TexChunk* pChild = *iNode;
    delete pChild;
  }
}

//*****************************************************************************
//*****************************************************************************
bool IsArgOptional()
{
  return isArgOptional;
}

//*****************************************************************************
//*****************************************************************************
int GetNoArgs() // Number of args for this macro
{
  return noArgs;
}

//*****************************************************************************
// Gets the text of a chunk on request (must be for small arguments only!)
//*****************************************************************************
void GetArgData1(TexChunk* chunk)
{
  switch (chunk->type)
  {
    case CHUNK_TYPE_MACRO:
    {
      TexMacroDef* def = chunk->def;
      if (def && def->ignore)
        return;

      if (def && (wxStrcmp(def->mName, _T("solo block")) != 0))
      {
        currentArgData.append("\\");
        currentArgData.append(def->mName);
      }

      for (
        list<TexChunk*>::iterator iNode = chunk->mChildren.begin();
        iNode != chunk->mChildren.end();
        ++iNode)
      {
        TexChunk* child_chunk = *iNode;
        currentArgData.append("{");
        GetArgData1(child_chunk);
        currentArgData.append("}");
      }
      break;
    }
    case CHUNK_TYPE_ARG:
    {
      for (
        list<TexChunk*>::iterator iNode = chunk->mChildren.begin();
        iNode != chunk->mChildren.end();
        ++iNode)
      {
        TexChunk* child_chunk = *iNode;
        GetArgData1(child_chunk);
      }
      break;
    }
    case CHUNK_TYPE_STRING:
    {
      if (!chunk->mValue.empty())
      {
        currentArgData.append(chunk->mValue);
      }
      break;
    }
  }
}

//*****************************************************************************
//*****************************************************************************
wxString GetArgData(TexChunk* WXUNUSED(chunk))
{
  currentArgData.clear();
  GetArgData1(currentArgument);
  haveArgData = false;
  return currentArgData;
}

//*****************************************************************************
//*****************************************************************************
wxString GetArgData()
{
  if (!haveArgData)
  {
    currentArgData.clear();
    GetArgData1(currentArgument);
  }
  return currentArgData;
}

//*****************************************************************************
//*****************************************************************************
TexChunk* GetArgChunk()
{
  return currentArgument;
}

//*****************************************************************************
//*****************************************************************************
TexChunk* GetNextChunk()     // Look ahead to the next chunk
{
  return pNextChunk;
}

//*****************************************************************************
//*****************************************************************************
TexChunk* GetTopLevelChunk()
{
  return TopLevel;
}

//*****************************************************************************
//*****************************************************************************
int GetCurrentColumn()
{
  return currentColumn;
}

//*****************************************************************************
// Traverses document calling functions to allow the client to
// write out the appropriate stuff
//*****************************************************************************
void TraverseFromChunk(
  TexChunk* chunk,
  list<TexChunk*>::iterator iThisNode,
  list<TexChunk*>::iterator iEnd,
  bool childrenOnly)
{
  Tex2RTFYield();
  if (stopRunning)
  {
    return;
  }

  switch (chunk->type)
  {
    case CHUNK_TYPE_MACRO:
    {
      TexMacroDef* def = chunk->def;
      if (def && def->ignore)
      {
        return;
      }

      if (!childrenOnly)
      {
        OnMacro(chunk->macroId, chunk->no_args, true);
      }

      for (
        list<TexChunk*>::iterator iNode = chunk->mChildren.begin();
        iNode != chunk->mChildren.end();
        ++iNode)
      {
        TexChunk* child_chunk = *iNode;
        TraverseFromChunk(child_chunk, iNode, chunk->mChildren.end());
      }

      if (iThisNode != iEnd)
      {
        ++iThisNode;
        if (iThisNode != iEnd)
        {
          pNextChunk = *iThisNode;
        }
        else
        {
          pNextChunk = nullptr;
        }
      }

      if (!childrenOnly)
      {
        OnMacro(chunk->macroId, chunk->no_args, false);
      }
      break;
    }
    case CHUNK_TYPE_ARG:
    {
      currentArgument = chunk;

      isArgOptional = chunk->optional;
      noArgs = chunk->no_args;

      // If OnArgument returns false, don't output.

      if (childrenOnly || OnArgument(chunk->macroId, chunk->argn, true))
      {
        for (
          list<TexChunk*>::iterator iNode = chunk->mChildren.begin();
          iNode != chunk->mChildren.end();
          ++iNode)
        {
          TexChunk* child_chunk = *iNode;
          TraverseFromChunk(child_chunk, iNode, chunk->mChildren.end());
        }
      }

      currentArgument = chunk;

      if (iThisNode != iEnd)
      {
        ++iThisNode;
        if (iThisNode != iEnd)
        {
          pNextChunk = *iThisNode;
        }
        else
        {
          pNextChunk = nullptr;
        }
      }

      isArgOptional = chunk->optional;
      noArgs = chunk->no_args;

      if (!childrenOnly)
      {
        OnArgument(chunk->macroId, chunk->argn, false);
      }
      break;
    }
    case CHUNK_TYPE_STRING:
    {
      extern int issuedNewParagraph;
      extern int forbidResetPar;
      if (!chunk->mValue.empty() && (forbidResetPar == 0))
      {
        // If non-whitespace text, we no longer have a new paragraph.
        if (
          issuedNewParagraph &&
          !((chunk->mValue[0] == 10 ||
             chunk->mValue[0] == 13 ||
             chunk->mValue[0] == 32) && chunk->mValue.length() == 1))
        {
          issuedNewParagraph = false;
        }
        TexOutput(chunk->mValue, true);
      }
      break;
    }
  }
}

//*****************************************************************************
//*****************************************************************************
void TraverseDocument()
{
  list<TexChunk*>::iterator iEnd = TopLevel->mChildren.end();
  TraverseFromChunk(TopLevel, iEnd, iEnd);
}

//*****************************************************************************
//*****************************************************************************
void SetCurrentOutput(FILE* fd)
{
  CurrentOutput1 = fd;
  CurrentOutput2 = nullptr;
}

//*****************************************************************************
//*****************************************************************************
void SetCurrentOutputs(FILE* fd1, FILE* fd2)
{
  CurrentOutput1 = fd1;
  CurrentOutput2 = fd2;
}

//*****************************************************************************
//*****************************************************************************
void AddMacroDef(
  int the_id,
  const wxChar* name,
  int n,
  bool Ignore,
  bool forbid)
{
  MacroDefs.insert(
    make_pair(name, new TexMacroDef(the_id, name, n, Ignore, forbid)));
}

//*****************************************************************************
//*****************************************************************************
void TexInitialize(int bufSize)
{
  InitialiseColourTable();
#ifdef __WXMSW__
  TexPathList.AddEnvList(_T("TEXINPUT"));
#endif
#ifdef __UNIX__
  TexPathList.AddEnvList(_T("TEXINPUTS"));
#endif

  for (int i = 0; i < NestingLimit; ++i)
  {
    Inputs[i] = nullptr;
    LineNumbers[i] = 1;
    FileNames[i] = wxEmptyString;
  }

  IgnorableInputFiles.Add(_T("psbox.tex"));
  BigBuffer = new wxChar[bufSize * 1000];
  AddMacroDef(ltTOPLEVEL, _T("toplevel"), 1);
  TopLevel = new TexChunk(CHUNK_TYPE_MACRO);
  TopLevel->macroId = ltTOPLEVEL;
  TopLevel->no_args = 1;
  auto TexMacroDefIter = MacroDefs.find(_T("verbatim"));
  if (TexMacroDefIter != MacroDefs.end())
  {
    VerbatimMacroDef = TexMacroDefIter->second;
  }
  else
  {
    VerbatimMacroDef = nullptr;
  }
}

//*****************************************************************************
//*****************************************************************************
void TexCleanUp()
{
  for (int i = 0; i < NestingLimit; ++i)
  {
    Inputs[i] = nullptr;
  }

  chapterNo = 0;
  sectionNo = 0;
  subsectionNo = 0;
  subsubsectionNo = 0;
  figureNo = 0;

  CurrentOutput1 = nullptr;
  CurrentOutput2 = nullptr;
  CurrentInputIndex = 0;
  haveArgData = false;
  noArgs = 0;

  if (TopLevel)
    delete TopLevel;
  TopLevel = new TexChunk(CHUNK_TYPE_MACRO);
  TopLevel->macroId = ltTOPLEVEL;
  TopLevel->no_args = 1;

  DocumentTitle = nullptr;
  DocumentAuthor = nullptr;
  DocumentDate = nullptr;
  DocumentStyle = LATEX_REPORT;
  MinorDocumentStyle = 0;
  BibliographyStyleString = "plain";
  DocumentStyleString = "report";
  MinorDocumentStyleString.clear();

  // gt - Changed this so if this is the final pass
  // then we DO want to remove these macros, so that
  // memory is not MASSIVELY leaked if the user
  // does not exit the program, but instead runs
  // the program again
  if (
    (passNumber == 1 && !runTwice) ||
    (passNumber == 2 && runTwice))
  {
    // Don't want to remove custom macros after each pass.
    SetFontSizes(10);
    for (auto& StringCustomMacroPointerPair : CustomMacroMap)
    {
      delete StringCustomMacroPointerPair.second;
    }
    CustomMacroMap.clear();
  }

  for (auto& StringTexRefPtrPair : TexReferences)
  {
    delete StringTexRefPtrPair.second;
  }
  TexReferences.clear();

  for (auto& StringBibEntryPointer : BibList)
  {
    delete StringBibEntryPointer.second;
  }
  BibList.clear();
  CitationList.clear();
  ResetTopicCounter();
}

//*****************************************************************************
// There is likely to be one set of macros used by all utilities.
//*****************************************************************************
void DefineDefaultMacros()
{
  // Put names which subsume other names at the TOP
  // so they get recognized first

  AddMacroDef(ltACCENT_GRAVE,        _T("`"), 1);
  AddMacroDef(ltACCENT_ACUTE,        _T("'"), 1);
  AddMacroDef(ltACCENT_CARET,        _T("^"), 1);
  AddMacroDef(ltACCENT_UMLAUT,       _T("\""), 1);
  AddMacroDef(ltACCENT_TILDE,        _T("~"), 1);
  AddMacroDef(ltACCENT_DOT,          _T("."), 1);
  AddMacroDef(ltACCENT_CADILLA,      _T("c"), 1);
  AddMacroDef(ltSMALLSPACE1,         _T(","), 0);
  AddMacroDef(ltSMALLSPACE2,         _T(";"), 0);

  AddMacroDef(ltABSTRACT,            _T("abstract"), 1);
  AddMacroDef(ltADDCONTENTSLINE,     _T("addcontentsline"), 3);
  AddMacroDef(ltADDTOCOUNTER,        _T("addtocounter"), 2);
  AddMacroDef(ltALEPH,               _T("aleph"), 0);
  AddMacroDef(ltALPHA,               _T("alpha"), 0);
  AddMacroDef(ltALPH1,               _T("alph"), 1);
  AddMacroDef(ltALPH2,               _T("Alph"), 1);
  AddMacroDef(ltANGLE,               _T("angle"), 0);
  AddMacroDef(ltAPPENDIX,            _T("appendix"), 0);
  AddMacroDef(ltAPPROX,              _T("approx"), 0);
  AddMacroDef(ltARABIC,              _T("arabic"), 1);
  AddMacroDef(ltARRAY,               _T("array"), 1);
  AddMacroDef(ltAST,                 _T("ast"), 0);
  AddMacroDef(ltASYMP,               _T("asymp"), 0);
  AddMacroDef(ltAUTHOR,              _T("author"), 1);

  AddMacroDef(ltBACKGROUNDCOLOUR,    _T("backgroundcolour"), 1);
  AddMacroDef(ltBACKGROUNDIMAGE,     _T("backgroundimage"), 1);
  AddMacroDef(ltBACKGROUND,          _T("background"), 1);
  AddMacroDef(ltBACKSLASHRAW,        _T("backslashraw"), 0);
  AddMacroDef(ltBACKSLASH,           _T("backslash"), 0);
  AddMacroDef(ltBASELINESKIP,        _T("baselineskip"), 1);
  AddMacroDef(ltBCOL,                _T("bcol"), 2);
  AddMacroDef(ltBETA,                _T("beta"), 0);
  AddMacroDef(ltBFSERIES,            _T("bfseries"), 1);
  AddMacroDef(ltBF,                  _T("bf"), 1);
  AddMacroDef(ltBIBITEM,             _T("bibitem"), 2);
             // For convenience, bibitem has 2 args: label and item.
                              // The Latex syntax permits writing as 2 args.
  AddMacroDef(ltBIBLIOGRAPHYSTYLE,   _T("bibliographystyle"), 1);
  AddMacroDef(ltBIBLIOGRAPHY,        _T("bibliography"), 1);
  AddMacroDef(ltBIGTRIANGLEDOWN,     _T("bigtriangledown"), 0);
  AddMacroDef(ltBOT,                 _T("bot"), 0);
  AddMacroDef(ltBOXIT,               _T("boxit"), 1);
  AddMacroDef(ltBOX,                 _T("box"), 0);
  AddMacroDef(ltBRCLEAR,             _T("brclear"), 0);
  AddMacroDef(ltBULLET,              _T("bullet"), 0);

  AddMacroDef(ltCAPTIONSTAR,         _T("caption*"), 1);
  AddMacroDef(ltCAPTION,             _T("caption"), 1);
  AddMacroDef(ltCAP,                 _T("cap"), 0);
  AddMacroDef(ltCDOTS,               _T("cdots"), 0);
  AddMacroDef(ltCDOT,                _T("cdot"), 0);
  AddMacroDef(ltCENTERLINE,          _T("centerline"), 1);
  AddMacroDef(ltCENTERING,           _T("centering"), 0);
  AddMacroDef(ltCENTER,              _T("center"), 1);
  AddMacroDef(ltCEXTRACT,            _T("cextract"), 0);
  AddMacroDef(ltCHAPTERHEADING,      _T("chapterheading"), 1);
  AddMacroDef(ltCHAPTERSTAR,         _T("chapter*"), 1);
  AddMacroDef(ltCHAPTER,             _T("chapter"), 1);
  AddMacroDef(ltCHI,                 _T("chi"), 0);
  AddMacroDef(ltCINSERT,             _T("cinsert"), 0);
  AddMacroDef(ltCIRC,                _T("circ"), 0);
  AddMacroDef(ltCITE,                _T("cite"), 1);
  AddMacroDef(ltCLASS,               _T("class"), 1);
  AddMacroDef(ltCLEARDOUBLEPAGE,     _T("cleardoublepage"), 0);
  AddMacroDef(ltCLEARPAGE,           _T("clearpage"), 0);
  AddMacroDef(ltCLINE,               _T("cline"), 1);
  AddMacroDef(ltCLIPSFUNC,           _T("clipsfunc"), 3);
  AddMacroDef(ltCLUBSUIT,            _T("clubsuit"), 0);
  AddMacroDef(ltCOLUMNSEP,           _T("columnsep"), 1);
  AddMacroDef(ltCOMMENT,             _T("comment"), 1, true);
  AddMacroDef(ltCONG,                _T("cong"), 0);
  AddMacroDef(ltCOPYRIGHT,           _T("copyright"), 0);
  AddMacroDef(ltCPARAM,              _T("cparam"), 2);
  AddMacroDef(ltCHEAD,               _T("chead"), 1);
  AddMacroDef(ltCFOOT,               _T("cfoot"), 1);
  AddMacroDef(ltCUP,                 _T("cup"), 0);

  AddMacroDef(ltDASHV,               _T("dashv"), 0);
  AddMacroDef(ltDATE,                _T("date"), 1);
  AddMacroDef(ltDELTA,               _T("delta"), 0);
  AddMacroDef(ltCAP_DELTA,           _T("Delta"), 0);
  AddMacroDef(ltDEFINECOLOUR,        _T("definecolour"), 4);
  AddMacroDef(ltDEFINECOLOR,         _T("definecolor"), 4);
  AddMacroDef(ltDESCRIPTION,         _T("description"), 1);
  AddMacroDef(ltDESTRUCT,            _T("destruct"), 1);
  AddMacroDef(ltDIAMOND2,            _T("diamond2"), 0);
  AddMacroDef(ltDIAMOND,             _T("diamond"), 0);
  AddMacroDef(ltDIV,                 _T("div"), 0);
  AddMacroDef(ltDOCUMENTCLASS,       _T("documentclass"), 1);
  AddMacroDef(ltDOCUMENTSTYLE,       _T("documentstyle"), 1);
  AddMacroDef(ltDOCUMENT,            _T("document"), 1);
  AddMacroDef(ltDOUBLESPACE,         _T("doublespace"), 1);
  AddMacroDef(ltDOTEQ,               _T("doteq"), 0);
  AddMacroDef(ltDOWNARROW,           _T("downarrow"), 0);
  AddMacroDef(ltDOWNARROW2,          _T("Downarrow"), 0);

  AddMacroDef(ltEMPTYSET,            _T("emptyset"), 0);
  AddMacroDef(ltEMPH,                _T("emph"), 1);
  AddMacroDef(ltEM,                  _T("em"), 1);
  AddMacroDef(ltENUMERATE,           _T("enumerate"), 1);
  AddMacroDef(ltEPSILON,             _T("epsilon"), 0);
  AddMacroDef(ltEQUATION,            _T("equation"), 1);
  AddMacroDef(ltEQUIV,               _T("equiv"), 0);
  AddMacroDef(ltETA,                 _T("eta"), 0);
  AddMacroDef(ltEVENSIDEMARGIN,      _T("evensidemargin"), 1);
  AddMacroDef(ltEXISTS,              _T("exists"), 0);

  AddMacroDef(ltFBOX,                _T("fbox"), 1);
  AddMacroDef(ltFCOL,                _T("fcol"), 2);
  AddMacroDef(ltFIGURE,              _T("figure"), 1);
  AddMacroDef(ltFIGURESTAR,          _T("figure*"), 1);
  AddMacroDef(ltFLUSHLEFT,           _T("flushleft"), 1);
  AddMacroDef(ltFLUSHRIGHT,          _T("flushright"), 1);
  AddMacroDef(ltFOLLOWEDLINKCOLOUR,  _T("followedlinkcolour"), 1);
  AddMacroDef(ltFOOTHEIGHT,          _T("footheight"), 1);
  AddMacroDef(ltFOOTNOTEPOPUP,       _T("footnotepopup"), 2);
  AddMacroDef(ltFOOTNOTE,            _T("footnote"), 1);
  AddMacroDef(ltFOOTSKIP,            _T("footskip"), 1);
  AddMacroDef(ltFORALL,              _T("forall"), 0);
  AddMacroDef(ltFRAMEBOX,            _T("framebox"), 1);
  AddMacroDef(ltFROWN,               _T("frown"), 0);
  AddMacroDef(ltFUNCTIONSECTION,     _T("functionsection"), 1);
  AddMacroDef(ltFUNC,                _T("func"), 3);
  AddMacroDef(ltFOOTNOTESIZE,        _T("footnotesize"), 0);
  AddMacroDef(ltFANCYPLAIN,          _T("fancyplain"), 2);

  AddMacroDef(ltGAMMA,               _T("gamma"), 0);
  AddMacroDef(ltCAP_GAMMA,           _T("Gamma"), 0);
  AddMacroDef(ltGEQ,                 _T("geq"), 0);
  AddMacroDef(ltGE,                  _T("ge"), 0);
  AddMacroDef(ltGG,                  _T("gg"), 0);
  AddMacroDef(ltGLOSSARY,            _T("glossary"), 1);
  AddMacroDef(ltGLOSS,               _T("gloss"), 1);

  AddMacroDef(ltHEADHEIGHT,          _T("headheight"), 1);
  AddMacroDef(ltHEARTSUIT,           _T("heartsuit"), 0);
  AddMacroDef(ltHELPGLOSSARY,        _T("helpglossary"), 1);
  AddMacroDef(ltHELPIGNORE,          _T("helpignore"), 1, true);
  AddMacroDef(ltHELPONLY,            _T("helponly"), 1);
  AddMacroDef(ltHELPINPUT,           _T("helpinput"), 1);
  AddMacroDef(ltHELPFONTFAMILY,      _T("helpfontfamily"), 1);
  AddMacroDef(ltHELPFONTSIZE,        _T("helpfontsize"), 1);
  AddMacroDef(ltHELPREFN,            _T("helprefn"), 2);
  AddMacroDef(ltHELPREF,             _T("helpref"), 2);
  AddMacroDef(ltHFILL,               _T("hfill"), 0);
  AddMacroDef(ltHLINE,               _T("hline"), 0);
  AddMacroDef(ltHRULE,               _T("hrule"), 0);
  AddMacroDef(ltHSPACESTAR,          _T("hspace*"), 1);
  AddMacroDef(ltHSPACE,              _T("hspace"), 1);
  AddMacroDef(ltHSKIPSTAR,           _T("hskip*"), 1);
  AddMacroDef(ltHSKIP,               _T("hskip"), 1);
  AddMacroDef(lthuge,                _T("huge"), 1);
  AddMacroDef(ltHuge,                _T("Huge"), 1);
  AddMacroDef(ltHUGE,                _T("HUGE"), 1);
  AddMacroDef(ltHTMLIGNORE,          _T("htmlignore"), 1);
  AddMacroDef(ltHTMLONLY,            _T("htmlonly"), 1);

  AddMacroDef(ltIM,                  _T("im"), 0);
  AddMacroDef(ltINCLUDEONLY,         _T("includeonly"), 1);
  AddMacroDef(ltINCLUDE,             _T("include"), 1);
  AddMacroDef(ltINDENTED,            _T("indented"), 2);
  AddMacroDef(ltINDEX,               _T("index"), 1);
  AddMacroDef(ltINPUT,               _T("input"), 1, true);
  AddMacroDef(ltIOTA,                _T("iota"), 0);
  AddMacroDef(ltITEMIZE,             _T("itemize"), 1);
  AddMacroDef(ltITEM,                _T("item"), 0);
  AddMacroDef(ltIMAGEMAP,            _T("imagemap"), 3);
  AddMacroDef(ltIMAGEL,              _T("imagel"), 2);
  AddMacroDef(ltIMAGER,              _T("imager"), 2);
  AddMacroDef(ltIMAGE,               _T("image"), 2);
  AddMacroDef(ltIN,                  _T("in"), 0);
  AddMacroDef(ltINFTY,               _T("infty"), 0);
  AddMacroDef(ltITSHAPE,             _T("itshape"), 1);
  AddMacroDef(ltIT,                  _T("it"), 1);
  AddMacroDef(ltITEMSEP,             _T("itemsep"), 1);
  AddMacroDef(ltINSERTATLEVEL,       _T("insertatlevel"), 2);

  AddMacroDef(ltKAPPA,               _T("kappa"), 0);
  AddMacroDef(ltKILL,                _T("kill"), 0);

  AddMacroDef(ltLABEL,               _T("label"), 1);
  AddMacroDef(ltLAMBDA,              _T("lambda"), 0);
  AddMacroDef(ltCAP_LAMBDA,          _T("Lambda"), 0);
  AddMacroDef(ltlarge,               _T("large"), 1);
  AddMacroDef(ltLarge,               _T("Large"), 1);
  AddMacroDef(ltLARGE,               _T("LARGE"), 1);
  AddMacroDef(ltLATEXIGNORE,         _T("latexignore"), 1);
  AddMacroDef(ltLATEXONLY,           _T("latexonly"), 1);
  AddMacroDef(ltLATEX,               _T("LaTeX"), 0);
  AddMacroDef(ltLBOX,                _T("lbox"), 1);
  AddMacroDef(ltLBRACERAW,           _T("lbraceraw"), 0);
  AddMacroDef(ltLDOTS,               _T("ldots"), 0);
  AddMacroDef(ltLEQ,                 _T("leq"), 0);
  AddMacroDef(ltLE,                  _T("le"), 0);
  AddMacroDef(ltLEFTARROW,           _T("leftarrow"), 0);
  AddMacroDef(ltLEFTRIGHTARROW,      _T("leftrightarrow"), 0);
  AddMacroDef(ltLEFTARROW2,          _T("Leftarrow"), 0);
  AddMacroDef(ltLEFTRIGHTARROW2,     _T("Leftrightarrow"), 0);
  AddMacroDef(ltLINEBREAK,           _T("linebreak"), 0);
  AddMacroDef(ltLINKCOLOUR,          _T("linkcolour"), 1);
  AddMacroDef(ltLISTOFFIGURES,       _T("listoffigures"), 0);
  AddMacroDef(ltLISTOFTABLES,        _T("listoftables"), 0);
  AddMacroDef(ltLHEAD,               _T("lhead"), 1);
  AddMacroDef(ltLFOOT,               _T("lfoot"), 1);
  AddMacroDef(ltLOWERCASE,           _T("lowercase"), 1);
  AddMacroDef(ltLL,                  _T("ll"), 0);

  AddMacroDef(ltMAKEGLOSSARY,        _T("makeglossary"), 0);
  AddMacroDef(ltMAKEINDEX,           _T("makeindex"), 0);
  AddMacroDef(ltMAKETITLE,           _T("maketitle"), 0);
  AddMacroDef(ltMARKRIGHT,           _T("markright"), 1);
  AddMacroDef(ltMARKBOTH,            _T("markboth"), 2);
  AddMacroDef(ltMARGINPARWIDTH,      _T("marginparwidth"), 1);
  AddMacroDef(ltMARGINPARSEP,        _T("marginparsep"), 1);
  AddMacroDef(ltMARGINPARODD,        _T("marginparodd"), 1);
  AddMacroDef(ltMARGINPAREVEN,       _T("marginpareven"), 1);
  AddMacroDef(ltMARGINPAR,           _T("marginpar"), 1);
  AddMacroDef(ltMBOX,                _T("mbox"), 1);
  AddMacroDef(ltMDSERIES,            _T("mdseries"), 1);
  AddMacroDef(ltMEMBERSECTION,       _T("membersection"), 1);
  AddMacroDef(ltMEMBER,              _T("member"), 2);
  AddMacroDef(ltMID,                 _T("mid"), 0);
  AddMacroDef(ltMODELS,              _T("models"), 0);
  AddMacroDef(ltMP,                  _T("mp"), 0);
  AddMacroDef(ltMULTICOLUMN,         _T("multicolumn"), 3);
  AddMacroDef(ltMU,                  _T("mu"), 0);

  AddMacroDef(ltNABLA,               _T("nabla"), 0);
  AddMacroDef(ltNEG,                 _T("neg"), 0);
  AddMacroDef(ltNEQ,                 _T("neq"), 0);
  AddMacroDef(ltNEWCOUNTER,          _T("newcounter"), 1, false, (bool)FORBID_ABSOLUTELY);
  AddMacroDef(ltNEWLINE,             _T("newline"), 0);
  AddMacroDef(ltNEWPAGE,             _T("newpage"), 0);
  AddMacroDef(ltNI,                  _T("ni"), 0);
  AddMacroDef(ltNOCITE,              _T("nocite"), 1);
  AddMacroDef(ltNOINDENT,            _T("noindent"), 0);
  AddMacroDef(ltNOLINEBREAK,         _T("nolinebreak"), 0);
  AddMacroDef(ltNOPAGEBREAK,         _T("nopagebreak"), 0);
  AddMacroDef(ltNORMALSIZE,          _T("normalsize"), 1);
  AddMacroDef(ltNORMALBOX,           _T("normalbox"), 1);
  AddMacroDef(ltNORMALBOXD,          _T("normalboxd"), 1);
  AddMacroDef(ltNOTEQ,               _T("noteq"), 0);
  AddMacroDef(ltNOTIN,               _T("notin"), 0);
  AddMacroDef(ltNOTSUBSET,           _T("notsubset"), 0);
  AddMacroDef(ltNU,                  _T("nu"), 0);

  AddMacroDef(ltODDSIDEMARGIN,       _T("oddsidemargin"), 1);
  AddMacroDef(ltOMEGA,               _T("omega"), 0);
  AddMacroDef(ltCAP_OMEGA,           _T("Omega"), 0);
  AddMacroDef(ltONECOLUMN,           _T("onecolumn"), 0);
  AddMacroDef(ltOPLUS,               _T("oplus"), 0);
  AddMacroDef(ltOSLASH,              _T("oslash"), 0);
  AddMacroDef(ltOTIMES,              _T("otimes"), 0);

  AddMacroDef(ltPAGEBREAK,           _T("pagebreak"), 0);
  AddMacroDef(ltPAGEREF,             _T("pageref"), 1);
  AddMacroDef(ltPAGESTYLE,           _T("pagestyle"), 1);
  AddMacroDef(ltPAGENUMBERING,       _T("pagenumbering"), 1);
  AddMacroDef(ltPARAGRAPHSTAR,       _T("paragraph*"), 1);
  AddMacroDef(ltPARAGRAPH,           _T("paragraph"), 1);
  AddMacroDef(ltPARALLEL,            _T("parallel"), 0);
  AddMacroDef(ltPARAM,               _T("param"), 2);
  AddMacroDef(ltPARINDENT,           _T("parindent"), 1);
  AddMacroDef(ltPARSKIP,             _T("parskip"), 1);
  AddMacroDef(ltPARTIAL,             _T("partial"), 0);
  AddMacroDef(ltPARTSTAR,            _T("part*"), 1);
  AddMacroDef(ltPART,                _T("part"), 1);
  AddMacroDef(ltPAR,                 _T("par"), 0);
  AddMacroDef(ltPERP,                _T("perp"), 0);
  AddMacroDef(ltPHI,                 _T("phi"), 0);
  AddMacroDef(ltCAP_PHI,             _T("Phi"), 0);
  AddMacroDef(ltPFUNC,               _T("pfunc"), 3);
  AddMacroDef(ltPICTURE,             _T("picture"), 1);
  AddMacroDef(ltPI,                  _T("pi"), 0);
  AddMacroDef(ltCAP_PI,              _T("Pi"), 0);
  AddMacroDef(ltPM,                  _T("pm"), 0);
  AddMacroDef(ltPOPREFONLY,          _T("poprefonly"), 1);
  AddMacroDef(ltPOPREF,              _T("popref"), 2);
  AddMacroDef(ltPOUNDS,              _T("pounds"), 0);
  AddMacroDef(ltPREC,                _T("prec"), 0);
  AddMacroDef(ltPRECEQ,              _T("preceq"), 0);
  AddMacroDef(ltPRINTINDEX,          _T("printindex"), 0);
  AddMacroDef(ltPROPTO,              _T("propto"), 0);
  AddMacroDef(ltPSBOXTO,             _T("psboxto"), 1, false, (bool)FORBID_ABSOLUTELY);
  AddMacroDef(ltPSBOX,               _T("psbox"), 1, false, (bool)FORBID_ABSOLUTELY);
  AddMacroDef(ltPSI,                 _T("psi"), 0);
  AddMacroDef(ltCAP_PSI,             _T("Psi"), 0);

  AddMacroDef(ltQUOTE,               _T("quote"), 1);
  AddMacroDef(ltQUOTATION,           _T("quotation"), 1);

  AddMacroDef(ltRAGGEDBOTTOM,        _T("raggedbottom"), 0);
  AddMacroDef(ltRAGGEDLEFT,          _T("raggedleft"), 0);
  AddMacroDef(ltRAGGEDRIGHT,         _T("raggedright"), 0);
  AddMacroDef(ltRBRACERAW,           _T("rbraceraw"), 0);
  AddMacroDef(ltREF,                 _T("ref"), 1);
  AddMacroDef(ltREGISTERED,          _T("registered"), 0);
  AddMacroDef(ltRE,                  _T("we"), 0);
  AddMacroDef(ltRHO,                 _T("rho"), 0);
  AddMacroDef(ltRIGHTARROW,          _T("rightarrow"), 0);
  AddMacroDef(ltRIGHTARROW2,         _T("rightarrow2"), 0);
  AddMacroDef(ltRMFAMILY,            _T("rmfamily"), 1);
  AddMacroDef(ltRM,                  _T("rm"), 1);
  AddMacroDef(ltROMAN,               _T("roman"), 1);
  AddMacroDef(ltROMAN2,              _T("Roman"), 1);
//  AddMacroDef(lt"row", 1);
  AddMacroDef(ltRTFSP,               _T("rtfsp"), 0);
  AddMacroDef(ltRTFIGNORE,           _T("rtfignore"), 1);
  AddMacroDef(ltRTFONLY,             _T("rtfonly"), 1);
  AddMacroDef(ltRULEDROW,            _T("ruledrow"), 1);
  AddMacroDef(ltDRULED,              _T("druled"), 1);
  AddMacroDef(ltRULE,                _T("rule"), 2);
  AddMacroDef(ltRHEAD,               _T("rhead"), 1);
  AddMacroDef(ltRFOOT,               _T("rfoot"), 1);
  AddMacroDef(ltROW,                 _T("row"), 1);

  AddMacroDef(ltSCSHAPE,             _T("scshape"), 1);
  AddMacroDef(ltSC,                  _T("sc"), 1);
  AddMacroDef(ltSECTIONHEADING,      _T("sectionheading"), 1);
  AddMacroDef(ltSECTIONSTAR,         _T("section*"), 1);
  AddMacroDef(ltSECTION,             _T("section"), 1);
  AddMacroDef(ltSETCOUNTER,          _T("setcounter"), 2);
  AddMacroDef(ltSFFAMILY,            _T("sffamily"), 1);
  AddMacroDef(ltSF,                  _T("sf"), 1);
  AddMacroDef(ltSHARP,               _T("sharp"), 0);
  AddMacroDef(ltSHORTCITE,           _T("shortcite"), 1);
  AddMacroDef(ltSIGMA,               _T("sigma"), 0);
  AddMacroDef(ltCAP_SIGMA,           _T("Sigma"), 0);
  AddMacroDef(ltSIM,                 _T("sim"), 0);
  AddMacroDef(ltSIMEQ,               _T("simeq"), 0);
  AddMacroDef(ltSINGLESPACE,         _T("singlespace"), 1);
  AddMacroDef(ltSIZEDBOX,            _T("sizedbox"), 2);
  AddMacroDef(ltSIZEDBOXD,           _T("sizedboxd"), 2);
  AddMacroDef(ltSLOPPYPAR,           _T("sloppypar"), 1);
  AddMacroDef(ltSLOPPY,              _T("sloppy"), 0);
  AddMacroDef(ltSLSHAPE,             _T("slshape"), 1);
  AddMacroDef(ltSL,                  _T("sl"), 1);
  AddMacroDef(ltSMALL,               _T("small"), 1);
  AddMacroDef(ltSMILE,               _T("smile"), 0);
  AddMacroDef(ltSS,                  _T("ss"), 0);
  AddMacroDef(ltSTAR,                _T("star"), 0);
  AddMacroDef(ltSUBITEM,             _T("subitem"), 0);
  AddMacroDef(ltSUBPARAGRAPHSTAR,    _T("subparagraph*"), 1);
  AddMacroDef(ltSUBPARAGRAPH,        _T("subparagraph"), 1);
  AddMacroDef(ltSPECIAL,             _T("special"), 1);
  AddMacroDef(ltSUBSECTIONSTAR,      _T("subsection*"), 1);
  AddMacroDef(ltSUBSECTION,          _T("subsection"), 1);
  AddMacroDef(ltSUBSETEQ,            _T("subseteq"), 0);
  AddMacroDef(ltSUBSET,              _T("subset"), 0);
  AddMacroDef(ltSUCC,                _T("succ"), 0);
  AddMacroDef(ltSUCCEQ,              _T("succeq"), 0);
  AddMacroDef(ltSUPSETEQ,            _T("supseteq"), 0);
  AddMacroDef(ltSUPSET,              _T("supset"), 0);
  AddMacroDef(ltSUBSUBSECTIONSTAR,   _T("subsubsection*"), 1);
  AddMacroDef(ltSUBSUBSECTION,       _T("subsubsection"), 1);
  AddMacroDef(ltSUPERTABULAR,        _T("supertabular"), 2, false);
  AddMacroDef(ltSURD,                _T("surd"), 0);
  AddMacroDef(ltSCRIPTSIZE,          _T("scriptsize"), 1);
  AddMacroDef(ltSETHEADER,           _T("setheader"), 6);
  AddMacroDef(ltSETFOOTER,           _T("setfooter"), 6);
  AddMacroDef(ltSETHOTSPOTCOLOUR,    _T("sethotspotcolour"), 1);
  AddMacroDef(ltSETHOTSPOTCOLOR,     _T("sethotspotcolor"), 1);
  AddMacroDef(ltSETHOTSPOTUNDERLINE, _T("sethotspotunderline"), 1);
  AddMacroDef(ltSETTRANSPARENCY,     _T("settransparency"), 1);
  AddMacroDef(ltSPADESUIT,           _T("spadesuit"), 0);

  AddMacroDef(ltTABBING,             _T("tabbing"), 2);
  AddMacroDef(ltTABLEOFCONTENTS,     _T("tableofcontents"), 0);
  AddMacroDef(ltTABLE,               _T("table"), 1);
  AddMacroDef(ltTABULAR,             _T("tabular"), 2, false);
  AddMacroDef(ltTAB,                 _T("tab"), 0);
  AddMacroDef(ltTAU,                 _T("tau"), 0);
  AddMacroDef(ltTEXTRM,              _T("textrm"), 1);
  AddMacroDef(ltTEXTSF,              _T("textsf"), 1);
  AddMacroDef(ltTEXTTT,              _T("texttt"), 1);
  AddMacroDef(ltTEXTBF,              _T("textbf"), 1);
  AddMacroDef(ltTEXTIT,              _T("textit"), 1);
  AddMacroDef(ltTEXTSL,              _T("textsl"), 1);
  AddMacroDef(ltTEXTSC,              _T("textsc"), 1);
  AddMacroDef(ltTEXTWIDTH,           _T("textwidth"), 1);
  AddMacroDef(ltTEXTHEIGHT,          _T("textheight"), 1);
  AddMacroDef(ltTEXTCOLOUR,          _T("textcolour"), 1);
  AddMacroDef(ltTEX,                 _T("TeX"), 0);
  AddMacroDef(ltTHEBIBLIOGRAPHY,     _T("thebibliography"), 2);
  AddMacroDef(ltTHETA,               _T("theta"), 0);
  AddMacroDef(ltTIMES,               _T("times"), 0);
  AddMacroDef(ltCAP_THETA,           _T("Theta"), 0);
  AddMacroDef(ltTITLEPAGE,           _T("titlepage"), 1);
  AddMacroDef(ltTITLE,               _T("title"), 1);
  AddMacroDef(ltTINY,                _T("tiny"), 1);
  AddMacroDef(ltTODAY,               _T("today"), 0);
  AddMacroDef(ltTOPMARGIN,           _T("topmargin"), 1);
  AddMacroDef(ltTOPSKIP,             _T("topskip"), 1);
  AddMacroDef(ltTRIANGLE,            _T("triangle"), 0);
  AddMacroDef(ltTTFAMILY,            _T("ttfamily"), 1);
  AddMacroDef(ltTT,                  _T("tt"), 1);
  AddMacroDef(ltTYPEIN,              _T("typein"), 1);
  AddMacroDef(ltTYPEOUT,             _T("typeout"), 1);
  AddMacroDef(ltTWOCOLWIDTHA,        _T("twocolwidtha"), 1);
  AddMacroDef(ltTWOCOLWIDTHB,        _T("twocolwidthb"), 1);
  AddMacroDef(ltTWOCOLSPACING,       _T("twocolspacing"), 1);
  AddMacroDef(ltTWOCOLITEMRULED,     _T("twocolitemruled"), 2);
  AddMacroDef(ltTWOCOLITEM,          _T("twocolitem"), 2);
  AddMacroDef(ltTWOCOLLIST,          _T("twocollist"), 1);
  AddMacroDef(ltTWOCOLUMN,           _T("twocolumn"), 0);
  AddMacroDef(ltTHEPAGE,             _T("thepage"), 0);
  AddMacroDef(ltTHECHAPTER,          _T("thechapter"), 0);
  AddMacroDef(ltTHESECTION,          _T("thesection"), 0);
  AddMacroDef(ltTHISPAGESTYLE,       _T("thispagestyle"), 1);

  AddMacroDef(ltUNDERLINE,           _T("underline"), 1);
  AddMacroDef(ltUPSILON,             _T("upsilon"), 0);
  AddMacroDef(ltCAP_UPSILON,         _T("Upsilon"), 0);
  AddMacroDef(ltUPARROW,             _T("uparrow"), 0);
  AddMacroDef(ltUPARROW2,            _T("Uparrow"), 0);
  AddMacroDef(ltUPPERCASE,           _T("uppercase"), 1);
  AddMacroDef(ltUPSHAPE,             _T("upshape"), 1);
  AddMacroDef(ltURLREF,              _T("urlref"), 2);
  AddMacroDef(ltUSEPACKAGE,          _T("usepackage"), 1);

  AddMacroDef(ltVAREPSILON,          _T("varepsilon"), 0);
  AddMacroDef(ltVARPHI,              _T("varphi"), 0);
  AddMacroDef(ltVARPI,               _T("varpi"), 0);
  AddMacroDef(ltVARRHO,              _T("varrho"), 0);
  AddMacroDef(ltVARSIGMA,            _T("varsigma"), 0);
  AddMacroDef(ltVARTHETA,            _T("vartheta"), 0);
  AddMacroDef(ltVDOTS,               _T("vdots"), 0);
  AddMacroDef(ltVEE,                 _T("vee"), 0);
  AddMacroDef(ltVERBATIMINPUT,       _T("verbatiminput"), 1);
  AddMacroDef(ltVERBATIM,            _T("verbatim"), 1);
  AddMacroDef(ltVERBSTAR,            _T("verb*"), 1);
  AddMacroDef(ltVERB,                _T("verb"), 1);
  AddMacroDef(ltVERSE,               _T("verse"), 1);
  AddMacroDef(ltVFILL,               _T("vfill"), 0);
  AddMacroDef(ltVLINE,               _T("vline"), 0);
  AddMacroDef(ltVOID,                _T("void"), 0);
  AddMacroDef(ltVDASH,               _T("vdash"), 0);
  AddMacroDef(ltVRULE,               _T("vrule"), 0);
  AddMacroDef(ltVSPACESTAR,          _T("vspace*"), 1);
  AddMacroDef(ltVSKIPSTAR,           _T("vskip*"), 1);
  AddMacroDef(ltVSPACE,              _T("vspace"), 1);
  AddMacroDef(ltVSKIP,               _T("vskip"), 1);

  AddMacroDef(ltWEDGE,               _T("wedge"), 0);
  AddMacroDef(ltWXCLIPS,             _T("wxclips"), 0);
  AddMacroDef(ltWINHELPIGNORE,       _T("winhelpignore"), 1);
  AddMacroDef(ltWINHELPONLY,         _T("winhelponly"), 1);
  AddMacroDef(ltWP,                  _T("wp"), 0);

  AddMacroDef(ltXI,                  _T("xi"), 0);
  AddMacroDef(ltCAP_XI,              _T("Xi"), 0);

  AddMacroDef(ltZETA,                _T("zeta"), 0);

  AddMacroDef(ltSPACE,               _T(" "), 0);
  AddMacroDef(ltBACKSLASHCHAR,       _T("\\"), 0);
  AddMacroDef(ltPIPE,                _T("|"), 0);
  AddMacroDef(ltFORWARDSLASH,        _T("/"), 0);
  AddMacroDef(ltUNDERSCORE,          _T("_"), 0);
  AddMacroDef(ltAMPERSAND,           _T("&"), 0);
  AddMacroDef(ltPERCENT,             _T("%"), 0);
  AddMacroDef(ltDOLLAR,              _T("$"), 0);
  AddMacroDef(ltHASH,                _T("#"), 0);
  AddMacroDef(ltLPARENTH,            _T("("), 0);
  AddMacroDef(ltRPARENTH,            _T(")"), 0);
  AddMacroDef(ltLBRACE,              _T("{"), 0);
  AddMacroDef(ltRBRACE,              _T("}"), 0);
//  AddMacroDef(ltEQUALS,              _T("="), 0);
  AddMacroDef(ltRANGLEBRA,           _T(">"), 0);
  AddMacroDef(ltLANGLEBRA,           _T("<"), 0);
  AddMacroDef(ltPLUS,                _T("+"), 0);
  AddMacroDef(ltDASH,                _T("-"), 0);
  AddMacroDef(ltAT_SYMBOL,           _T("@"), 0);
//  AddMacroDef(ltSINGLEQUOTE,         _T("'"), 0);
//  AddMacroDef(ltBACKQUOTE,           _T("`"), 0);
}

//*****************************************************************************
// Default behaviour, should be called by client if can't match locally.
// Called on start/end of macro examination
//*****************************************************************************
void DefaultOnMacro(int macroId, int no_args, bool start)
{
  switch (macroId)
  {
    // Default behaviour for abstract
    case ltABSTRACT:
    {
      if (start)
      {
        // Write the heading
        FakeCurrentSection(AbstractNameString);
        OnMacro(ltPAR, 0, true);
        OnMacro(ltPAR, 0, false);
      }
      else
      {
        if (DocumentStyle == LATEX_ARTICLE)
          sectionNo --;
        else
          chapterNo --;
      }
      break;
    }

    // Default behaviour for glossary
    case ltHELPGLOSSARY:
    {
      if (start)
      {
        // Write the heading
        FakeCurrentSection(GlossaryNameString);
        OnMacro(ltPAR, 0, true);
        OnMacro(ltPAR, 0, false);
        if ((convertMode == TEX_RTF) && !winHelp)
        {
          OnMacro(ltPAR, 0, true);
          OnMacro(ltPAR, 0, false);
        }
      }
      break;
    }
    case ltSPECIALAMPERSAND:
      if (start)
        TexOutput(_T("  "));
      break;

    case ltCINSERT:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&lt;&lt;"));
        else
            TexOutput(_T("<<"), true);
      }
      break;
    case ltCEXTRACT:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&gt;&gt;"));
        else
            TexOutput(_T(">>"), true);
      }
      break;
    case ltDESTRUCT:
      if (start)
        TexOutput(_T("~"), true);
      break;
    case ltTILDE:
      if (start)
        TexOutput(_T("~"), true);
      break;
    case ltSPECIALTILDE:
      if (start)
        TexOutput(_T(" "), true);
      break;
    case ltUNDERSCORE:
      if (start)
        TexOutput(_T("_"), true);
      break;
    case ltHASH:
      if (start)
        TexOutput(_T("#"), true);
      break;
    case ltAMPERSAND:
      if (start)
        TexOutput(_T("&"), true);
      break;
    case ltSPACE:
      if (start)
        TexOutput(_T(" "), true);
      break;
    case ltPIPE:
      if (start)
        TexOutput(_T("|"), true);
      break;
    case ltPERCENT:
      if (start)
        TexOutput(_T("%"), true);
      break;
    case ltDOLLAR:
      if (start)
        TexOutput(_T("$"), true);
      break;
    case ltLPARENTH:
      if (start)
        TexOutput(_T(""), true);
      break;
    case ltRPARENTH:
      if (start)
        TexOutput(_T(""), true);
      break;
    case ltLBRACE:
      if (start)
        TexOutput(_T("{"), true);
      break;
    case ltRBRACE:
      if (start)
        TexOutput(_T("}"), true);
      break;
    case ltCOPYRIGHT:
      if (start)
        TexOutput(_T("(c)"), true);
      break;
    case ltREGISTERED:
      if (start)
        TexOutput(_T("(r)"), true);
      break;
    case ltBACKSLASH:
      if (start)
        TexOutput(_T("\\"), true);
      break;
    case ltLDOTS:
    case ltCDOTS:
      if (start)
        TexOutput(_T("..."), true);
      break;
    case ltVDOTS:
      if (start)
        TexOutput(_T("|"), true);
      break;
    case ltLATEX:
      if (start)
        TexOutput(_T("LaTeX"), true);
      break;
    case ltTEX:
      if (start)
        TexOutput(_T("TeX"), true);
      break;
    case ltPOUNDS:
      if (start)
        // The pound sign has Unicode code point U+00A3.
        TexOutput(wxString(L"\u00A3"), true);
      break;
    case ltSPECIALDOUBLEDOLLAR:  // Interpret as center
      OnMacro(ltCENTER, no_args, start);
      break;
    case ltEMPH:
    case ltTEXTSL:
    case ltSLSHAPE:
    case ltSL:
      OnMacro(ltIT, no_args, start);
      break;
    case ltPARAGRAPH:
    case ltPARAGRAPHSTAR:
    case ltSUBPARAGRAPH:
    case ltSUBPARAGRAPHSTAR:
      OnMacro(ltSUBSUBSECTION, no_args, start);
      break;
    case ltTODAY:
    {
      if (start)
      {
        time_t when;
        time(&when);
        TexOutput(wxCtime(&when), true);
      }
      break;
    }
    case ltNOINDENT:
      if (start)
        ParIndent = 0;
      break;

    // Symbols
    case ltALPHA:
      if (start) TexOutput(_T("alpha"));
      break;
    case ltBETA:
      if (start) TexOutput(_T("beta"));
      break;
    case ltGAMMA:
      if (start) TexOutput(_T("gamma"));
      break;
    case ltDELTA:
      if (start) TexOutput(_T("delta"));
      break;
    case ltEPSILON:
    case ltVAREPSILON:
      if (start) TexOutput(_T("epsilon"));
      break;
    case ltZETA:
      if (start) TexOutput(_T("zeta"));
      break;
    case ltETA:
      if (start) TexOutput(_T("eta"));
      break;
    case ltTHETA:
    case ltVARTHETA:
      if (start) TexOutput(_T("theta"));
      break;
    case ltIOTA:
      if (start) TexOutput(_T("iota"));
      break;
    case ltKAPPA:
      if (start) TexOutput(_T("kappa"));
      break;
    case ltLAMBDA:
      if (start) TexOutput(_T("lambda"));
      break;
    case ltMU:
      if (start) TexOutput(_T("mu"));
      break;
    case ltNU:
      if (start) TexOutput(_T("nu"));
      break;
    case ltXI:
      if (start) TexOutput(_T("xi"));
      break;
    case ltPI:
    case ltVARPI:
      if (start) TexOutput(_T("pi"));
      break;
    case ltRHO:
    case ltVARRHO:
      if (start) TexOutput(_T("rho"));
      break;
    case ltSIGMA:
    case ltVARSIGMA:
      if (start) TexOutput(_T("sigma"));
      break;
    case ltTAU:
      if (start) TexOutput(_T("tau"));
      break;
    case ltUPSILON:
      if (start) TexOutput(_T("upsilon"));
      break;
    case ltPHI:
    case ltVARPHI:
      if (start) TexOutput(_T("phi"));
      break;
    case ltCHI:
      if (start) TexOutput(_T("chi"));
      break;
    case ltPSI:
      if (start) TexOutput(_T("psi"));
      break;
    case ltOMEGA:
      if (start) TexOutput(_T("omega"));
      break;
    case ltCAP_GAMMA:
      if (start) TexOutput(_T("GAMMA"));
      break;
    case ltCAP_DELTA:
      if (start) TexOutput(_T("DELTA"));
      break;
    case ltCAP_THETA:
      if (start) TexOutput(_T("THETA"));
      break;
    case ltCAP_LAMBDA:
      if (start) TexOutput(_T("LAMBDA"));
      break;
    case ltCAP_XI:
      if (start) TexOutput(_T("XI"));
      break;
    case ltCAP_PI:
      if (start) TexOutput(_T("PI"));
      break;
    case ltCAP_SIGMA:
      if (start) TexOutput(_T("SIGMA"));
      break;
    case ltCAP_UPSILON:
      if (start) TexOutput(_T("UPSILON"));
      break;
    case ltCAP_PHI:
      if (start) TexOutput(_T("PHI"));
      break;
    case ltCAP_PSI:
      if (start) TexOutput(_T("PSI"));
      break;
    case ltCAP_OMEGA:
      if (start) TexOutput(_T("OMEGA"));
      break;

    // Binary operation symbols
    case ltLE:
    case ltLEQ:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&lt;="));
        else
            TexOutput(_T("<="));
      }
      break;
    case ltLL:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&lt;&lt;"));
        else
            TexOutput(_T("<<"));
      }
      break;
    case ltSUBSET:
      if (start) TexOutput(_T("SUBSET"));
      break;
    case ltSUBSETEQ:
      if (start) TexOutput(_T("SUBSETEQ"));
      break;
    case ltIN:
      if (start) TexOutput(_T("IN"));
      break;
    case ltVDASH:
      if (start) TexOutput(_T("VDASH"));
      break;
    case ltMODELS:
      if (start) TexOutput(_T("MODELS"));
      break;
    case ltGE:
    case ltGEQ:
    {
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&gt;="));
        else
            TexOutput(_T(">="));
      }
      break;
    }
    case ltGG:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&gt;&gt;"));
        else
            TexOutput(_T(">>"));
      }
      break;
    case ltSUPSET:
      if (start) TexOutput(_T("SUPSET"));
      break;
    case ltSUPSETEQ:
      if (start) TexOutput(_T("SUPSETEQ"));
      break;
    case ltNI:
      if (start) TexOutput(_T("NI"));
      break;
    case ltDASHV:
      if (start) TexOutput(_T("DASHV"));
      break;
    case ltPERP:
      if (start) TexOutput(_T("PERP"));
      break;
    case ltNEQ:
      if (start) TexOutput(_T("NEQ"));
      break;
    case ltDOTEQ:
      if (start) TexOutput(_T("DOTEQ"));
      break;
    case ltAPPROX:
      if (start) TexOutput(_T("APPROX"));
      break;
    case ltCONG:
      if (start) TexOutput(_T("CONG"));
      break;
    case ltEQUIV:
      if (start) TexOutput(_T("EQUIV"));
      break;
    case ltPROPTO:
      if (start) TexOutput(_T("PROPTO"));
      break;
    case ltPREC:
      if (start) TexOutput(_T("PREC"));
      break;
    case ltPRECEQ:
      if (start) TexOutput(_T("PRECEQ"));
      break;
    case ltPARALLEL:
      if (start) TexOutput(_T("|"));
      break;
    case ltSIM:
      if (start) TexOutput(_T("~"));
      break;
    case ltSIMEQ:
      if (start) TexOutput(_T("SIMEQ"));
      break;
    case ltASYMP:
      if (start) TexOutput(_T("ASYMP"));
      break;
    case ltSMILE:
      if (start) TexOutput(_T(":-)"));
      break;
    case ltFROWN:
      if (start) TexOutput(_T(":-("));
      break;
    case ltSUCC:
      if (start) TexOutput(_T("SUCC"));
      break;
    case ltSUCCEQ:
      if (start) TexOutput(_T("SUCCEQ"));
      break;
    case ltMID:
      if (start) TexOutput(_T("|"));
      break;

    // Negated relation symbols
    case ltNOTEQ:
      if (start) TexOutput(_T("!="));
      break;
    case ltNOTIN:
      if (start) TexOutput(_T("NOTIN"));
      break;
    case ltNOTSUBSET:
      if (start) TexOutput(_T("NOTSUBSET"));
      break;

    // Arrows
    case ltLEFTARROW:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&lt;--"));
        else
            TexOutput(_T("<--"));
      }
      break;
    case ltLEFTARROW2:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&lt;=="));
        else
            TexOutput(_T("<=="));
      }
      break;
    case ltRIGHTARROW:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("--&gt;"));
        else
            TexOutput(_T("-->"));
      }
      break;
    case ltRIGHTARROW2:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("==&gt;"));
        else
            TexOutput(_T("==>"));
      }
      break;
    case ltLEFTRIGHTARROW:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&lt;--&gt;"));
        else
            TexOutput(_T("<-->"));
      }
      break;
    case ltLEFTRIGHTARROW2:
      if (start)
      {
        if (convertMode == TEX_HTML)
            TexOutput(_T("&lt;==&gt;"));
        else
            TexOutput(_T("<==>"));
      }
      break;
    case ltUPARROW:
      if (start) TexOutput(_T("UPARROW"));
      break;
    case ltUPARROW2:
      if (start) TexOutput(_T("UPARROW2"));
      break;
    case ltDOWNARROW:
      if (start) TexOutput(_T("DOWNARROW"));
      break;
    case ltDOWNARROW2:
      if (start) TexOutput(_T("DOWNARROW2"));
      break;
    // Miscellaneous symbols
    case ltALEPH:
      if (start) TexOutput(_T("ALEPH"));
      break;
    case ltWP:
      if (start) TexOutput(_T("WP"));
      break;
    case ltRE:
      if (start) TexOutput(_T("RE"));
      break;
    case ltIM:
      if (start) TexOutput(_T("IM"));
      break;
    case ltEMPTYSET:
      if (start) TexOutput(_T("EMPTYSET"));
      break;
    case ltNABLA:
      if (start) TexOutput(_T("NABLA"));
      break;
    case ltSURD:
      if (start) TexOutput(_T("SURD"));
      break;
    case ltPARTIAL:
      if (start) TexOutput(_T("PARTIAL"));
      break;
    case ltBOT:
      if (start) TexOutput(_T("BOT"));
      break;
    case ltFORALL:
      if (start) TexOutput(_T("FORALL"));
      break;
    case ltEXISTS:
      if (start) TexOutput(_T("EXISTS"));
      break;
    case ltNEG:
      if (start) TexOutput(_T("NEG"));
      break;
    case ltSHARP:
      if (start) TexOutput(_T("SHARP"));
      break;
    case ltANGLE:
      if (start) TexOutput(_T("ANGLE"));
      break;
    case ltTRIANGLE:
      if (start) TexOutput(_T("TRIANGLE"));
      break;
    case ltCLUBSUIT:
      if (start) TexOutput(_T("CLUBSUIT"));
      break;
    case ltDIAMONDSUIT:
      if (start) TexOutput(_T("DIAMONDSUIT"));
      break;
    case ltHEARTSUIT:
      if (start) TexOutput(_T("HEARTSUIT"));
      break;
    case ltSPADESUIT:
      if (start) TexOutput(_T("SPADESUIT"));
      break;
    case ltINFTY:
      if (start) TexOutput(_T("INFTY"));
      break;
    case ltPM:
      if (start) TexOutput(_T("PM"));
      break;
    case ltMP:
      if (start) TexOutput(_T("MP"));
      break;
    case ltTIMES:
      if (start) TexOutput(_T("TIMES"));
      break;
    case ltDIV:
      if (start) TexOutput(_T("DIV"));
      break;
    case ltCDOT:
      if (start) TexOutput(_T("CDOT"));
      break;
    case ltAST:
      if (start) TexOutput(_T("AST"));
      break;
    case ltSTAR:
      if (start) TexOutput(_T("STAR"));
      break;
    case ltCAP:
      if (start) TexOutput(_T("CAP"));
      break;
    case ltCUP:
      if (start) TexOutput(_T("CUP"));
      break;
    case ltVEE:
      if (start) TexOutput(_T("VEE"));
      break;
    case ltWEDGE:
      if (start) TexOutput(_T("WEDGE"));
      break;
    case ltCIRC:
      if (start) TexOutput(_T("CIRC"));
      break;
    case ltBULLET:
      if (start) TexOutput(_T("BULLET"));
      break;
    case ltDIAMOND:
      if (start) TexOutput(_T("DIAMOND"));
      break;
    case ltOSLASH:
      if (start) TexOutput(_T("OSLASH"));
      break;
    case ltBOX:
      if (start) TexOutput(_T("BOX"));
      break;
    case ltDIAMOND2:
      if (start) TexOutput(_T("DIAMOND2"));
      break;
    case ltBIGTRIANGLEDOWN:
      if (start) TexOutput(_T("BIGTRIANGLEDOWN"));
      break;
    case ltOPLUS:
      if (start) TexOutput(_T("OPLUS"));
      break;
    case ltOTIMES:
      if (start) TexOutput(_T("OTIMES"));
      break;
    case ltSS:
      if (start) TexOutput(_T("s"));
      break;
    case ltBACKSLASHRAW:
      if (start) TexOutput(_T("\\"));
      break;
    case ltLBRACERAW:
      if (start) TexOutput(_T("{"));
      break;
    case ltRBRACERAW:
      if (start) TexOutput(_T("}"));
      break;
    case ltSMALLSPACE1:
    case ltSMALLSPACE2:
      if (start) TexOutput(_T(" "));
      break;
    default:
      break;
  }
}

//*****************************************************************************
// Called on start/end of argument examination
//*****************************************************************************
bool DefaultOnArgument(int macroId, int arg_no, bool start)
{
  switch (macroId)
  {
    case ltREF:
    {
    if (arg_no == 1 && start)
    {
      wxString refName = GetArgData();
      if (!refName.empty())
      {
        TexRef* texRef = FindReference(refName);
        if (texRef)
        {
          // Must strip the 'section' or 'chapter' or 'figure' text
          // from a normal 'ref' reference
          wxString buf = texRef->sectionNumber;
          size_t len = buf.length();
          size_t i = 0;
          if (buf != "??")
          {
            while (i < len)
            {
              if (buf[i] == ' ')
              {
                ++i;
                break;
              }
              else
              {
                ++i;
              }
            }
          }
          TexOutput(texRef->sectionNumber.Mid(i), true);
        }
        else
        {
          wxString informBuf;
          informBuf << "Warning: unresolved reference \"" << refName << '"';
          OnInform(informBuf);
        }
      }
      else TexOutput(_T("??"), true);
      return false;
    }
    break;
    }
    case ltLABEL:
    {
      return false;
    }
    case ltAUTHOR:
    {
      if (start && (arg_no == 1))
        DocumentAuthor = GetArgChunk();
      return false;
    }
    case ltDATE:
    {
      if (start && (arg_no == 1))
        DocumentDate = GetArgChunk();
      return false;
    }
    case ltTITLE:
    {
      if (start && (arg_no == 1))
        DocumentTitle = GetArgChunk();
      return false;
    }
  case ltDOCUMENTCLASS:
  case ltDOCUMENTSTYLE:
  {
    if (start && !IsArgOptional())
    {
      DocumentStyleString = GetArgData();
      if (DocumentStyleString == "art")
      {
        DocumentStyle = LATEX_ARTICLE;
      }
      else if (DocumentStyleString == "rep")
      {
        DocumentStyle = LATEX_REPORT;
      }
      else if (
        DocumentStyleString == "book" || DocumentStyleString == "thesis")
      {
        DocumentStyle = LATEX_BOOK;
      }
      else if (DocumentStyleString == "letter")
      {
        DocumentStyle = LATEX_LETTER;
      }
      else if (DocumentStyleString == "slides")
      {
        DocumentStyle = LATEX_SLIDES;
      }

      if (DocumentStyleString == "10")
      {
        SetFontSizes(10);
      }
      else if (DocumentStyleString == "11")
      {
        SetFontSizes(11);
      }
      else if (DocumentStyleString == "12")
      {
        SetFontSizes(12);
      }

      OnMacro(ltHELPFONTSIZE, 1, true);
      currentArgData.clear();
      currentArgData << normalFont;
      haveArgData = true;
      OnArgument(ltHELPFONTSIZE, 1, true);
      OnArgument(ltHELPFONTSIZE, 1, false);
      haveArgData = false;
      OnMacro(ltHELPFONTSIZE, 1, false);
    }
    else if (start && IsArgOptional())
    {
      MinorDocumentStyleString = GetArgData();

      if (MinorDocumentStyleString == "10")
      {
        SetFontSizes(10);
      }
      else if (MinorDocumentStyleString == "11")
      {
        SetFontSizes(11);
      }
      else if (MinorDocumentStyleString == "12")
      {
        SetFontSizes(12);
      }
    }
    return false;
  }
  case ltBIBLIOGRAPHYSTYLE:
  {
    if (start && !IsArgOptional())
    {
      BibliographyStyleString = GetArgData();
    }
    return false;
  }
  case ltPAGESTYLE:
  {
    if (start && !IsArgOptional())
    {
      PageStyle = GetArgData();
    }
    return false;
  }
//  case ltLHEAD:
//  {
//    if (start && !IsArgOptional())
//      LeftHeader = GetArgChunk();
//    return false;
//    break;
//  }
//  case ltLFOOT:
//  {
//    if (start && !IsArgOptional())
//      LeftFooter = GetArgChunk();
//    return false;
//    break;
//  }
//  case ltCHEAD:
//  {
//    if (start && !IsArgOptional())
//      CentreHeader = GetArgChunk();
//    return false;
//    break;
//  }
//  case ltCFOOT:
//  {
//    if (start && !IsArgOptional())
//      CentreFooter = GetArgChunk();
//    return false;
//    break;
//  }
//  case ltRHEAD:
//  {
//    if (start && !IsArgOptional())
//      RightHeader = GetArgChunk();
//    return false;
//    break;
//  }
//  case ltRFOOT:
//  {
//    if (start && !IsArgOptional())
//      RightFooter = GetArgChunk();
//    return false;
//    break;
//  }
  case ltCITE:
  case ltSHORTCITE:
  {
    if (start && !IsArgOptional())
    {
      wxString citeKeys = GetArgData();
      size_t pos = 0;
      wxChar* citeKey = ParseMultifieldString(citeKeys, &pos);
      while (citeKey)
      {
        AddCitation(citeKey);
        TexRef* ref = FindReference(citeKey);
        if (ref)
        {
          TexOutput(ref->sectionNumber, true);
          if (wxStrcmp(ref->sectionNumber, _T("??")) == 0)
          {
            wxString informBuf;
            informBuf << "Warning: unresolved citation " << citeKey << '.';
            OnInform(informBuf);
          }
        }
        citeKey = ParseMultifieldString(citeKeys, &pos);
        if (citeKey)
        {
          TexOutput(_T(", "), true);
        }
      }
      return false;
    }
    break;
  }
  case ltNOCITE:
  {
    if (start && !IsArgOptional())
    {
      wxString citeKey = GetArgData();
      AddCitation(citeKey);
      return false;
    }
    break;
  }
  case ltHELPFONTSIZE:
  {
    if (start)
    {
      wxString data = GetArgData();
      if (data == "10")
        SetFontSizes(10);
      else if (data == "11")
        SetFontSizes(11);
      else if (data == "12")
        SetFontSizes(12);
      return false;
    }
    break;
  }
  case ltPAGEREF:
  {
    if (start)
    {
      TexOutput(_T(" ??"), true);
      return false;
    }
    break;
  }
  case ltPARSKIP:
  {
    if (start && arg_no == 1)
    {
      wxString data = GetArgData();
      ParSkip = ParseUnitArgument(data);
      return false;
    }
    break;
  }
  case ltPARINDENT:
  {
    if (start && arg_no == 1)
    {
      wxString data = GetArgData();
      ParIndent = ParseUnitArgument(data);
      return false;
    }
    break;
  }
  case ltSL:
  {
    return OnArgument(ltIT, arg_no, start);
  }
  case ltSPECIALDOUBLEDOLLAR:
  {
    return OnArgument(ltCENTER, arg_no, start);
  }
  case ltPARAGRAPH:
  case ltPARAGRAPHSTAR:
  case ltSUBPARAGRAPH:
  case ltSUBPARAGRAPHSTAR:
  {
    return OnArgument(ltSUBSUBSECTION, arg_no, start);
  }
  case ltTYPEOUT:
  {
    if (start)
      OnInform(GetArgData());
    break;
  }
  case ltFOOTNOTE:
  {
    if (start)
      TexOutput(_T(" ("), true);
    else
      TexOutput(_T(")"), true);
    break;
  }
  case ltBIBLIOGRAPHY:
  {
    if (start)
    {
      int ch;
      wxChar smallBuf[2];
      smallBuf[1] = 0;
      FILE* fd = wxFopen(TexBibName, _T("r"));
      if (fd)
      {
        ch = getc(fd);
        smallBuf[0] = (wxChar)ch;
        while (ch != EOF)
        {
          TexOutput(smallBuf);
          ch = getc(fd);
          smallBuf[0] = (wxChar)ch;
        }
        fclose(fd);
      }
      else
      {
        OnInform(_T("Run Tex2RTF again to include bibliography."));
      }

      // Read in the .bib file, resolve all known references, write out the RTF.
      wxString allFiles = GetArgData();
      size_t pos = 0;
      wxChar* bibFile = ParseMultifieldString(allFiles, &pos);
      while (bibFile)
      {
        wxChar fileBuf[300];
        wxStrcpy(fileBuf, bibFile);
        wxString actualFile = TexPathList.FindValidPath(fileBuf);
        if (actualFile.empty())
        {
          wxStrcat(fileBuf, _T(".bib"));
          actualFile = TexPathList.FindValidPath(fileBuf);
        }
        if (!actualFile.empty())
        {
          if (!ReadBib(actualFile))
          {
            wxString ErrorMessage;
            ErrorMessage
              << ".bib file " << actualFile << " not found or malformed";
            OnError(ErrorMessage);
          }
        }
        else
        {
          wxString ErrorMessage;
          ErrorMessage.Printf(_T(".bib file %s not found"), fileBuf);
          OnError(ErrorMessage);
        }
        bibFile = ParseMultifieldString(allFiles, &pos);
      }

      ResolveBibReferences();

      // Write it a new bib section in the appropriate format.
      FILE* save1 = CurrentOutput1;
      FILE* save2 = CurrentOutput2;
      FILE* Biblio = wxFopen(TexTmpBibName, _T("w"));
      SetCurrentOutput(Biblio);
      OutputBib();
      fclose(Biblio);
      if (wxFileExists(TexTmpBibName))
      {
        if (wxFileExists(TexBibName))
        {
          wxRemoveFile(TexBibName);
        }
        wxRenameFile(TexTmpBibName, TexBibName);
      }
      SetCurrentOutputs(save1, save2);
      return false;
    }
    break;
  }
  case ltMULTICOLUMN:
    return (start && (arg_no == 3));
  case ltSCSHAPE:
  case ltTEXTSC:
  case ltSC:
  {
    if (start && (arg_no == 1))
    {
      wxString s = GetArgData();
      if (!s.empty())
      {
        wxString Upper = s.MakeUpper();
        TexOutput(Upper);
        return false;
      }
      else return true;

    }
    return true;
  }
  case ltLOWERCASE:
  {
    if (start && (arg_no == 1))
    {
      wxString s = GetArgData();
      if (!s.empty())
      {
        wxString Lower = s.MakeLower();
        TexOutput(Lower);
        return false;
      }
      else return true;

    }
    return true;
  }
  case ltUPPERCASE:
  {
    if (start && (arg_no == 1))
    {
      wxString s = GetArgData();
      if (!s.empty())
      {
        wxString Upper = s.MakeUpper();
        TexOutput(Upper);
        return false;
      }
      else return true;

    }
    return true;
  }
  case ltPOPREF:  // Ignore second argument by default
    return (start && (arg_no == 1));
  case ltTWOCOLUMN:
    return true;
  case ltHTMLIGNORE:
    return ((convertMode == TEX_HTML) ? false : true);
  case ltHTMLONLY:
    return ((convertMode != TEX_HTML) ? false : true);
  case ltRTFIGNORE:
    return (((convertMode == TEX_RTF) && !winHelp) ? false : true);
  case ltRTFONLY:
    return (!((convertMode == TEX_RTF) && !winHelp) ? false : true);
  case ltWINHELPIGNORE:
    return (winHelp ? false : true);
  case ltWINHELPONLY:
    return (!winHelp ? false : true);
  case ltLATEXIGNORE:
    return true;
  case ltLATEXONLY:
    return false;
  case ltCLINE:
  case ltARABIC:
  case ltALPH1:
  case ltALPH2:
  case ltROMAN:
  case ltROMAN2:
  case ltSETCOUNTER:
  case ltADDTOCOUNTER:
  case ltADDCONTENTSLINE:
  case ltNEWCOUNTER:
  case ltTEXTWIDTH:
  case ltTEXTHEIGHT:
  case ltBASELINESKIP:
  case ltVSPACESTAR:
  case ltHSPACESTAR:
  case ltVSPACE:
  case ltHSPACE:
  case ltVSKIPSTAR:
  case ltHSKIPSTAR:
  case ltVSKIP:
  case ltHSKIP:
  case ltPAGENUMBERING:
  case ltTHEPAGE:
  case ltTHECHAPTER:
  case ltTHESECTION:
  case ltITEMSEP:
  case ltFANCYPLAIN:
  case ltCHEAD:
  case ltRHEAD:
  case ltLHEAD:
  case ltCFOOT:
  case ltRFOOT:
  case ltLFOOT:
  case ltTHISPAGESTYLE:
  case ltMARKRIGHT:
  case ltMARKBOTH:
  case ltEVENSIDEMARGIN:
  case ltODDSIDEMARGIN:
  case ltMARGINPAR:
  case ltMARGINPARWIDTH:
  case ltMARGINPARSEP:
  case ltMARGINPAREVEN:
  case ltMARGINPARODD:
  case ltTWOCOLWIDTHA:
  case ltTWOCOLWIDTHB:
  case ltTWOCOLSPACING:
  case ltSETHEADER:
  case ltSETFOOTER:
  case ltINDEX:
  case ltITEM:
  case ltBCOL:
  case ltFCOL:
  case ltSETHOTSPOTCOLOUR:
  case ltSETHOTSPOTCOLOR:
  case ltSETHOTSPOTUNDERLINE:
  case ltSETTRANSPARENCY:
  case ltUSEPACKAGE:
  case ltBACKGROUND:
  case ltBACKGROUNDCOLOUR:
  case ltBACKGROUNDIMAGE:
  case ltLINKCOLOUR:
  case ltFOLLOWEDLINKCOLOUR:
  case ltTEXTCOLOUR:
  case ltIMAGE:
  case ltIMAGEMAP:
  case ltIMAGEL:
  case ltIMAGER:
  case ltPOPREFONLY:
  case ltINSERTATLEVEL:
    return false;
  case ltTABULAR:
  case ltSUPERTABULAR:
  case ltINDENTED:
  case ltSIZEDBOX:
  case ltSIZEDBOXD:
    return (arg_no == 2);
  case ltDEFINECOLOUR:
  case ltDEFINECOLOR:
  {
    static int redVal = 0;
    static int greenVal = 0;
    static int blueVal = 0;
    static wxString colourName;
    if (start)
    {
      switch (arg_no)
      {
        case 1:
        {
          colourName = GetArgData();
          break;
        }
        case 2:
        {
          redVal = wxAtoi(GetArgData());
          break;
        }
        case 3:
        {
          greenVal = wxAtoi(GetArgData());
          break;
        }
        case 4:
        {
          blueVal = wxAtoi(GetArgData());
          AddColour(colourName, redVal, greenVal, blueVal);
          break;
        }
        default:
          break;
      }
    }
    return false;
  }
  case ltFIGURE:
  case ltFIGURESTAR:
  case ltNORMALBOX:
  case ltNORMALBOXD:
  default:
    return (!IsArgOptional());
  }
  return true;
}
