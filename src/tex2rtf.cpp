//*****************************************************************************
// Copyright:   (c) Julian Smart
// Author:      Julian Smart
// Modified by: Wlodzimiez ABX Skiba 2003/2004 Unicode support
//              Ron Lee
// Created:     7/9/1993
// License:     wxWindows license
// Description:
//   Converts Latex to linear/WinHelp RTF, HTML, wxHelp.
//*****************************************************************************

#if defined(__WXMSW__)
#include "wx/msw/wrapwin.h"
#endif

#ifndef WX_PRECOMP
#ifndef NO_GUI
#include <wx/menu.h>
#include <wx/textctrl.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/icon.h>
#endif
#endif

#include <wx/log.h>

#ifndef NO_GUI
#include <wx/timer.h>
#include <wx/help.h>
#include <wx/cshelp.h>
#include <wx/helphtml.h>
#ifdef __WXMSW__
#include <wx/msw/helpchm.h>
#else
#include <wx/html/helpctrl.h>
#endif
#endif // !NO_GUI

#include <wx/utils.h>
#include <wx/wxcrtvararg.h>

#include <ctype.h>
#include "tex2any.h"
#include "tex2rtf.h"
#include "rtfutils.h"
#include "symbols.h"

#if (defined(__WXGTK__) || defined(__WXMOTIF__) || defined(__WXMAC__) || defined(__WXX11__) || defined(__WXMGL__)) && !defined(NO_GUI)
#include "tex2rtf.xpm"
#endif

#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace std;

TexChunk* pCurrentMember = NULL;
bool startedSections = false;
wxChar* contentsString = NULL;
bool suppressNameDecoration = false;
bool OkToClose = true;
int passNumber = 1;
unsigned long errorCount = 0;

extern wxChar* BigBuffer;
extern TexChunk* TopLevel;

#ifndef NO_GUI

extern ColourTableMap ColourTable;

#if wxUSE_HELP
wxHelpControllerBase* pHelpInstance = NULL;
#endif // wxUSE_HELP

#ifdef __WXMSW__
static wxChar* pIpcBuffer = NULL;
static wxChar Tex2RTFLastStatus[100];
Tex2RTFServer *TheTex2RTFServer = NULL;
#endif // __WXMSW__

#endif // !NO_GUI

wxString bulletFile;

FILE* Contents = NULL;   // Contents page
FILE* Chapters = NULL;   // Chapters (WinHelp RTF) or rest of file (linear RTF)
FILE* Sections = NULL;
FILE* Subsections = NULL;
FILE* Subsubsections = NULL;
FILE* Popups = NULL;
FILE* WinHelpContentsFile = NULL;

wxString InputFile;
wxString OutputFile;
wxString MacroFile = "tex2rtf.ini";

wxString FileRoot;
wxString ContentsName;            // Contents page from last time around.
wxString TmpContentsName;         // Current contents page file name.
wxString TmpFrameContentsName;    // Current frame contents page.
wxString WinHelpContentsFileName; // WinHelp .cnt file name.
wxString RefFileName;             // Reference file name.

wxString RTFCharset = "ansi";

#ifdef __WXMSW__
int BufSize = 100;             // Size of buffer in K
#else
int BufSize = 500;
#endif

bool Go(void);
void ShowOptions(void);
void ShowVersion(void);
void CleanupHtmlProcessing();

wxChar wxTex2RTFBuffer[1500];

#ifdef NO_GUI
IMPLEMENT_APP_CONSOLE(Tex2RtfApplication)
#else
wxMenuBar* pMenuBar = NULL;
MyFrame* pFrame = NULL;
// DECLARE_APP(Tex2RtfApplication)
IMPLEMENT_APP(Tex2RtfApplication)
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Tex2RtfApplication::Tex2RtfApplication()
#ifdef NO_GUI
  : wxAppConsole()
#else
  : wxApp()
#endif
{
#ifdef _MSC_VER
  // When using the Microsoft C++ compiler in debug mode, each heap allocation
  // (i.e. calling new) is counted. The following line will cause the code to
  // generate a user defined break point when the passed allocation index is
  // hit.  To find leaks, run in debug and look for the following type of line
  // in the Debug output window.
  //
  // {5899} normal block at 0x00EDC998, 200000 bytes long.
  // Data: <    i s   p a n > 0A 00 00 00 69 00 73 00 20 00 70 00 61 00 6E 00
  //
  // Then use the following line to cause a break point when this allocation
  // occurs:
  //
  // _CrtSetBreakAlloc(5899);
#endif // _MSC_VER
}

