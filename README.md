一. 分數分配：
   (1) 基本題(60%)
   (2) Error處理(30%)
   (3) document(10%)

二. 扣分：
   (1)Compiler error,只有執行檔,直接寫死印出答案,以上三種皆為零分計.
   (2)無法讀檔(僅能手動輸入),則扣30分.
   (3)無法寫檔(僅顯示在螢幕),則扣30分.
   (4)輸出格式不符(只有印出機器碼,沒有source code)扣30分
   (5)輸入檔名稱寫死,導致跑不出結果,0分.
   (6)只有token,0分.
   (7)版本(提供者也算),0分
   (8)沒有寫Error處理(起碼要有Syntax Error),0分
   (9)程式部分0分,則該次程式分數皆為0分,不額外給予document分數.

三. 基本要求(必須做到否則不用機測)：
   如果只有寫到token的同學甚至更差的，助教沒辦法給你任何分數
   要能作出 Lexical analysis 及 Syntax analysis 並產生中間碼
　 Franscis Language 的 11 個 Statement 都必須實作，缺一個未做都視為程式未完成，以零分計。
   
  FRANCIS Language 的 11 個 statement：

  1.PROGRAM（程式開頭）
  
  2.GTO（無條件跳躍）
  
  3.SUBROUTINE（副程式定義）
  
  4.assignment（指派，如 X = expr;）
  
  5.VARIABLE（變數宣告）
  
  6.CALL（呼叫副程式）
  
  7.LABEL（標號宣告）
  
  8.INPUT（輸入）
  
  9.DIMENSION（陣列宣告）
  
  10.OUTPUT（輸出）
  
  11.IF（條件式 IF ... THEN ... ELSE ...;）

五. Error處理：
   必須要能輸出Error的行數,並翻出Error前後的code.
   例如第6行出現Error,那你必須輸出說line6有一個Error.
   並判斷為何種錯誤種類,output檔中除了要指名Error在第六行外,
   必須要印出除了第六行以外的中間碼結果,
   因為Error而程式當掉或是跳過Error當作沒看到的一律 0  分.

六. output格式：
   每一行除了中間碼外,[同時要將輸入的code輸出]
   EX:((5,24), , , ) [input_code]
   輸出格式不對也會予以扣分

七. document：
   基本分5分,最多10分.
   內容需包含：開發平台
   使用開發環境
   說明你的程式設計(功能，流程，使用的data structure)
   未完成的功能(請詳細列出，若測出不符合所列的將會扣分)

   為追求效率，用純文字檔(.txt)即可，不需使用doc檔作美化，排版等...
   上述的document內容缺一不可，少其中一項者doc的分數0分

