; ModuleID = 'func_hand.sy'
source_filename = "func_hand.sy"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @add(i32 %0, i32 %1) #0 {
    %3 = alloca i32, align 4            ; alloc place to store a
    %4 = alloca i32, align 4            ; alloc place to store b
    store i32 %0, i32* %3, align 4
    store i32 %1, i32* %4, align 4      ; store procedure
    %5 = load i32, i32* %3, align 4
    %6 = load i32, i32* %4, align 4     ; load so that we get a i32 rather than a i32*
    %7 = add i32 %5, %6                 ; add: a+b
    %8 = sub i32 %7, 1                  ; sub: (a+b) - 1
    ret i32 %8                          ; function return
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
    %1 = alloca i32, align 4            ; alloc place for a
    %2 = alloca i32, align 4            ; alloc place for b
    %3 = alloca i32, align 4            ; alloc place for c
    store i32 3, i32* %1, align 4
    store i32 2, i32* %2, align 4
    store i32 5, i32* %3, align 4       ; store procedure
    %4 = load i32, i32* %1, align 4
    %5 = load i32, i32* %2, align 4     ; get a and b
    %6 = call i32 @add(i32 %4, i32 %5)  ; funcall ret add(a, b)
    %7 = alloca i32, align 4            
    store i32 %6, i32* %7, align 4      ; store the data returned
    %8 = load i32, i32* %3, align 4
    %9 = load i32, i32* %7, align 4      ; load add(a, b) and c
    %10 = add i32 %8, %9                ; add add(a, b), c
    ret i32 %10                         ; return result
}

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.0-4ubuntu1~18.04.2 "}