//-----------------------------------------------------------------------------
// Description:
//   `Main program' equivalent, creats windows and the application main frame.
//-----------------------------------------------------------------------------
bool Tex2RtfApplication::OnInit()
{
  // Use default list of macros defined in tex2any.cpp.
  DefineDefaultMacros();
  AddMacroDef(ltHARDY, _T("hardy"), 0);

  for (
    ColourTableMap::iterator it = ColourTable.begin();
    it != ColourTable.end();
    ++it)
  {
    ColourTableEntry* entry = it->second;
    delete entry;
  }
  ColourTable.clear();

  int n = 1;

  // Read input/output files
  if (argc > 1)
  {
    if (argv[1][0] != _T('-'))
    {
      InputFile = argv[1];
      n ++;

      if (argc > 2)
      {
        if (argv[2][0] != _T('-'))
        {
          OutputFile = argv[2];
          ++n;
        }
      }
    }
  }

  TexPathList.Add(::wxGetCwd());

  int i;
  for (i = n; i < argc;)
  {
    if (wxStrcmp(argv[i], _T("-winhelp")) == 0)
    {
      ++i;
      convertMode = TEX_RTF;
      winHelp = true;
    }
#ifndef NO_GUI
    else if (wxStrcmp(argv[i], _T("-interactive")) == 0)
    {
      ++i;
      isInteractive = true;
    }
#endif
    else if (wxStrcmp(argv[i], _T("-sync")) == 0)  // Don't yield
    {
      ++i;
      isSync = true;
    }
    else if (wxStrcmp(argv[i], _T("-rtf")) == 0)
    {
      ++i;
      convertMode = TEX_RTF;
    }
    else if (wxStrcmp(argv[i], _T("-html")) == 0)
    {
      ++i;
      convertMode = TEX_HTML;
    }
    else if (wxStrcmp(argv[i], _T("-twice")) == 0)
    {
      ++i;
      runTwice = true;
    }
    else if (wxStrcmp(argv[i], _T("-macros")) == 0)
    {
      ++i;
      if (i < argc)
      {
        MacroFile = argv[i];
        ++i;
      }
    }
    else if (wxStrcmp(argv[i], _T("-bufsize")) == 0)
    {
      ++i;
      if (i < argc)
      {
        BufSize = wxAtoi(argv[i]);
        ++i;
      }
    }
    else if (wxStrcmp(argv[i], _T("-charset")) == 0)
    {
      ++i;
      if (i < argc)
      {
        wxString s = argv[i];
        ++i;
        if (s == "ansi" || s == "pc" || s == "mac" || s == "pca")
        {
          RTFCharset = s;
        }
        else
        {
          OnError(_T("Incorrect argument for -charset"));
          return false;
        }
      }
    }
    else if (wxStrcmp(argv[i], _T("-checkcurlybraces")) == 0)
    {
      ++i;
      checkCurlyBraces = true;
    }
    else if (wxStrcmp(argv[i], _T("-checkcurleybraces")) == 0)
    {
      // Support the old, incorrectly spelled version of -checkcurlybraces
      // so that old scripts which run tex2rtf -checkcurleybraces still work.
      ++i;
      checkCurlyBraces = true;
    }
    else if (wxStrcmp(argv[i], _T("-checksyntax")) == 0)
    {
      ++i;
      checkSyntax = true;
    }
    else if (wxStrcmp(argv[i], _T("-version")) == 0)
    {
      ++i;
      ShowVersion();
#ifdef NO_GUI
      exit(1);
#else
      return false;
#endif
    }
    else
    {
      wxString Message;
      Message << "Invalid switch " << argv[i] << '\n';
      OnError(Message);
#ifdef NO_GUI
      ShowOptions();
      exit(1);
#else
      return false;
#endif
    }
  }

#ifdef NO_GUI
  if (InputFile.empty() || OutputFile.empty())
  {
    wxString Message;
    Message << "Tex2RTF: input or output file is missing.";
    OnError(Message);
    ShowOptions();
    exit(1);
  }
#endif

  if (!InputFile.empty())
  {
      TexPathList.EnsureFileAccessible(InputFile);
  }
  if (InputFile.empty() || OutputFile.empty())
      isInteractive = true;

#if defined(__WXMSW__) && !defined(NO_GUI)
  wxDDEInitialize();
  Tex2RTFLastStatus[0] = 0; // DDE connection return value
  TheTex2RTFServer = new Tex2RTFServer;
  TheTex2RTFServer->Create(_T("TEX2RTF"));
#endif

  TexInitialize(BufSize);
  ResetContentsLevels(0);

#ifndef NO_GUI

  if (isInteractive)
  {
    // Create the main frame window
    pFrame = new MyFrame(
      NULL,
      wxID_ANY,
      _T("Tex2RTF"),
      wxDefaultPosition,
      wxSize(400, 300));

    pFrame->CreateStatusBar(2);

    // Give it an icon
    pFrame->SetIcon(wxICON(tex2rtf));

    if (!InputFile.empty())
    {
      wxString title;
      title.Printf(_T("Tex2RTF [%s]"), wxFileNameFromPath(InputFile).c_str());
      pFrame->SetTitle(title);
    }

    // Make a menubar
    wxMenu *file_menu = new wxMenu;
    file_menu->Append(TEX_GO, _T("&Go"), _T("Run converter"));
    file_menu->Append(TEX_SET_INPUT, _T("Set &Input File"), _T("Set the LaTeX input file"));
    file_menu->Append(TEX_SET_OUTPUT, _T("Set &Output File"), _T("Set the output file"));
    file_menu->AppendSeparator();
    file_menu->Append(TEX_VIEW_LATEX, _T("View &LaTeX File"), _T("View the LaTeX input file"));
    file_menu->Append(TEX_VIEW_OUTPUT, _T("View Output &File"), _T("View output file"));
    file_menu->Append(TEX_SAVE_FILE, _T("&Save log file"), _T("Save displayed text into file"));
    file_menu->AppendSeparator();
    file_menu->Append(TEX_QUIT, _T("E&xit"), _T("Exit Tex2RTF"));

    wxMenu *macro_menu = new wxMenu;

    macro_menu->Append(TEX_LOAD_CUSTOM_MACROS, _T("&Load Custom Macros"), _T("Load custom LaTeX macro file"));
    macro_menu->Append(TEX_VIEW_CUSTOM_MACROS, _T("View &Custom Macros"), _T("View custom LaTeX macros"));

    wxMenu *mode_menu = new wxMenu;

    mode_menu->Append(TEX_MODE_RTF, _T("Output linear &RTF"), _T("Wordprocessor-compatible RTF"));
    mode_menu->Append(TEX_MODE_WINHELP, _T("Output &WinHelp RTF"), _T("WinHelp-compatible RTF"));
    mode_menu->Append(TEX_MODE_HTML, _T("Output &HTML"), _T("HTML World Wide Web hypertext file"));

    wxMenu *options_menu = new wxMenu;

    options_menu->Append(TEX_OPTIONS_CURLY_BRACE, _T("Curly brace matching"), _T("Checks for mismatched curly braces"),true);
    options_menu->Append(TEX_OPTIONS_SYNTAX_CHECKING, _T("Syntax checking"), _T("Syntax checking for common errors"),true);

    options_menu->Check(TEX_OPTIONS_CURLY_BRACE, checkCurlyBraces);
    options_menu->Check(TEX_OPTIONS_SYNTAX_CHECKING, checkSyntax);

    wxMenu *help_menu = new wxMenu;

    help_menu->Append(TEX_HELP, _T("&Help"), _T("Tex2RTF Contents Page"));
    help_menu->Append(TEX_ABOUT, _T("&About Tex2RTF"), _T("About Tex2RTF"));

    pMenuBar = new wxMenuBar;
    pMenuBar->Append(file_menu, _T("&File"));
    pMenuBar->Append(macro_menu, _T("&Macros"));
    pMenuBar->Append(mode_menu, _T("&Conversion Mode"));
    pMenuBar->Append(options_menu, _T("&Options"));
    pMenuBar->Append(help_menu, _T("&Help"));

    pFrame->SetMenuBar(pMenuBar);
    pFrame->textWindow = new wxTextCtrl(
      pFrame,
      wxID_ANY,
      wxEmptyString,
      wxDefaultPosition,
      wxDefaultSize,
      wxTE_READONLY | wxTE_MULTILINE);

    (*pFrame->textWindow) << _T("Welcome to Tex2RTF.\n");
//    ShowOptions();

#if wxUSE_HELP
#if wxUSE_MS_HTML_HELP && !defined(__WXUNIVERSAL__)
    pHelpInstance = new wxCHMHelpController;
#else
    pHelpInstance = new wxHelpController;
#endif
    pHelpInstance->Initialize(_T("tex2rtf"));
#endif // wxUSE_HELP

    // Read macro/initialisation file

    wxString path = TexPathList.FindValidPath(MacroFile);
    if (!path.empty())
    {
      ReadCustomMacros(path);
    }

    wxString inStr(_T("In "));
    switch (convertMode)
    {
      case TEX_RTF:
        if(winHelp)
          inStr += _T("WinHelp RTF");
        else
          inStr += _T("linear RTF");
        break;

      case TEX_HTML:
        inStr += _T("HTML");
        break;

      default:
        inStr += _T("unknown");
        break;
    }
    inStr += _T(" mode.");
    pFrame->SetStatusText(inStr, 1);

    pFrame->Show(true);
    return true;
  }
  else
#endif // NO_GUI
  {
    // Read macro/initialization file.
    wxString path = TexPathList.FindValidPath(MacroFile);
    if (!path.empty())
    {
      ReadCustomMacros(path);
    }

    bool rc = Go();
    if (rc && runTwice)
    {
      rc = Go();
    }

#ifdef NO_GUI
    ClearKeyWordTable();
    CleanUp();
    return rc;
#else
    OnExit(); // Do cleanup since OnExit won't be called now
    return false;
#endif
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Tex2RtfApplication::CleanUp()
{
  for (
    ColourTableMap::iterator it = ColourTable.begin();
    it != ColourTable.end();
    ++it)
  {
    ColourTableEntry *entry = it->second;
    delete entry;
  }
  ColourTable.clear();

  for (
    MacroMap::iterator it = CustomMacroMap.begin();
    it != CustomMacroMap.end();
    ++it)
  {
    CustomMacro *macro = it->second;
    delete macro;
  }
  CustomMacroMap.clear();
  MacroDefs.BeginFind();
  wxHashTable::Node* mNode = MacroDefs.Next();
  while (mNode)
  {
    TexMacroDef* def = (TexMacroDef*) mNode->GetData();
    delete def;
    mNode = MacroDefs.Next();
  }
  MacroDefs.Clear();

  if (BigBuffer)
  {
    delete [] BigBuffer;
    BigBuffer = NULL;
  }
  if (TopLevel)
  {
    delete TopLevel;
    TopLevel = NULL;
  }

  CleanupHtmlProcessing();
}

#ifndef NO_GUI
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int Tex2RtfApplication::OnExit()
{
  for (
    ColourTableMap::iterator it = ColourTable.begin();
    it != ColourTable.end();
    ++it)
  {
    ColourTableEntry *entry = it->second;
    delete entry;
  }
  ColourTable.clear();

  for (
    MacroMap::iterator it = CustomMacroMap.begin();
    it != CustomMacroMap.end();
    ++it)
  {
    CustomMacro *macro = it->second;
    delete macro;
  }
  CustomMacroMap.clear();
  MacroDefs.BeginFind();
  wxHashTable::Node* mNode = MacroDefs.Next();
  while (mNode)
  {
    TexMacroDef* def = (TexMacroDef*) mNode->GetData();
    delete def;
    mNode = MacroDefs.Next();
  }
  MacroDefs.Clear();
#ifdef __WXMSW__
  delete TheTex2RTFServer;
  wxDDECleanUp();
#endif

#if wxUSE_HELP
  delete pHelpInstance;
#endif // wxUSE_HELP

  // TODO: this simulates zero-memory leaks!
  // Otherwise there are just too many...
#ifndef __WXGTK__
#if (defined(__WXDEBUG__) && wxUSE_MEMORY_TRACING) || wxUSE_DEBUG_CONTEXT
  wxDebugContext::SetCheckpoint();
#endif
#endif
    if (BigBuffer)
    {
      delete [] BigBuffer;
      BigBuffer = NULL;
    }
    if (TopLevel)
    {
      delete TopLevel;
      TopLevel = NULL;
    }

  return 0;
}
#endif

void ShowVersion(void)
{
  wxString Version;
  Version << "Tex2RTF version " << TEX2RTF_VERSION_NUMBER_STRING;
  OnInform(Version);
}

void ShowOptions(void)
{
  ShowVersion();
  OnInform(_T("Usage: tex2rtf [input] [output] [switches]\n"));
  OnInform(_T("where valid switches are"));
#ifndef NO_GUI
  OnInform(_T("    -interactive"));
#endif
  OnInform(_T("    -bufsize <size in K>"));
  OnInform(_T("    -charset <pc | pca | ansi | mac> (default ansi)"));
  OnInform(_T("    -twice"));
  OnInform(_T("    -sync"));
  OnInform(_T("    -checkcurlybraces"));
  OnInform(_T("    -checksyntax"));
  OnInform(_T("    -version"));
  OnInform(_T("    -macros <filename>"));
  OnInform(_T("    -winhelp"));
  OnInform(_T("    -rtf (default)"));
  OnInform(_T("    -html"));
}

#ifndef NO_GUI

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
  EVT_CLOSE(MyFrame::OnCloseWindow)
  EVT_MENU(TEX_QUIT, MyFrame::OnExit)
  EVT_MENU(TEX_GO, MyFrame::OnGo)
  EVT_MENU(TEX_SET_INPUT, MyFrame::OnSetInput)
  EVT_MENU(TEX_SET_OUTPUT, MyFrame::OnSetOutput)
  EVT_MENU(TEX_SAVE_FILE, MyFrame::OnSaveFile)
  EVT_MENU(TEX_VIEW_LATEX, MyFrame::OnViewLatex)
  EVT_MENU(TEX_VIEW_OUTPUT, MyFrame::OnViewOutput)
  EVT_MENU(TEX_VIEW_CUSTOM_MACROS, MyFrame::OnShowMacros)
  EVT_MENU(TEX_LOAD_CUSTOM_MACROS, MyFrame::OnLoadMacros)
  EVT_MENU(TEX_MODE_RTF, MyFrame::OnModeRTF)
  EVT_MENU(TEX_MODE_WINHELP, MyFrame::OnModeWinHelp)
  EVT_MENU(TEX_MODE_HTML, MyFrame::OnModeHTML)
  EVT_MENU(TEX_OPTIONS_CURLY_BRACE, MyFrame::OnOptionsCurlyBrace)
  EVT_MENU(TEX_OPTIONS_SYNTAX_CHECKING, MyFrame::OnOptionsSyntaxChecking)
  EVT_MENU(TEX_HELP, MyFrame::OnHelp)
  EVT_MENU(TEX_ABOUT, MyFrame::OnAbout)
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
// My frame constructor
//-----------------------------------------------------------------------------
MyFrame::MyFrame(
  wxFrame* frame,
  wxWindowID id,
  const wxString& title,
  const wxPoint& pos,
  const wxSize& size)
  : wxFrame(frame, id, title, pos, size)
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
  if (!stopRunning && !OkToClose)
  {
    stopRunning = true;
    runTwice = false;
    return;
  }
  else if (OkToClose)
  {
    this->Destroy();
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnExit(wxCommandEvent& WXUNUSED(event))
{
  Close();
//    this->Destroy();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnGo(wxCommandEvent& WXUNUSED(event))
{
  passNumber = 1;
  errorCount = 0;
  pMenuBar->EnableTop(0, false);
  pMenuBar->EnableTop(1, false);
  pMenuBar->EnableTop(2, false);
  pMenuBar->EnableTop(3, false);
  textWindow->Clear();
  Tex2RTFYield(true);
  Go();

  if (stopRunning)
  {
    SetStatusText(_T("Build aborted!"));
    wxString errBuf;
    errBuf.Printf(_T("\nErrors encountered during this pass: %lu\n"), errorCount);
    OnInform(errBuf);
  }


  if (runTwice && !stopRunning)
  {
    Tex2RTFYield(true);
    Go();
  }
  pMenuBar->EnableTop(0, true);
  pMenuBar->EnableTop(1, true);
  pMenuBar->EnableTop(2, true);
  pMenuBar->EnableTop(3, true);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnSetInput(wxCommandEvent& WXUNUSED(event))
{
  ChooseInputFile(true);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnSetOutput(wxCommandEvent& WXUNUSED(event))
{
  ChooseOutputFile(true);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnSaveFile(wxCommandEvent& WXUNUSED(event))
{
#if wxUSE_FILEDLG
  wxString s = wxFileSelector(
    _T("Save text to file"),
    wxEmptyString,
    wxEmptyString,
    _T("txt"),
    _T("*.txt"));
  if (!s.empty())
  {
    textWindow->SaveFile(s);
    const size_t bufSize = 350;
    wxChar buf[bufSize];
    wxSnprintf(buf, bufSize, _T("Saved text to %s"), (const wxChar*) s.c_str());
    pFrame->SetStatusText(buf, 0);
  }
#endif // wxUSE_FILEDLG
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnViewOutput(wxCommandEvent& WXUNUSED(event))
{
  ChooseOutputFile();
  if (!OutputFile.empty() && wxFileExists(OutputFile))
  {
    textWindow->LoadFile(OutputFile);
    const size_t bufSize = 300;
    wxChar buf[bufSize];
    wxString str(wxFileNameFromPath(OutputFile));
    wxSnprintf(buf, bufSize, _T("Tex2RTF [%s]"), (const wxChar*) str.c_str());
    pFrame->SetTitle(buf);
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnViewLatex(wxCommandEvent& WXUNUSED(event))
{
  ChooseInputFile();
  if (!InputFile.empty() && wxFileExists(InputFile))
  {
    textWindow->LoadFile(InputFile);
    const size_t bufSize = 300;
    wxChar buf[bufSize];
    wxString str(wxFileNameFromPath(OutputFile));
    wxSnprintf(buf, bufSize, _T("Tex2RTF [%s]"), (const wxChar*) str.c_str());
    pFrame->SetTitle(buf);
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnLoadMacros(wxCommandEvent& WXUNUSED(event))
{
    textWindow->Clear();
#if wxUSE_FILEDLG
    wxString s = wxFileSelector(
      "Choose custom macro file",
      wxPathOnly(MacroFile),
      wxFileNameFromPath(MacroFile),
      "ini",
      "*.ini");
    if (!s.empty() && wxFileExists(s))
    {
        MacroFile = s;
        ReadCustomMacros(s);
        ShowCustomMacros();
    }
#endif // wxUSE_FILEDLG
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnShowMacros(wxCommandEvent& WXUNUSED(event))
{
    textWindow->Clear();
    Tex2RTFYield(true);
    ShowCustomMacros();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnModeRTF(wxCommandEvent& WXUNUSED(event))
{
    convertMode = TEX_RTF;
    winHelp = false;
    InputFile = wxEmptyString;
    OutputFile = wxEmptyString;
    SetStatusText(_T("In linear RTF mode."), 1);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnModeWinHelp(wxCommandEvent& WXUNUSED(event))
{
    convertMode = TEX_RTF;
    winHelp = true;
    InputFile = wxEmptyString;
    OutputFile = wxEmptyString;
    SetStatusText(_T("In WinHelp RTF mode."), 1);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnModeHTML(wxCommandEvent& WXUNUSED(event))
{
  convertMode = TEX_HTML;
  winHelp = false;
  InputFile = wxEmptyString;
  OutputFile = wxEmptyString;
  SetStatusText(_T("In HTML mode."), 1);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnOptionsCurlyBrace(wxCommandEvent& WXUNUSED(event))
{
    checkCurlyBraces = !checkCurlyBraces;
    if (checkCurlyBraces)
    {
        SetStatusText(_T("Checking curly braces: YES"), 1);
    }
    else
    {
        SetStatusText(_T("Checking curly braces: NO"), 1);
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnOptionsSyntaxChecking(wxCommandEvent& WXUNUSED(event))
{
  checkSyntax = !checkSyntax;
  if (checkSyntax)
  {
    SetStatusText(_T("Checking syntax: YES"), 1);
  }
  else
  {
    SetStatusText(_T("Checking syntax: NO"), 1);
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnHelp(wxCommandEvent& WXUNUSED(event))
{
#if wxUSE_HELP
  pHelpInstance->LoadFile();
  pHelpInstance->DisplayContents();
#endif // wxUSE_HELP
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
  wxString Platform = wxGetOsDescription();
  wxString Message;
  Message
    << "Tex2RTF Version " << TEX2RTF_VERSION_NUMBER_STRING
    << ' ' << Platform << '\n'
    << "LaTeX to RTF, WinHelp, and HTML Conversion" << "\n\n"
    << "(c) Pete Stieber, Julian Smart, George Tasker and others, 1999-2014";
  wxMessageBox(Message, _T("About Tex2RTF"));
}

void ChooseInputFile(bool force)
{
#if wxUSE_FILEDLG
    if (force || InputFile.empty())
    {
        wxString s = wxFileSelector(_T("Choose LaTeX input file"), wxPathOnly(InputFile), wxFileNameFromPath(InputFile), _T("tex"), _T("*.tex"));
        if (!s.empty())
        {
            // Different file, so clear index entries.
            ClearKeyWordTable();
            ResetContentsLevels(0);
            passNumber = 1;
            errorCount = 0;

            InputFile = s;
            wxString str = wxFileNameFromPath(InputFile);
            wxString buf;
            buf <<  "Tex2RTF [" << wxFileNameFromPath(InputFile) << "]";
            pFrame->SetTitle(buf);
            OutputFile = wxEmptyString;
        }
    }
#else
    wxUnusedVar(force);
#endif // wxUSE_FILEDLG
}

void ChooseOutputFile(bool force)
{
    wxChar extensionBuf[10];
    wxChar wildBuf[10];
    wxStrcpy(wildBuf, _T("*."));
    wxString path;
    if (!OutputFile.empty())
        path = wxPathOnly(OutputFile);
    else if (!InputFile.empty())
        path = wxPathOnly(InputFile);

    switch (convertMode)
    {
        case TEX_RTF:
        {
            wxStrcpy(extensionBuf, _T("rtf"));
            wxStrcat(wildBuf, _T("rtf"));
            break;
        }
        case TEX_HTML:
        {
            wxStrcpy(extensionBuf, _T("html"));
            wxStrcat(wildBuf, _T("html"));
            break;
        }
    }
#if wxUSE_FILEDLG
    if (force || OutputFile.empty())
    {
        wxString s = wxFileSelector(_T("Choose output file"), path, wxFileNameFromPath(OutputFile),
                                    extensionBuf, wildBuf);
        if (!s.empty())
            OutputFile = s;
    }
#else
    wxUnusedVar(force);
#endif // wxUSE_FILEDLG
}
#endif

bool Go(void)
{
#ifndef NO_GUI
  ChooseInputFile();
  ChooseOutputFile();
#endif

  if (InputFile.empty() || OutputFile.empty() || stopRunning)
    return false;

#ifndef NO_GUI
  if (isInteractive)
  {
    wxString buf;
    buf << "Tex2RTF [" << wxFileNameFromPath(InputFile) << "]";
    pFrame->SetTitle(buf);
  }

  wxLongLong localTime = wxGetLocalTimeMillis();
#endif

  // Find extension-less filename
  FileRoot = OutputFile;
  StripExtension(FileRoot);

  ContentsName = FileRoot;
  ContentsName.append(".con");

  TmpContentsName = FileRoot;
  TmpContentsName.append(".cn1");

  TmpFrameContentsName = FileRoot;
  TmpFrameContentsName.append(".frc");

  WinHelpContentsFileName = FileRoot;
  WinHelpContentsFileName.append(".cnt");

  RefFileName = FileRoot;
  RefFileName.append(".ref");

  TexPathList.EnsureFileAccessible(InputFile);
  if (!bulletFile.empty())
  {
    wxString s = TexPathList.FindValidPath(_T("bullet.bmp"));
    if (!s.empty())
    {
      bulletFile = wxFileNameFromPath(s);
    }
  }

  if (wxFileExists(RefFileName))
  {
    ReadTexReferences(RefFileName);
  }

  bool success = false;

  if (!InputFile.empty() && !OutputFile.empty())
  {
    if (!wxFileExists(InputFile))
    {
      OnError(_T("Cannot open input file!"));
      TexCleanUp();
      return false;
    }
#ifndef NO_GUI
    if (isInteractive)
    {
      wxString buf;
      buf.Printf(_T("Working, pass %d...Click CLOSE to abort"), passNumber);
      pFrame->SetStatusText(buf);
    }
#endif
    OkToClose = false;
    OnInform("Reading LaTeX file...");
    TexLoadFile(InputFile);

    if (stopRunning)
    {
      OkToClose = true;
      return false;
    }

    switch (convertMode)
    {
      case TEX_RTF:
      {
        success = RTFGo();
        break;
      }
      case TEX_HTML:
      {
        success = HTMLGo();
        break;
      }
    }
  }
  if (stopRunning)
  {
    OnInform("*** Aborted by user.");
    success = false;
    stopRunning = false;
    OkToClose = true;
  }

  if (success)
  {
    WriteTexReferences(RefFileName);
    TexCleanUp();
    startedSections = false;

    wxString buf;
#ifndef NO_GUI
    wxLongLong elapsed = wxGetLocalTimeMillis() - localTime;
    buf.Printf(_T("Finished PASS #%d in %ld seconds.\n"), passNumber, (long)(elapsed.GetLo()/1000.0));
    OnInform(buf);

    if (errorCount)
    {
        buf.Printf(_T("Errors encountered during this pass: %lu\n"), errorCount);
        OnInform(buf);
    }

    if (isInteractive)
    {
      buf.Printf(_T("Done, %d %s."), passNumber, (passNumber > 1) ? _T("passes") : _T("pass"));
      pFrame->SetStatusText(buf);
    }
#else
    buf << "Done, " << passNumber << " pass";
    if (passNumber > 1)
    {
      buf << "es";
    }

    OnInform(buf);
    if (errorCount)
    {
      buf.clear();
      buf << "Errors encountered during this pass: " << errorCount << '\n';
      OnInform(buf);
    }
#endif
    passNumber ++;
    errorCount = 0;
    OkToClose = true;
    return true;
  }

  TexCleanUp();
  startedSections = false;

#ifndef NO_GUI
  pFrame->SetStatusText(_T("Aborted by user."));
#endif // GUI

  OnInform("Sorry, unsuccessful.");
  OkToClose = true;
  return false;
}

void OnError(const wxString& ErrorMessage)
{
  ++errorCount;

#ifdef NO_GUI
  cerr << "Error: " << ErrorMessage << '\n';
  cerr.flush();
#else
  if (isInteractive && pFrame)
  {
    (*pFrame->textWindow) << _T("Error: ") << ErrorMessage << '\n';
  }
  else
  {
#if defined(__UNIX__)
    cerr << "Error: " << ErrorMessage << '\n';
    cerr.flush();
#elif defined(__WXMSW__)
    wxLogError(ErrorMessage);
#endif
  }

  Tex2RTFYield(true);
#endif // NO_GUI
}

void OnInform(const wxString& Message)
{
#ifdef NO_GUI
  cout << Message << '\n';
  cout.flush();
#else
  if (isInteractive && pFrame)
  {
    (*pFrame->textWindow) << Message << '\n';
  }
  else
  {
#if defined(__UNIX__)
    cout << Message << '\n';
    cout.flush();
#elif defined(__WXMSW__)
    wxLogInfo(Message);
#endif
  }

  if (isInteractive)
  {
    Tex2RTFYield(true);
  }
#endif // NO_GUI
}

void OnMacro(int macroId, int no_args, bool start)
{
  switch (convertMode)
  {
    case TEX_RTF:
    {
      RTFOnMacro(macroId, no_args, start);
      break;
    }
    case TEX_HTML:
    {
      HTMLOnMacro(macroId, no_args, start);
      break;
    }
  }
}

bool OnArgument(int macroId, int arg_no, bool start)
{
  switch (convertMode)
  {
    case TEX_RTF:
    {
      return RTFOnArgument(macroId, arg_no, start);
      // break;
    }
    case TEX_HTML:
    {
      return HTMLOnArgument(macroId, arg_no, start);
      // break;
    }
  }
  return true;
}

//*****************************************************************************
// DDE Stuff
//*****************************************************************************
#if defined(__WXMSW__) && !defined(NO_GUI)

//*****************************************************************************
// Server
//*****************************************************************************

wxConnectionBase *Tex2RTFServer::OnAcceptConnection(const wxString& topic)
{
  if (topic == _T("TEX2RTF"))
  {
    if (!pIpcBuffer)
    {
      pIpcBuffer = new wxChar[1000];
    }

    return new Tex2RTFConnection(pIpcBuffer, 4000);
  }
  return NULL;
}

//*****************************************************************************
// Connection
//*****************************************************************************
Tex2RTFConnection::Tex2RTFConnection(wxChar* buf, int size)
  : wxDDEConnection(buf, size)
{
}

bool SplitCommand(wxChar *data, wxChar *firstArg, wxChar *secondArg)
{
  firstArg[0] = 0;
  secondArg[0] = 0;
  int i = 0;
  bool stop = false;
  // Find first argument (command name)
  while (!stop)
  {
    if (data[i] == ' ' || data[i] == 0)
      stop = true;
    else
    {
      firstArg[i] = data[i];
      ++i;
    }
  }
  firstArg[i] = 0;
  if (data[i] == ' ')
  {
    // Find second argument
    ++i;
    int j = 0;
    while (data[i] != 0)
    {
      secondArg[j] = data[i];
      ++i;
      ++j;
    }
    secondArg[j] = 0;
  }
  return true;
}

bool Tex2RTFConnection::OnExecute(
  const wxString& WXUNUSED(topic),
  wxChar* data,
  int WXUNUSED(size),
  wxIPCFormat WXUNUSED(format))
{
  wxStrcpy(Tex2RTFLastStatus, _T("OK"));

  wxChar firstArg[50];
  wxChar secondArg[300];
  if (SplitCommand(data, firstArg, secondArg))
  {
    bool hasArg = (wxStrlen(secondArg) > 0);
    if (wxStrcmp(firstArg, _T("INPUT")) == 0 && hasArg)
    {
      InputFile = secondArg;
      if (pFrame)
      {
        wxString buf;
        wxString str = wxFileNameFromPath(InputFile);
        buf << "Tex2RTF [" << str << "]";
        pFrame->SetTitle(buf);
      }
    }
    else if (wxStrcmp(firstArg, _T("OUTPUT")) == 0 && hasArg)
    {
      OutputFile = secondArg;
    }
    else if (wxStrcmp(firstArg, _T("GO")) == 0)
    {
      wxStrcpy(Tex2RTFLastStatus, _T("WORKING"));
      if (!Go())
        wxStrcpy(Tex2RTFLastStatus, _T("CONVERSION ERROR"));
      else
        wxStrcpy(Tex2RTFLastStatus, _T("OK"));
    }
    else if (wxStrcmp(firstArg, _T("EXIT")) == 0)
    {
      if (pFrame)
      {
        pFrame->Close();
      }
    }
    else if (wxStrcmp(firstArg, _T("MINIMIZE")) == 0 || wxStrcmp(firstArg, _T("ICONIZE")) == 0)
    {
      if (pFrame)
        pFrame->Iconize(true);
    }
    else if (wxStrcmp(firstArg, _T("SHOW")) == 0 || wxStrcmp(firstArg, _T("RESTORE")) == 0)
    {
      if (pFrame)
      {
        pFrame->Iconize(false);
        pFrame->Show(true);
      }
    }
    else
    {
      // Try for a setting
      wxStrcpy(Tex2RTFLastStatus, RegisterSetting(firstArg, secondArg, false));
#ifndef NO_GUI
      if (pFrame && wxStrcmp(firstArg, _T("conversionMode")) == 0)
      {
        wxChar buf[100];
        wxStrcpy(buf, _T("In "));

        if (winHelp && (convertMode == TEX_RTF))
        {
          wxStrcat(buf, _T("WinHelp RTF"));
        }
        else if (!winHelp && (convertMode == TEX_RTF))
        {
          wxStrcat(buf, _T("linear RTF"));
        }
        else if (convertMode == TEX_HTML)
        {
          wxStrcat(buf, _T("HTML"));
        }
        wxStrcat(buf, _T(" mode."));
        pFrame->SetStatusText(buf, 1);
      }
#endif
    }
  }
  return true;
}

wxChar *Tex2RTFConnection::OnRequest(
  const wxString& WXUNUSED(topic),
  const wxString& WXUNUSED(item),
  int *WXUNUSED(size),
  wxIPCFormat WXUNUSED(format))
{
  return Tex2RTFLastStatus;
}

#endif
