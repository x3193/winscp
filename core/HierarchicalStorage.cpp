//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "Common.h"
#include "PuttyIntf.h"
#include "HierarchicalStorage.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
#define READ_REGISTRY(Method) \
  if (FRegistry->ValueExists(Name)) \
  try { return FRegistry->Method(Name); } catch(...) { FFailed++; return Default; } \
  else return Default;
#define WRITE_REGISTRY(Method) \
  try { FRegistry->Method(Name, Value); } catch(...) { FFailed++; }
//---------------------------------------------------------------------------
AnsiString __fastcall MungeStr(const AnsiString Str)
{
  AnsiString Result;
  Result.SetLength(Str.Length() * 3 + 1);
  putty_mungestr(Str.c_str(), Result.c_str());
  PackStr(Result);
  return Result;
}
//---------------------------------------------------------------------------
AnsiString __fastcall UnMungeStr(const AnsiString Str)
{
  AnsiString Result;
  Result.SetLength(Str.Length() * 3 + 1);
  putty_unmungestr(Str.c_str(), Result.c_str(), Result.Length());
  PackStr(Result);
  return Result;
}
//===========================================================================
__fastcall THierarchicalStorage::THierarchicalStorage(const AnsiString AStorage)
{
  FStorage = AStorage;
  FKeyHistory = new TStringList();
  AccessMode = smRead;
}
//---------------------------------------------------------------------------
__fastcall THierarchicalStorage::~THierarchicalStorage()
{
  delete FKeyHistory;
}
//---------------------------------------------------------------------------
void __fastcall THierarchicalStorage::SetAccessMode(TStorageAccessMode value)
{
  FAccessMode = value;
}
//---------------------------------------------------------------------------
AnsiString __fastcall THierarchicalStorage::GetCurrentSubKey()
{
  if (FKeyHistory->Count) return FKeyHistory->Strings[FKeyHistory->Count-1];
    else return "";
}
//---------------------------------------------------------------------------
bool __fastcall THierarchicalStorage::OpenRootKey(bool CanCreate)
{
  return OpenSubKey("", CanCreate);
}
//---------------------------------------------------------------------------
bool __fastcall THierarchicalStorage::OpenSubKey(const AnsiString SubKey, bool )
{
  FKeyHistory->Add(IncludeTrailingBackslash(CurrentSubKey+SubKey));
  return true;
}
//---------------------------------------------------------------------------
bool __fastcall THierarchicalStorage::CreateSubKey(const AnsiString SubKey)
{
  FKeyHistory->Add(IncludeTrailingBackslash(CurrentSubKey+SubKey));
  return true;
}
//---------------------------------------------------------------------------
void __fastcall THierarchicalStorage::CloseSubKey()
{
  if (FKeyHistory->Count == 0) throw Exception("");
    else FKeyHistory->Delete(FKeyHistory->Count-1);
}
//---------------------------------------------------------------------------
void __fastcall THierarchicalStorage::RecursiveDeleteSubKey(const AnsiString Key)
{
  if (OpenSubKey(Key, false))
  {
    TStringList *SubKeys = new TStringList();
    try
    {
      GetSubKeyNames(SubKeys);
      for (int Index = 0; Index < SubKeys->Count; Index++)
      {
        RecursiveDeleteSubKey(SubKeys->Strings[Index]);
      }
    }
    __finally
    {
      delete SubKeys;
    }
    CloseSubKey();
  }
  DeleteSubKey(Key);
}
//---------------------------------------------------------------------------
bool __fastcall THierarchicalStorage::HasSubKeys()
{
  bool Result;
  TStrings * SubKeys = new TStringList();
  try
  {
    GetSubKeyNames(SubKeys);
    Result = (SubKeys->Count > 0);
  }
  __finally
  {
    delete SubKeys;
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall THierarchicalStorage::ReadValues(Classes::TStrings* Strings)
{
  TStrings * Names = new TStringList();
  try
  {
    GetValueNames(Names);
    for (int Index = 0; Index < Names->Count; Index++)
    {
      Strings->Add(ReadString(Names->Strings[Index], ""));
    }
  }
  __finally
  {
    delete Names;
  }
}
//---------------------------------------------------------------------------
void __fastcall THierarchicalStorage::WriteValues(Classes::TStrings* Strings)
{
  TStrings * Names = new TStringList();
  try
  {
    GetValueNames(Names);
    for (int Index = 0; Index < Names->Count; Index++)
    {
      DeleteValue(Names->Strings[Index]);
    }
  }
  __finally
  {
    delete Names;
  }

  if (Strings)
  {
    for (int Index = 0; Index < Strings->Count; Index++)
    {
      WriteString(IntToStr(Index), Strings->Strings[Index]);
    }
  }
}
//---------------------------------------------------------------------------
AnsiString __fastcall THierarchicalStorage::ReadString(const AnsiString Name, const AnsiString Default)
{
  return UnMungeStr(ReadStringRaw(Name, Default));
}
//---------------------------------------------------------------------------
void __fastcall THierarchicalStorage::WriteString(const AnsiString Name, const AnsiString Value)
{
  WriteStringRaw(Name, MungeStr(Value));
}
//---------------------------------------------------------------------------
AnsiString __fastcall THierarchicalStorage::IncludeTrailingBackslash(const AnsiString S) const
{
  return S.IsEmpty() ? S : ::IncludeTrailingBackslash(S);
}
//---------------------------------------------------------------------------
AnsiString __fastcall THierarchicalStorage::ExcludeTrailingBackslash(const AnsiString S) const
{
  return S.IsEmpty() ? S : ::ExcludeTrailingBackslash(S);
}
//===========================================================================
__fastcall TRegistryStorage::TRegistryStorage(const AnsiString AStorage):
  THierarchicalStorage(IncludeTrailingBackslash(AStorage))
{
  FFailed = 0;
  FRegistry = new TRegistry();
  FRegistry->Access = KEY_READ;
};
//---------------------------------------------------------------------------
__fastcall TRegistryStorage::~TRegistryStorage()
{
  delete FRegistry;
};
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::SetAccessMode(TStorageAccessMode value)
{
  THierarchicalStorage::SetAccessMode(value);
  if (FRegistry)
  {
    switch (AccessMode) {
      case smRead:
        FRegistry->Access = KEY_READ;
        break;

      case smReadWrite:
      default:
        FRegistry->Access = KEY_READ | KEY_WRITE;
        break;
    }
  }
}
//---------------------------------------------------------------------------
bool __fastcall TRegistryStorage::OpenSubKey(const AnsiString SubKey, bool CanCreate)
{
  bool Result;
  if (FKeyHistory->Count > 0) FRegistry->CloseKey();
  Result = FRegistry->OpenKey(
    ExcludeTrailingBackslash(Storage + CurrentSubKey + SubKey), CanCreate);
  if (Result) Result = THierarchicalStorage::OpenSubKey(SubKey, CanCreate);
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TRegistryStorage::CreateSubKey(const AnsiString SubKey)
{
  bool Result;
  if (FKeyHistory->Count) FRegistry->CloseKey();
  Result = FRegistry->CreateKey(ExcludeTrailingBackslash(Storage + CurrentSubKey + SubKey));
  if (Result) Result = THierarchicalStorage::CreateSubKey(CurrentSubKey + SubKey);
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::CloseSubKey()
{
  FRegistry->CloseKey();
  THierarchicalStorage::CloseSubKey();
  if (FKeyHistory->Count)
  {
    FRegistry->OpenKey(Storage + CurrentSubKey, True);
  }
}
//---------------------------------------------------------------------------
bool __fastcall TRegistryStorage::DeleteSubKey(const AnsiString SubKey)
{
  AnsiString K;
  if (FKeyHistory->Count == 0) K = Storage + CurrentSubKey;
  K += SubKey;
  return FRegistry->DeleteKey(K);
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::GetSubKeyNames(Classes::TStrings* Strings)
{
  FRegistry->GetKeyNames(Strings);
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::GetValueNames(Classes::TStrings* Strings)
{
  FRegistry->GetValueNames(Strings);
}
//---------------------------------------------------------------------------
bool __fastcall TRegistryStorage::DeleteValue(const AnsiString Name)
{
  return FRegistry->DeleteValue(Name);
}
//---------------------------------------------------------------------------
bool __fastcall TRegistryStorage::ReadBool(const AnsiString Name, bool Default)
{
  READ_REGISTRY(ReadBool);
}
//---------------------------------------------------------------------------
TDateTime __fastcall TRegistryStorage::ReadDateTime(const AnsiString Name, TDateTime Default)
{
  READ_REGISTRY(ReadDateTime);
}
//---------------------------------------------------------------------------
double __fastcall TRegistryStorage::ReadFloat(const AnsiString Name, double Default)
{
  READ_REGISTRY(ReadFloat);
}
//---------------------------------------------------------------------------
int __fastcall TRegistryStorage::ReadInteger(const AnsiString Name, int Default)
{
  READ_REGISTRY(ReadInteger);
}
//---------------------------------------------------------------------------
__int64 __fastcall TRegistryStorage::ReadInt64(const AnsiString Name, __int64 Default)
{
  __int64 Result = Default;
  if (FRegistry->ValueExists(Name))
  {
    try
    {
      FRegistry->ReadBinaryData(Name, &Result, sizeof(Result));
    }
    catch(...)
    {
      FFailed++;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
AnsiString __fastcall TRegistryStorage::ReadStringRaw(const AnsiString Name, const AnsiString Default)
{
  READ_REGISTRY(ReadString);
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::WriteBool(const AnsiString Name, bool Value)
{
  WRITE_REGISTRY(WriteBool);
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::WriteDateTime(const AnsiString Name, TDateTime Value)
{
  WRITE_REGISTRY(WriteDateTime);
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::WriteFloat(const AnsiString Name, double Value)
{
  WRITE_REGISTRY(WriteFloat);
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::WriteStringRaw(const AnsiString Name, const AnsiString Value)
{
  WRITE_REGISTRY(WriteString);
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::WriteInteger(const AnsiString Name, int Value)
{
  WRITE_REGISTRY(WriteInteger);
}
//---------------------------------------------------------------------------
void __fastcall TRegistryStorage::WriteInt64(const AnsiString Name, __int64 Value)
{
  try
  {
    FRegistry->WriteBinaryData(Name, &Value, sizeof(Value));
  }
  catch(...)
  {
    FFailed++;
  }
}
//---------------------------------------------------------------------------
int __fastcall TRegistryStorage::GetFailed()
{
  int Result = FFailed;
  FFailed = 0;
  return Result;
}
//===========================================================================
__fastcall TIniFileStorage::TIniFileStorage(const AnsiString AStorage):
  THierarchicalStorage(AStorage)
{
  FIniFile = new TIniFile(Storage);
}
//---------------------------------------------------------------------------
__fastcall TIniFileStorage::~TIniFileStorage()
{
  delete FIniFile;
}
//---------------------------------------------------------------------------
AnsiString __fastcall TIniFileStorage::GetCurrentSection()
{
  return ExcludeTrailingBackslash(CurrentSubKey);
}
//---------------------------------------------------------------------------
bool __fastcall TIniFileStorage::DeleteSubKey(const AnsiString SubKey)
{
  bool Result;
  try
  {
    FIniFile->EraseSection(CurrentSubKey + SubKey);
    Result = true;
  }
  catch (...)
  {
    Result = false;
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TIniFileStorage::GetSubKeyNames(Classes::TStrings* Strings)
{
  TStrings * Sections = new TStringList();
  try
  {
    FIniFile->ReadSections(Sections);
    for (int i = 0; i < Sections->Count; i++)
    {
      AnsiString Section = Sections->Strings[i];
      if (AnsiCompareText(CurrentSubKey,
          Section.SubString(1, CurrentSubKey.Length())) == 0)
      {
        Strings->Add(Section.SubString(CurrentSubKey.Length() + 1,
          Section.Length() - CurrentSubKey.Length()));
      }
    }
  }
  __finally
  {
    delete Sections;
  }
}
//---------------------------------------------------------------------------
void __fastcall TIniFileStorage::GetValueNames(Classes::TStrings* Strings)
{
  return FIniFile->ReadSection(CurrentSection, Strings);
}
//---------------------------------------------------------------------------
bool __fastcall TIniFileStorage::DeleteValue(const AnsiString Name)
{
  FIniFile->DeleteKey(CurrentSection, Name);
  return true;
}
//---------------------------------------------------------------------------
bool __fastcall TIniFileStorage::ReadBool(const AnsiString Name, bool Default)
{
  return FIniFile->ReadBool(CurrentSection, Name, Default);
}
//---------------------------------------------------------------------------
int __fastcall TIniFileStorage::ReadInteger(const AnsiString Name, int Default)
{
  return FIniFile->ReadInteger(CurrentSection, Name, Default);
}
//---------------------------------------------------------------------------
__int64 __fastcall TIniFileStorage::ReadInt64(const AnsiString Name, __int64 Default)
{
  __int64 Result = Default;
  AnsiString Str;
  Str = ReadStringRaw(Name, "");
  if (!Str.IsEmpty())
  {
    Result = StrToInt64Def(Str, Default);
  }
  return Result;
}
//---------------------------------------------------------------------------
TDateTime __fastcall TIniFileStorage::ReadDateTime(const AnsiString Name, TDateTime Default)
{
  return FIniFile->ReadDateTime(CurrentSection, Name, Default);
}
//---------------------------------------------------------------------------
double __fastcall TIniFileStorage::ReadFloat(const AnsiString Name, double Default)
{
  return FIniFile->ReadFloat(CurrentSection, Name, Default);
}
//---------------------------------------------------------------------------
AnsiString __fastcall TIniFileStorage::ReadStringRaw(const AnsiString Name, AnsiString Default)
{
  return FIniFile->ReadString(CurrentSection, Name, Default);
}
//---------------------------------------------------------------------------
void __fastcall TIniFileStorage::WriteBool(const AnsiString Name, bool Value)
{
  FIniFile->WriteBool(CurrentSection, Name, Value);
}
//---------------------------------------------------------------------------
void __fastcall TIniFileStorage::WriteInteger(const AnsiString Name, int Value)
{
  FIniFile->WriteInteger(CurrentSection, Name, Value);
}
//---------------------------------------------------------------------------
void __fastcall TIniFileStorage::WriteInt64(const AnsiString Name, __int64 Value)
{
  WriteStringRaw(Name, IntToStr(Value));
}
//---------------------------------------------------------------------------
void __fastcall TIniFileStorage::WriteDateTime(const AnsiString Name, TDateTime Value)
{
  FIniFile->WriteDateTime(CurrentSection, Name, Value);
}
//---------------------------------------------------------------------------
void __fastcall TIniFileStorage::WriteFloat(const AnsiString Name, double Value)
{
  FIniFile->WriteFloat(CurrentSection, Name, Value);
}
//---------------------------------------------------------------------------
void __fastcall TIniFileStorage::WriteStringRaw(const AnsiString Name, const AnsiString Value)
{
  FIniFile->WriteString(CurrentSection, Name, Value);
}








