source_filename = "if_test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@a = global i32 0, align 4          ; 全局变量 @a ，初始化为 0 因为是全局标识符，所以 @ 开头

define i32 @main() #0 {
	%1 = alloca i32, align 4        ; 给局部变量 %1 分配空间
	store i32 0, i32* %1, align 4   ; 把 %1 赋值为 0, %1 是用作返回值
	store i32 10, i32* @a, align 4  ; 把全局变量 @a 赋值为 10
	%2 = load i32, i32* @a, align 4 ; 将 @a 的值取出赋值给 %2
	%3 = icmp sgt i32 %2, 0         ; 比较 %2 是否大于 0
	br i1 %3, label %4, label %6    ; 如果 %2 大于0 跳转到 %4 否则跳转到 %6

4:																								; preds = %0
	%5 = load i32, i32* @a, align 4 ; 将 @a 的值取出赋值给 %5
	store i32 %5, i32* %1, align 4  ; 把 %5 的值存入 %1
	br label %7                     ; 无条件跳转到 %7，执行返回语句

6:																								; preds = %0
	store i32 0, i32* %1, align 4   ; 把 %1 的值设置为 0
	br label %7                     ; 无条件跳转到 %7，执行返回语句

7:																								; preds = %6, %4
	%8 = load i32, i32* %1, align 4 ; %8 = %1
	ret i32 %8                      ; 返回 %8
}

attributes #0 = { noinline nounwind optnone sspstrong uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 13.0.0"}
