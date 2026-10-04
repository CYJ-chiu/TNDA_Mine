# Source 原始碼協作規則

本文件套用於 `Source/` 下所有手寫原始碼；與上層 `AGENTS.md` 一併遵守。

## 繁體中文註解

新增或修改程式碼時，必須同步新增或更新詳盡的繁體中文註解：

- 類別、結構、列舉、函式、成員與 Unreal 屬性的宣告處，說明責任、使用時機、重要限制、
  副作用及失敗行為；適用時補充 Unreal 生命週期、跨類別協作與 Editor 設定位置。
- 實作中的註解說明程式碼本身看不出的原因、時序、單位、引擎限制與備援路徑；不逐行翻譯語法。
- 行為、函式簽名、資料欄位、Editor 屬性、前置條件或備援流程改變時，同步修改所有受影響的
  Doxygen、行內註解、模組 README 與其功能文件，移除已失效的說明。功能目錄另有 `AGENTS.md`
  時，依其中的原始碼與文件對應關係執行。
- 識別符維持英文；Blueprint、PlayerStart、PIE 等 Unreal 術語可保留原文，說明文字使用繁體中文。

## 換行符號

- `Source/` 下的手寫原始碼一律使用 CRLF；不得混用 CRLF 與 LF。
- 修改 `.h`、`.hpp`、`.cpp`、`.inl`、`.Build.cs`、`.Target.cs` 後，必須統一為 CRLF。
- 完成前對每個修改過的原始碼檔執行 `git ls-files --eol <檔案>`，結果必須為 `w/crlf`，不得為 `w/mixed` 或 `w/lf`。

## 新檔案版權

在 `Source/` 下新增任何可使用 `//` 註解的原始碼檔案時，第一行必須完全一致地寫為：

```cpp
// Copyright © 2026 USERJOY Technology Co., Ltd.All Rights Reserved.
```

此規則包含 `.h`、`.hpp`、`.cpp`、`.inl`、模組 `.Build.cs` 與 `.Target.cs`。

## 完成條件

- 完成前重讀本次 `Source/` diff，逐項確認每個行為與宣告變更都有正確、未過期的繁體中文註解。
- 逐一檢查本次新增的原始碼檔案，確認第一行與指定版權文字逐字相同。
