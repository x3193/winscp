//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <Common.h>

#include "Authenticate.h"

#include <VCLCommon.h>
#include <TextsWin.h>
#include <Terminal.h>
#include <CoreMain.h>
#include <PasTools.hpp>
#include <CustomWinConfiguration.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "PasswordEdit"
#ifndef NO_RESOURCES
#pragma resource "*.dfm"
#endif
//---------------------------------------------------------------------------
__fastcall TAuthenticateForm::TAuthenticateForm(TComponent * Owner)
  : TForm(Owner), FSessionData(NULL), FTerminal(NULL)
{
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::Init(TTerminal * Terminal)
{
  FTerminal = Terminal;
  FSessionData = Terminal->SessionData;

  UseSystemSettings(this);
  FShowAsModalStorage = NULL;
  FFocusControl = NULL;
  UseDesktopFont(LogView);
  FAnimationPainted = false;

  FPromptParent = InstructionsLabel->Parent;
  FPromptLeft = InstructionsLabel->Left;
  FPromptTop = InstructionsLabel->Top;
  FPromptRight = FPromptParent->ClientWidth - InstructionsLabel->Width - FPromptLeft;
  FPromptEditGap = PromptEdit1->Top - PromptLabel1->Top - PromptLabel1->Height;
  FPromptsGap = PromptLabel2->Top - PromptEdit1->Top - PromptEdit1->Height;

  ClientHeight = ScaleByTextHeight(this, 270);
  FHorizontalLogPadding = ScaleByTextHeight(this, 4);
  FVerticalLogPadding = ScaleByTextHeight(this, 3);
  FLogTextFormat << tfNoPrefix << tfWordBreak << tfVerticalCenter;

  ClearLog();
}
//---------------------------------------------------------------------------
__fastcall TAuthenticateForm::~TAuthenticateForm()
{
  ReleaseAsModal(this, FShowAsModalStorage);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::ShowAsModal()
{
  ::ShowAsModal(this, FShowAsModalStorage);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::HideAsModal()
{
  ::HideAsModal(this, FShowAsModalStorage);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::WMNCCreate(TWMNCCreate & Message)
{
  // bypass TForm::WMNCCreate to prevent disabling "resize"
  // (which is done for bsDialog, see comments in CreateParams)
  DefaultHandler(&Message);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::DoCancel()
{
  if (FOnCancel != NULL)
  {
    FOnCancel(this);
  }
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::Dispatch(void * AMessage)
{
  TMessage & Message = *reinterpret_cast<TMessage *>(AMessage);
  if (Message.Msg == WM_NCCREATE)
  {
    WMNCCreate(*reinterpret_cast<TWMNCCreate *>(AMessage));
  }
  else if (Message.Msg == WM_CLOSE)
  {
    DoCancel();
    TForm::Dispatch(AMessage);
  }
  else if (Message.Msg == WM_MANAGES_CAPTION)
  {
    // caption managed in TAuthenticateForm::AdjustControls()
    Message.Result = 1;
  }
  else
  {
    TForm::Dispatch(AMessage);
  }
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::CreateParams(TCreateParams & Params)
{
  TForm::CreateParams(Params);

  // Allow resizing of the window, even if this is bsDialog.
  // This makes it more close to bsSizeable, but bsSizeable cannot for some
  // reason receive focus, if window is shown atop non-main window
  // (like editor)
  Params.Style = Params.Style | WS_THICKFRAME;
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::FormShow(TObject * /*Sender*/)
{
  AdjustControls();

  if (FFocusControl != NULL)
  {
    ActiveControl = FFocusControl;
  }

  UnicodeString AnimationName = FSessionData->IsSecure() ? L"ConnectingSecure" : L"ConnectingInsecure";
  FFrameAnimation.Init(AnimationPaintBox, AnimationName);
  FFrameAnimation.Start();
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::ClearLog()
{
  // TListItems::Clear() does nothing without allocated handle
  LogView->HandleNeeded();
  LogView->Items->Clear();
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::Log(const UnicodeString Message)
{
  // HACK
  // The first call to Repaint from TFrameAnimation happens
  // even before the form is showing. After it is shown it takes sometime too long
  // before the animation is painted, so that the form is closed before the first frame even appers
  if (!FAnimationPainted && Showing)
  {
    AnimationPaintBox->Repaint();
    FAnimationPainted = true;
  }

  int Index = LogView->Items->Add(Message);
  MakeLogItemVisible(Index);
  LogView->Repaint();
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::MakeLogItemVisible(int Index)
{
  int CountVisible = LogView->Height / LogView->ItemHeight;
  if (Index < LogView->TopIndex)
  {
    LogView->TopIndex = Index;
  }
  else
  {
    int TotalHeight = 0;
    int Index2 = Index;
    while ((Index2 >= 0) && TotalHeight < LogView->ClientHeight)
    {
      TotalHeight += LogItemHeight(Index2);
      Index2--;
    }

    // Index2 is the last item above the first fully visible,
    // were the Index on the very bottom.
    if (LogView->TopIndex <= Index2 + 1)
    {
      LogView->TopIndex = Index2 + 2;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::AdjustControls()
{
  UnicodeString ACaption;
  if (FStatus.IsEmpty())
  {
    ACaption = FSessionData->SessionName;
  }
  else
  {
    ACaption = FORMAT(L"%s - %s", (FStatus, FSessionData->SessionName));
  }
  Caption = FormatFormCaption(this, ACaption);
}
//---------------------------------------------------------------------------
TLabel * __fastcall TAuthenticateForm::GenerateLabel(int Current, UnicodeString Caption)
{
  TLabel * Result = new TLabel(this);
  Result->Parent = FPromptParent;

  Result->Anchors = TAnchors() << akLeft << akTop << akRight;
  Result->WordWrap = true;
  Result->AutoSize = false;

  Result->Top = Current;
  Result->Left = FPromptLeft;
  Result->Caption = Caption;
  int Width = FPromptParent->ClientWidth - FPromptLeft - FPromptRight;
  Result->Width = Width;

  return Result;
}
//---------------------------------------------------------------------------
TCustomEdit * __fastcall TAuthenticateForm::GenerateEdit(int Current, bool Echo)
{
  TCustomEdit * Result = (Echo ? static_cast<TCustomEdit *>(new TEdit(this)) :
    static_cast<TCustomEdit *>(new TPasswordEdit(this)));
  Result->Parent = FPromptParent;

  Result->Anchors = TAnchors() << akLeft << akTop << akRight;
  Result->Top = Current;
  Result->Left = FPromptLeft;
  Result->Width = FPromptParent->ClientWidth - FPromptLeft - FPromptRight;

  return Result;
}
//---------------------------------------------------------------------------
TList * __fastcall TAuthenticateForm::GeneratePrompt(UnicodeString Instructions,
  TStrings * Prompts)
{
  while (FPromptParent->ControlCount > 0)
  {
    delete FPromptParent->Controls[0];
  }
  TList * Result = new TList;

  int Current = FPromptTop;

  if (!Instructions.IsEmpty())
  {
    TLabel * Label = GenerateLabel(Current, Instructions);
    Current += Label->Height + FPromptsGap;
  }

  for (int Index = 0; Index < Prompts->Count; Index++)
  {
    if (Index > 0)
    {
      Current += FPromptEditGap;
    }

    TLabel * Label = GenerateLabel(Current, Prompts->Strings[Index]);
    Current += Label->Height + FPromptEditGap;

    bool Echo = FLAGSET(int(Prompts->Objects[Index]), pupEcho);
    TCustomEdit * Edit = GenerateEdit(Current, Echo);
    Result->Add(Edit);
    Label->FocusControl = Edit;
    Current += Edit->Height;
  }

  FPromptParent->ClientHeight = Current;

  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TAuthenticateForm::PromptUser(TPromptKind Kind, UnicodeString Name,
  UnicodeString Instructions, TStrings * Prompts, TStrings * Results, bool ForceLog,
  bool StoredCredentialsTried)
{

  bool Result;
  TList * Edits = GeneratePrompt(Instructions, Prompts);

  try
  {
    bool ShowSessionRememberPasswordPanel = false;
    bool ShowSavePasswordPanel = false;
    TSessionData * Data = NULL;
    if (IsPasswordPrompt(Kind, Prompts) && StoredCredentialsTried)
    {
      Data = StoredSessions->FindSame(FSessionData);
      ShowSavePasswordPanel = (Data != NULL) && !Data->Password.IsEmpty();
    }
    // do not offer to remember password,
    // if we are offering to save the password to stored site
    if (!ShowSavePasswordPanel &&
        (Prompts->Count == 1) &&
        FLAGSET(int(Prompts->Objects[0]), pupRemember) &&
        DebugAlwaysTrue(IsPasswordOrPassphrasePrompt(Kind, Prompts)))
    {
      ShowSessionRememberPasswordPanel = true;
    }

    SavePasswordCheck->Checked = false;
    SavePasswordPanel->Visible = ShowSavePasswordPanel;
    SessionRememberPasswordCheck->Checked = false;
    SessionRememberPasswordPanel->Visible = ShowSessionRememberPasswordPanel;

    if (PasswordPanel->AutoSize)
    {
      PasswordPanel->AutoSize = false;
      PasswordPanel->AutoSize = true;
    }
    PasswordPanel->Realign();

    DebugAssert(Results->Count == Edits->Count);
    for (int Index = 0; Index < Edits->Count; Index++)
    {
      TCustomEdit * Edit = reinterpret_cast<TCustomEdit *>(Edits->Items[Index]);
      Edit->Text = Results->Strings[Index];
    }

    Result = Execute(Name, PasswordPanel,
      ((Edits->Count > 0) ?
         reinterpret_cast<TWinControl *>(Edits->Items[0]) :
         static_cast<TWinControl *>(PasswordOKButton)),
      PasswordOKButton, PasswordCancelButton, true, false, ForceLog);
    if (Result)
    {
      for (int Index = 0; Index < Edits->Count; Index++)
      {
        TCustomEdit * Edit = reinterpret_cast<TCustomEdit *>(Edits->Items[Index]);
        Results->Strings[Index] = Edit->Text;

        Prompts->Objects[Index] = (TObject *)
          ((int(Prompts->Objects[Index]) & ~pupRemember) |
           FLAGMASK(((Index == 0) && SessionRememberPasswordCheck->Checked), pupRemember));
      }

      if (SavePasswordCheck->Checked)
      {
        DebugAssert(Data != NULL);
        DebugAssert(Results->Count >= 1);
        FSessionData->Password = Results->Strings[0];
        Data->Password = Results->Strings[0];
        // modified only, explicit
        StoredSessions->Save(false, true);
      }
    }
  }
  __finally
  {
    delete Edits;
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::Banner(const UnicodeString & Banner,
  bool & NeverShowAgain, int Options)
{
  BannerMemo->Lines->Text = Banner;
  NeverShowAgainCheck->Visible = FLAGCLEAR(Options, boDisableNeverShowAgain);
  NeverShowAgainCheck->Checked = NeverShowAgain;
  bool Result = Execute(LoadStr(AUTHENTICATION_BANNER), BannerPanel, BannerCloseButton,
    BannerCloseButton, BannerCloseButton, false, true, false);
  if (Result)
  {
    NeverShowAgain = NeverShowAgainCheck->Checked;
  }
}
//---------------------------------------------------------------------------
bool __fastcall TAuthenticateForm::Execute(UnicodeString Status, TPanel * Panel,
  TWinControl * FocusControl, TButton * DefaultButton, TButton * CancelButton,
  bool FixHeight, bool Zoom, bool ForceLog)
{
  TModalResult DefaultButtonResult;
  TAlign Align = Panel->Align;
  try
  {
    // If not visible yet, the animation was not even initialized yet
    if (Visible)
    {
      FFrameAnimation.Stop();
    }
    DebugAssert(FStatus.IsEmpty());
    FStatus = Status;
    DefaultButton->Default = true;
    CancelButton->Cancel = true;

    DefaultButtonResult = DefaultResult(this);

    if (Zoom)
    {
      Panel->Align = alClient;
    }

    if (ForceLog || Visible)
    {
      if (ClientHeight < Panel->Height)
      {
        ClientHeight = Panel->Height;
      }
      // Panel being hidden gets not realigned automatically, even if it
      // has Align property set
      Panel->Top = ClientHeight - Panel->Height;
      Panel->Show();
      TCursor PrevCursor = Screen->Cursor;
      try
      {
        if (Zoom)
        {
          TopPanel->Hide();
        }
        else
        {
          if (LogView->Items->Count > 0)
          {
            // To avoid the scrolling effect when setting TopIndex
            LogView->Items->BeginUpdate();
            try
            {
              MakeLogItemVisible(LogView->Items->Count - 1);
            }
            __finally
            {
              LogView->Items->EndUpdate();
              RedrawLog();
            }
          }
        }
        Screen->Cursor = crDefault;

        if (!Visible)
        {
          DebugAssert(ForceLog);
          ShowAsModal();
        }

        ActiveControl = FocusControl;
        ModalResult = mrNone;
        AdjustControls();
        do
        {
          Application->HandleMessage();
        }
        while (!Application->Terminated && (ModalResult == mrNone));
      }
      __finally
      {
        Panel->Hide();
        Screen->Cursor = PrevCursor;
        if (Zoom)
        {
          TopPanel->Show();
        }
        Repaint();
      }
    }
    else
    {
      int PrevHeight = ClientHeight;
      int PrevMinHeight = Constraints->MinHeight;
      int PrevMaxHeight = Constraints->MaxHeight;
      try
      {
        Constraints->MinHeight = 0;
        ClientHeight = Panel->Height;
        if (FixHeight)
        {
          Constraints->MinHeight = Height;
          Constraints->MaxHeight = Height;
        }
        TopPanel->Hide();
        Panel->Show();
        FFocusControl = FocusControl;

        ShowModal();
      }
      __finally
      {
        FFocusControl = NULL;
        ClientHeight = PrevHeight;
        Constraints->MinHeight = PrevMinHeight;
        Constraints->MaxHeight = PrevMaxHeight;
        Panel->Hide();
        TopPanel->Show();
      }
    }
  }
  __finally
  {
    Panel->Align = Align;
    DefaultButton->Default = false;
    CancelButton->Cancel = false;
    FStatus = L"";
    AdjustControls();
    FFrameAnimation.Start();
  }

  bool Result = (ModalResult == DefaultButtonResult);

  if (!Result)
  {
    // This is not nice as it may ultimately route to
    // TTerminalThread::Cancel() and throw fatal exception,
    // what actually means that any PromptUser call during authentication never
    // return false and their fall back/alternative code never occurs.
    // It probably needs fixing.
    DoCancel();
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::HelpButtonClick(TObject * /*Sender*/)
{
  FormHelp(this);
}
//---------------------------------------------------------------------------
int __fastcall TAuthenticateForm::LogItemHeight(int Index)
{
  UnicodeString S = LogView->Items->Strings[Index];
  TRect TextRect(0, 0, LogView->ClientWidth - (2 * FHorizontalLogPadding), 0);
  LogView->Canvas->Font = LogView->Font;
  LogView->Canvas->TextRect(TextRect, S, FLogTextFormat + (TTextFormat() << tfCalcRect));
  int Result = TextRect.Height() + (2 * FVerticalLogPadding);
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::LogViewMeasureItem(TWinControl * /*Control*/, int Index,
  int & Height)
{
  Height = LogItemHeight(Index);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::LogViewDrawItem(TWinControl * /*Control*/, int Index,
  TRect & Rect, TOwnerDrawState /*State*/)
{
  // Reset back to base colors. We do not want to render selection.
  // + At initial phases the canvas font will not yet reflect he desktop font.
  LogView->Canvas->Font = LogView->Font;
  LogView->Canvas->Brush->Color = LogView->Color;

  LogView->Canvas->FillRect(Rect);

  int Height = LogItemHeight(Index);
  // tfVerticalCenter does not seem to center the text,
  // so we need to deflate the vertical size too
  Rect.Inflate(-FHorizontalLogPadding,  -FVerticalLogPadding);
  UnicodeString S = LogView->Items->Strings[Index];
  LogView->Canvas->TextRect(Rect, S, FLogTextFormat);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::RedrawLog()
{
  // Redraw including the scrollbar (RDW_FRAME)
  RedrawWindow(LogView->Handle, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::FormResize(TObject * /*Sender*/)
{
  if (LogView->Showing)
  {
    // Mainly to avoid the scrolling effect when setting TopIndex
    LogView->Items->BeginUpdate();
    try
    {
      // Rebuild the list view to force Windows to resend WM_MEASUREITEM
      int ItemIndex = LogView->ItemIndex;
      int TopIndex = LogView->TopIndex;
      std::unique_ptr<TStringList> Items(new TStringList);
      Items->Assign(LogView->Items);
      LogView->Items->Assign(Items.get());
      LogView->ItemIndex = ItemIndex;
      LogView->TopIndex = TopIndex;
    }
    __finally
    {
      LogView->Items->EndUpdate();
      RedrawLog();
    }
  }
}
//---------------------------------------------------------------------------
