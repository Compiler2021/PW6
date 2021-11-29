source_filename = "while_test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@b = global i32 0, align 4 ; a b 为全局变量，为全局标识符，需要有前缀 @
@a = global i32 0, align 4 ; i32 代表全局变量类型，align 4 代表对齐，初始化为 0 ，@b 与 @a 本质是两个指针

define i32 @main() #0 {
	%1 = alloca i32, align 4        ; 给局部变量 %1 在栈上分配空间 
	store i32 0, i32* %1, align 4   ; 将 %1 赋值为 0 
	store i32 0, i32* @b, align 4   ; 将全局变量 @b 赋值为 0
	store i32 3, i32* @a, align 4   ; 将全局变量 @a 赋值为 0
	br label %2                     ; 无条件跳转到 %2

2:									
	%3 = load i32, i32* @a, align 4 ; 取出全局变量 @a 的值到局部变量 %3
	%4 = icmp sgt i32 %3, 0         ; 比较 %3 的值是否大于 0
	br i1 %4, label %5, label %11   ; 如果 %3 > 0 那就跳转到 %5 否则跳转到 %11 执行返回语句

5:																								; preds = %2
	%6 = load i32, i32* @b, align 4 ; 取出全局变量 @b 的值到局部变量 %6
	%7 = load i32, i32* @a, align 4 ; 取出全局变量 @a 的值到局部变量 %7
	%8 = add i32 %6, %7             ; %8 = %6 + %7  
	store i32 %8, i32* @b, align 4  ; @b = %8
	%9 = load i32, i32* @a, align 4 ; %9 = @a
	%10 = sub i32 %9, 1             ; %10 = %9 - 1
	store i32 %10, i32* @a, align 4 ; @a = %10
	br label %2, !llvm.loop !6      ; 无条件跳转到 %2

11:																							 ; preds = %2
	%12 = load i32, i32* @b, align 4 ; %12 = @b
	ret i32 %12                      ; 返回 %12
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
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
