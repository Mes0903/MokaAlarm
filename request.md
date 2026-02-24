# MokaBoard — Trello-style Task Board with Windows Task Scheduler Integration

## 一、原始需求（概述）

一個 Windows 桌面看板程式（類 Trello），使用者可建立多個 List，每個 List 中有多張 Card，Card 之間可跨 List 拖曳排序。Card 可附帶 Windows 工作排程器任務，在指定時間自動執行動作。點開 Card 可呼出浮動的便利貼視窗（Card Detail），即使主視窗最小化，便利貼仍維持可見；應用程式關閉時所有視窗一起消失。

---

## 二、技術規格

### 平台與語言

- **作業系統**：Windows only
- **語言標準**：C++20
- **GUI 框架**：ImGui（Dear ImGui）
  - Platform backend：`imgui_impl_win32`（直接接 Win32，不使用 GLFW）
  - Renderer backend：`imgui_impl_vulkan`
- **圖形 API**：Vulkan（透過系統已安裝的 VulkanSDK）
- **資料格式**：JSON（nlohmann/json）
- **Build System**：CMake 3.20+
- **程式碼風格**：Clang-format（LLVM base，160 char line limit）

### 架構

- **MVC 分層**：
  - `Model`：純資料結構 + `to_json()` / `from_json()`
  - `Controller`：業務邏輯、Windows API / COM API 呼叫
  - `View`：ImGui 渲染函式，只畫畫面，透過 Controller 操作資料
- **PersistenceService**：統一負責 JSON 的讀寫（`data/board.json`）
- **System Tray**：程式常駐於系統匣，主視窗可隱藏/顯示

### Win32 視窗策略

兩個 Win32 視窗共用同一個 Vulkan instance / device / queue，但各自有獨立的 swapchain：

| 視窗 | 角色 | 樣式 |
|------|------|------|
| `hwndBoard_` | 主看板視窗 | 標準 `WS_OVERLAPPEDWINDOW`，有工作列圖示 |
| `hwndNotes_` | 便利貼覆蓋層 | `WS_EX_LAYERED` + `WS_EX_TOPMOST`，無工作列圖示，透明背景 |

- **最小化主視窗** → `hwndBoard_` 縮到工作列，`hwndNotes_` 維持可見
- **關閉應用程式** → 兩個視窗同時消失（process 結束）
- ImGui Multi-Viewport（`ImGuiConfigFlags_ViewportsEnable`）用來在兩個視窗間無縫渲染

### Console 視窗控制（CMake）

- **Debug build**：保留 console（方便 log 輸出）
- **Release build**：使用 `WIN32` subsystem，不出現 CMD 視窗
- MSVC Release 使用 `/ENTRY:mainCRTStartup` 保留 `int main()` 入口點

### 專案結構（目標）

```
Alarm/
├── CMakeLists.txt
├── .clang-format
├── 3rdparty/
│   ├── imgui/            ← git submodule (ocornut/imgui)
│   ├── ImGuiFileDialog/  ← git submodule (aiekick/ImGuiFileDialog)
│   └── json/             ← git submodule (nlohmann/json)
├── include/
│   ├── Application.h     ← 骨架層（Win32 + Vulkan + ImGui loop）
│   ├── models/           ← 資料結構
│   ├── controllers/      ← 業務邏輯
│   └── views/            ← ImGui 渲染
├── src/
│   ├── main.cpp
│   ├── Application.cpp
│   ├── controllers/
│   └── views/
└── data/
    └── board.json        ← 持久化資料
```

### 命名規範

| 對象 | 規範 | 範例 |
|------|------|------|
| 類別/結構 | PascalCase | `Card`, `BoardList`, `ScheduleTrigger` |
| 檔案名稱 | PascalCase + `.h/.cpp` | `CardModel.h`, `BoardController.cpp` |
| 公開方法 | camelCase | `addCard()`, `moveCard()` |
| 私有方法 | camelCase + 尾端 `_` | `saveToJson_()`, `createTask_()` |
| 私有成員變數 | 尾端 `_` | `lists_`, `cards_` |
| 常數 | UPPER_SNAKE_CASE | `DEFAULT_CARD_WIDTH` |
| 命名空間 | snake_case | `moka::model` |

---

## 三、功能規格

### 3.1 主看板視窗（Board View）

#### 版面結構

