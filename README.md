Quadruple-IR-Compiler - 中間碼 (四元式) 編譯器實作大綱
=================================================

一、整體架構與里程碑
1) Lexer（詞彙分析） → 產生 (表號, 索引) 的 token
2) Parser（語法分析） → 依語法把敘述分解、運算式轉 RPN
3) IRBuilder（四元式生成） → 由 RPN 建暫存 T1/T2…，輸出四元式
4) 標籤/跳轉回填（Backpatch） → 解決 GTO/IF 等目標列號
5) 輸出器 → 以行號列印四元式，右側附「人可讀」註解（如 I = 1）

二、資料表與編碼
- Table 1 分隔符：; ( ) = + - * / ^ ' , : → (1,j)
- Table 2 保留字：AND/BOOLEAN/CALL/DIMENSION/ELSE/ENP/ENS/EQ/GE/GT/GTO/IF/INPUT/INTEGER/LABEL/LE/LT/NE/OR/OUTPUT/PROGRAM/REAL/SUBROUTINE/THEN/VARIABLE → (2,j)
- Table 3 整數常數 → (3,j)
- Table 4 實數常數 → (4,j)
- Table 5 識別字（變數、陣列、標籤、副程式名）→ (5,j)
- Table 6 四元式目標列（跳轉目的地）→ (6,j)
- Table 7 資訊表（陣列維度、CALL 參數列表等）→ (7,j)
- 暫存變數 → (0,k) 表示 T1, T2, …

三、Lexer（詞彙分析）
1) 跳過空白，若遇分隔符 → (1,j)
2) 數字 → 整數或實數 → (3,j)/(4,j)
3) 字母開頭 → 保留字 (2,j) 或 識別字 (5,j)

四、Parser（語法分析）
- 宣告：登記符號表 (Table 5)，必要時連動 Table 7
- 指派：右側轉 RPN，再交 IRBuilder
- 陣列讀/寫：依型別決定 load/store 四元式
- IF/GTO/標籤：產生跳轉四元式，必要時延遲回填
- CALL：建 CALL 四元式，最後一欄指向 Table 7 參數清單

五、運算式處理（RPN → 四元式）
1) Shunting-yard 轉換運算式成 RPN
2) RPN 逐掃：遇運算子 → 彈出兩個運算元 → 產生一條四元式 → 推回暫存
3) 最後結果四元式形如 (=, result, -, X)

六、跳轉與回填
- GTO/IF → 若目標標籤未出現，先記錄待回填清單
- 遇標籤時 → 回填所有未決跳轉的目的地

七、輸出格式
- 每行：行號    ((op),(a),(b),(r))    人可讀註解
- 各欄位以 (表號, 索引) 表示，例如 ((1,4),(3,2), ,(5,2)) 代表 I=1
- 空欄位留空，暫存用 (0,k)，陣列元素加上索引註解

八、錯誤處理
- 詞法：非法字元、數字格式錯誤
- 語法：括號不匹配、缺分號
- 語意：未宣告變數、型別不合
- 標籤：跳轉至不存在的標籤

九、測試順序
1) 指派 + 四則運算
2) 常數賦值
3) 陣列存取
4) GTO/標籤
5) CALL（可選）

十、可擴充方向
- 布林/比較運算 (EQ, NE, GT, GE, LT, LE)
- 副程式區塊 (PROGRAM vs SUBROUTINE)

十一、測試方式
- 以 g++ 編譯 `compiler.cpp`，並以提供的 `input.txt` 作為輸入
- 執行 `./run_tests.sh` 會自動產生四元式並與 `answer.txt` 比對
