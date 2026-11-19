// Bounded JSON reader for the generated build-info contract. No registry value
// or filename is treated as a version. Unknown fields remain forward compatible.
var
  JsonText: String;
  JsonIndex: Integer;

procedure JsonSpace;
begin
  while (JsonIndex <= Length(JsonText)) and
    (Ord(JsonText[JsonIndex]) <= 32) do JsonIndex := JsonIndex + 1;
end;

procedure JsonExpect(C: Char);
begin
  JsonSpace;
  if (JsonIndex > Length(JsonText)) or (JsonText[JsonIndex] <> C) then
    RaiseException('Invalid build-info.json');
  JsonIndex := JsonIndex + 1;
end;

function JsonString: String;
var C: Char; Hex, Value, I: Integer;
begin
  Result := '';
  JsonExpect('"');
  while JsonIndex <= Length(JsonText) do begin
    C := JsonText[JsonIndex];
    JsonIndex := JsonIndex + 1;
    if C = '"' then exit;
    if Ord(C) < 32 then RaiseException('Invalid JSON string');
    if C = '\' then begin
      if JsonIndex > Length(JsonText) then RaiseException('Incomplete JSON escape');
      C := JsonText[JsonIndex];
      JsonIndex := JsonIndex + 1;
      case C of
        '"', '\', '/': Result := Result + C;
        'b': Result := Result + #8;
        'f': Result := Result + #12;
        'n': Result := Result + #10;
        'r': Result := Result + #13;
        't': Result := Result + #9;
        'u': begin
          Value := 0;
          for I := 1 to 4 do begin
            if JsonIndex > Length(JsonText) then RaiseException('Incomplete Unicode escape');
            Hex := Pos(Uppercase(JsonText[JsonIndex]), '0123456789ABCDEF') - 1;
            if Hex < 0 then RaiseException('Invalid Unicode escape');
            Value := Value * 16 + Hex;
            JsonIndex := JsonIndex + 1;
          end;
          Result := Result + Chr(Value);
        end;
      else RaiseException('Invalid JSON escape');
      end;
    end else Result := Result + C;
  end;
  RaiseException('Unterminated JSON string');
end;

procedure JsonSkip(Depth: Integer);
var C: Char; Ignored: String; Start: Integer;
begin
  if Depth > 16 then RaiseException('JSON nesting limit');
  JsonSpace;
  if JsonIndex > Length(JsonText) then RaiseException('Incomplete JSON');
  C := JsonText[JsonIndex];
  if C = '"' then begin Ignored := JsonString; exit; end;
  if (C = '{') or (C = '[') then begin
    JsonIndex := JsonIndex + 1;
    JsonSpace;
    while (JsonIndex <= Length(JsonText)) and
      (JsonText[JsonIndex] <> '}') and (JsonText[JsonIndex] <> ']') do begin
      if C = '{' then begin Ignored := JsonString; JsonExpect(':'); end;
      JsonSkip(Depth + 1);
      JsonSpace;
      if (JsonIndex <= Length(JsonText)) and (JsonText[JsonIndex] = ',') then
        JsonIndex := JsonIndex + 1
      else break;
    end;
    if C = '{' then JsonExpect('}') else JsonExpect(']');
    exit;
  end;
  Start := JsonIndex;
  while (JsonIndex <= Length(JsonText)) and
    (Pos(JsonText[JsonIndex], ',}] ' + #9 + #10 + #13) = 0) do
    JsonIndex := JsonIndex + 1;
  if Start = JsonIndex then RaiseException('Invalid JSON value');
end;

function ValidVersion(Value: String): Boolean;
var I, Part, Digits: Integer;
begin
  Part := 0; Digits := 0; Result := False;
  for I := 1 to Length(Value) do begin
    if Value[I] = '.' then begin
      if Digits = 0 then exit;
      Part := Part + 1; Digits := 0;
    end else begin
      if (Value[I] < '0') or (Value[I] > '9') then exit;
      Digits := Digits + 1;
      if Digits > 5 then exit;
    end;
  end;
  Result := (Part = 2) and (Digits > 0);
end;

function VersionComponent(var Value: String): Integer;
var P: Integer; Part: String;
begin
  P := Pos('.', Value);
  if P = 0 then begin Part := Value; Value := ''; end
  else begin Part := Copy(Value, 1, P - 1); Delete(Value, 1, P); end;
  Result := StrToIntDef(Part, 0);
end;

function NewerVersion(NewVersion, OldVersion: String): Boolean;
var I, A, B: Integer;
begin
  Result := False;
  if not ValidVersion(NewVersion) or not ValidVersion(OldVersion) then exit;
  for I := 1 to 3 do begin
    A := VersionComponent(NewVersion); B := VersionComponent(OldVersion);
    if A <> B then begin Result := A > B; exit; end;
  end;
end;

function SafeRuntimePath(Value: String): Boolean;
begin
  // Reject rooted paths, alternate streams and traversal before deletion.
  Result := (Value <> '') and (Value[1] <> '/') and (Value[1] <> '\') and
    (Pos(':', Value) = 0) and (Pos('..', Value) = 0) and
    (Pos(#0, Value) = 0) and (Pos('*', Value) = 0) and (Pos('?', Value) = 0);
end;

function ReadBuildInfo(FileName: String; var Version: String; Files: TStrings): Boolean;
var Bytes: AnsiString; Key, Product, Entry: String;
    SeenVersion, SeenProduct: Boolean;
begin
  Result := False; Version := ''; Product := ''; Files.Clear;
  SeenVersion := False; SeenProduct := False;
  if not LoadStringFromFile(FileName, Bytes) or (Length(Bytes) > 4194304) then exit;
  JsonText := UTF8Decode(Bytes); JsonIndex := 1;
  if (Length(JsonText) > 0) and (Ord(JsonText[1]) = $FEFF) then JsonIndex := 2;
  try
    JsonExpect('{'); JsonSpace;
    while (JsonIndex <= Length(JsonText)) and (JsonText[JsonIndex] <> '}') do begin
      Key := JsonString; JsonExpect(':');
      if Key = 'product' then begin
        if SeenProduct then RaiseException('Duplicate product');
        SeenProduct := True; Product := JsonString;
      end else if Key = 'version' then begin
        if SeenVersion then RaiseException('Duplicate version');
        SeenVersion := True; Version := JsonString;
      end else if Key = 'runtime_files' then begin
        JsonExpect('['); JsonSpace;
        while (JsonIndex <= Length(JsonText)) and (JsonText[JsonIndex] <> ']') do begin
          Entry := JsonString;
          if not SafeRuntimePath(Entry) or (Files.Count >= 10000) then
            RaiseException('Unsafe runtime manifest');
          Files.Add(Entry); JsonSpace;
          if (JsonIndex <= Length(JsonText)) and (JsonText[JsonIndex] = ',') then
            JsonIndex := JsonIndex + 1 else break;
        end;
        JsonExpect(']');
      end else JsonSkip(1);
      JsonSpace;
      if (JsonIndex <= Length(JsonText)) and (JsonText[JsonIndex] = ',') then
        JsonIndex := JsonIndex + 1 else break;
    end;
    JsonExpect('}'); JsonSpace;
    Result := (JsonIndex > Length(JsonText)) and (Product = 'AudioHole') and ValidVersion(Version);
  except
    Result := False;
  end;
  if not Result then begin Version := ''; Files.Clear; end;
end;