```
┌─────────────────────────────────────────────────────────────┐
│  [+ New List]                                    [≡ Menu]   │  ← 工具列
│─────────────────────────────────────────────────────────────│
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Todo         │  │ In Progress  │  │ Done         │      │  ← List 欄
│  │──────────────│  │──────────────│  │──────────────│      │
│  │ [Card 1]     │  │ [Card 3]     │  │ [Card 5]     │      │  ← Card
│  │ [Card 2]     │  │              │  │              │      │
│  │ [+ Add Card] │  │ [+ Add Card] │  │ [+ Add Card] │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

- List 以欄位（column）方式橫向排列
- Card 在 List 內縱向堆疊，可**跨 List 拖曳**重新排序
- 每個 List 底部有 `[+ Add Card]` 快速新增按鈕
- 工具列有 `[+ New List]` 新增欄位

#### Card 在看板上的外觀

```
┌──────────────────────────┐
│ ● [tag1] [tag2]          │  ← 顏色圓點 + 標籤
│ Card Title               │  ← 標題
│ ☐ 2/3 subtasks           │  ← checkbox 進度（若有）
│              [🗓] [📋]   │  ← 有排程 / 開啟便利貼
└──────────────────────────┘
```

### 3.2 Card（卡片）資料欄位

| 欄位 | 類型 | 說明 |
|------|------|------|
| `id` | UUID string | 唯一識別碼 |
| `title` | string（必填）| 卡片標題 |
| `description` | string | 多行描述文字 |
| `checkboxes` | `CheckboxItem[]` | Checklist 子項目（label + checked） |
| `color` | `#RRGGBB` string | 顏色標籤（套用於卡片左側色條） |
| `tags` | `string[]` | 標籤列表，用於 filtering |
| `list_id` | UUID string | 所屬 List |
| `position` | int | 在 List 中的排序位置 |
| `note_pos` | `{x, y}` | 便利貼浮動視窗位置（持久化） |
| `note_size` | `{w, h}` | 便利貼浮動視窗大小（持久化） |
| `note_visible` | bool | 便利貼是否顯示中 |
| `schedule` | `Schedule?` | 可選的排程屬性（見 3.4） |

### 3.3 便利貼視窗（Card Detail / Floating Note）

- 點擊 Card 上的 `[📋]` 按鈕呼出，對應一張 Card 的詳細視圖
- 渲染在 `hwndNotes_` 覆蓋層上，**主視窗最小化時仍可見**
- 應用程式關閉時隨 process 結束消失

#### 便利貼視覺結構

```
┌──────────────────────────────────┐  ← ImGui 邊框（含 resize）
│ [Card Title]          [▼] [×]  │  ← 標題 + 選項按鈕
│ ─────────────────────────────── │
│ ☐ checkbox 項目一               │
│ ☐ checkbox 項目二               │
│                                  │
│ 多行描述文字輸入...               │
│                                  │
│ Tags: [tag1] [tag2] [+ tag]     │
└──────────────────────────────────┘
```

- **`[▼]`**：展開下拉選單 — 顏色選擇（預設色 + 調色盤）、排程資訊
- **`[×]`**：隱藏此便利貼（`note_visible = false`）
- 位置、大小持久化存回 JSON

### 3.4 工作排程（Task Scheduling）

#### 技術方案

- 採用 **Windows Task Scheduler COM API**（`taskschd.h`）
- 即使本程式未執行，任務也會由系統觸發

#### 動作類型（Action）

- **啟動程式（Start a program）**：目標程式完整路徑 + 啟動參數

#### 觸發條件（Trigger）

每張 Card 可設定**多個觸發時間**：

| 類型 | 說明 |
|------|------|
| **Daily** | 每天的指定時:分 |
| **Weekly** | 每週指定星期幾（可複選） + 時:分 |
| **Monthly by weekday** | 每月第 N 個星期幾 + 時:分 |
| **Once** | 指定日期 + 時:分，觸發後自動停用 |

#### 「僅提醒」模式

- Card 可不填程式路徑，觸發時僅彈出 Balloon Tip / Toast 通知

### 3.5 System Tray

- 程式常駐於 Windows 系統匣
- 右鍵選單：顯示主視窗 / 顯示所有便利貼 / 結束程式

---

## 四、資料格式（JSON Schema）

