// SPDX-License-Identifier: GPL-3.0-only
// Change rendered ink only. The canonical RTF and its other character/paragraph
// formatting remain intact. CHARFORMATW has the same 92-byte ABI on x86/x64.
type
  TLicenseCharFormat = record
    Size, Mask, Effects: LongWord;
    Height, Offset: Integer;
    TextColor: LongWord;
    CharSet, PitchAndFamily: Byte;
    FaceName: array[0..31] of Word;
    Padding: Word;
  end;

function LicenseFormatMessage(Window: HWND; Message: Cardinal; WParam: WPARAM;
  var Format: TLicenseCharFormat): LRESULT;
  external 'SendMessageW@user32.dll stdcall';
function LicenseSystemColor(Index: Integer): LongWord;
  external 'GetSysColor@user32.dll stdcall';

procedure ApplyLicenseColors;
var Format: TLicenseCharFormat; Ink: LongWord;
begin
  if IsDarkInstallMode and not HighContrastActive then Ink := $FFFFFF
  else Ink := LicenseSystemColor(8); { COLOR_WINDOWTEXT; respects high contrast }
  Format.Size := 92;
  Format.Mask := $40000000; { CFM_COLOR: never change bold, font or paragraphs }
  Format.Effects := 0;
  Format.TextColor := Ink;
  if LicenseFormatMessage(WizardForm.LicenseMemo.Handle, $444, 4, Format) = 0 then
    RaiseException('Cannot apply readable license agreement colors.'); { EM_SETCHARFORMAT, SCF_ALL }
end;

procedure CheckLicenseColors;
var Format: TLicenseCharFormat; Ink: LongWord; Memo: TRichEditViewer;
begin
  Memo := WizardForm.LicenseMemo;
  if IsDarkInstallMode and not HighContrastActive then Ink := $FFFFFF
  else Ink := LicenseSystemColor(8);
  Memo.SelStart := 0;
  Memo.SelLength := Length(Memo.Text);
  Format.Size := 92;
  LicenseFormatMessage(Memo.Handle, $43A, 1, Format); { EM_GETCHARFORMAT, SCF_SELECTION }
  if ((Format.Mask and $40000000) = 0) or (Format.TextColor <> Ink) or
    ((Format.Effects and $40000000) <> 0) then
    RaiseException('License agreement has unreadable or mixed foreground colors.');
  if (Format.Mask and 1) <> 0 then
    RaiseException('License agreement lost its mixed bold/plain formatting.');
  Memo.SelLength := 1;
  LicenseFormatMessage(Memo.Handle, $43A, 1, Format);
  if ((Format.Mask and 1) = 0) or ((Format.Effects and 1) = 0) or
    (Pos('fnord', Memo.Text) = 0) or (Pos('TL;DR', Memo.Text) = 0) then
    RaiseException('License agreement lost its heading or complete RTF content.');
  Memo.SelLength := 0;
  Log('PASS: complete license ink and RTF formatting; dark=' + IntToStr(Ord(IsDarkInstallMode)) +
    '; high-contrast=' + IntToStr(Ord(HighContrastActive)) + '; ink=' + IntToStr(Ink));
end;