```json
{
  "lists": [
    {
      "id": "uuid",
      "title": "Todo",
      "position": 0
    }
  ],
  "cards": [
    {
      "id": "uuid",
      "list_id": "uuid",
      "position": 0,
      "title": "Card 標題",
      "description": "多行描述",
      "checkboxes": [
        { "label": "子項目", "checked": false }
      ],
      "color": "#4A90D9",
      "tags": ["work", "urgent"],
      "note_pos": { "x": 100, "y": 200 },
      "note_size": { "w": 300, "h": 220 },
      "note_visible": false,
      "schedule": {
        "enabled": false,
        "action": {
          "program": "C:\\Windows\\System32\\notepad.exe",
          "arguments": ""
        },
        "triggers": [
          {
            "type": "weekly",
            "days_of_week": [1, 3],
            "hour": 9,
            "minute": 0
          }
        ]
      }
    }
  ]
}
```

---

## 五、開發風格準則（Development Style Guide）

1. **檔案組織**：`include/` 放標頭檔，`src/` 放實作，子資料夾按 MVC 分層
2. **類別命名**：PascalCase（`CardModel`、`BoardController`）
3. **私有成員**：尾端加 `_`（`cards_`、`lists_`）
4. **私有方法**：尾端加 `_`（`loadJson_()`、`createTask_()` ）
5. **公開方法**：camelCase，`[[nodiscard]]` 標記重要回傳值
6. **錯誤處理**：`std::expected<T, std::string>` 用於可恢復錯誤
7. **Smart Pointer**：`std::shared_ptr` / `std::unique_ptr`，不用裸指標管理資源
8. **JSON 序列化**：每個 Model 自帶 `to_json()` 與 `from_json()` 靜態方法
9. **PersistenceService**：統一負責讀寫 `data/board.json`
10. **Clang-format**：LLVM base，160 char line limit，TabWidth 2
11. **CMake**：3.20+，依賴放 `3rdparty/`，編譯警告開啟（`-Wall -Wextra`）
12. **C++ Standard**：C++20

---

## 六、TODO List（實作順序）

### Phase 1 — 骨架 ✅
- [x] CMakeLists.txt 建立，整合 ImGui + nlohmann/json + ImGuiFileDialog
- [x] Win32 host 視窗建立（標準視窗）
- [x] Vulkan instance / device / swapchain 初始化
- [x] ImGui render loop（`imgui_impl_win32` + `imgui_impl_vulkan`）
- [x] System Tray icon + 右鍵選單
- [x] Application 拆分為 `include/Application.h` + `src/Application.cpp`

### Phase 2 — 資料模型 + 看板 UI
- [ ] `BoardList` Model + JSON 序列化
- [ ] `Card` Model（含 checkboxes、tags、color、schedule placeholder）+ JSON 序列化
- [ ] `PersistenceService` 讀寫 `board.json`
- [ ] 看板主視圖：多欄 List 橫向排列
- [ ] Card 在 List 內的渲染（色條 + 標題 + checkbox 進度 + 按鈕）
- [ ] 跨 List 拖曳 Card（`ImGui::SetNextWindowDragDropTarget` 等）
- [ ] `[+ Add Card]` / `[+ New List]` 快速新增
- [ ] 刪除 Card / List

### Phase 3 — 便利貼浮動視窗
- [ ] `hwndNotes_` 第二 Win32 視窗 + 獨立 Vulkan swapchain
- [ ] ImGui Multi-Viewport 啟用（`ImGuiConfigFlags_ViewportsEnable`）
- [ ] Card Detail 便利貼渲染（標題、描述、checkboxes、tags、顏色選擇）
- [ ] `[▼]` 下拉：顏色預設色 + 調色盤自訂
- [ ] 便利貼位置 / 大小持久化
- [ ] 最小化主視窗，便利貼維持可見

### Phase 4 — 標籤與 Filtering
- [ ] Tag 新增 / 刪除 / 編輯
- [ ] 看板工具列：依 tag 篩選顯示 Card
- [ ] Card 顏色標籤在看板外觀上顯示

### Phase 5 — 工作排程（COM API）
- [ ] `ScheduleTrigger` Model + JSON 序列化
- [ ] `ScheduleController`：COM API 建立 / 修改 / 刪除 Windows Task
- [ ] Daily / Weekly / Monthly Weekday / Once 觸發類型
- [ ] 多 trigger 支援
- [ ] 「僅提醒」模式（Balloon Tip 通知）
- [ ] Card Detail 的排程設定面板（使用 ImGuiFileDialog 選程式路徑）

### Phase 6 — 整合與收尾
- [ ] 程式啟動自動載入 JSON，還原看板與所有便利貼狀態
- [ ] 主視窗關閉按鈕 → 收到 Tray（不退出程式）
- [ ] 排程狀態顯示（啟用中 / 停用 / 僅提醒）
- [ ] 測試各觸發類型正常建立至 Windows 工作排程器
