; ModuleID = 'go_upstairs.c'
source_filename = "go_upstairs.c"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.29.30137"

@num = dso_local global [2 x i32] [i32 4, i32 8], align 4
@tmp = dso_local global i32 1, align 4
@n = dso_local global i32 0, align 4
@x = dso_local global [1 x i32] zeroinitializer, align 4

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @climbStairs(i32 %0) #0 {
  %2 = alloca i32, align 4                        ; %2 is ret data
  %3 = alloca i32, align 4
  %4 = alloca [10 x i32], align 16                ; alloc dp[10]
  %5 = alloca i32, align 4                        ; int i
  store i32 %0, i32* %3, align 4                  ; store n to %3
  %6 = load i32, i32* %3, align 4                 ; load %3 = n
  %7 = icmp slt i32 %6, 4                         ; if n < 4
  br i1 %7, label %8, label %10                   ; true to %8

8:                                                ; preds = %1
  %9 = load i32, i32* %3, align 4
  store i32 %9, i32* %2, align 4
  br label %41

10:                                               ; preds = %1
  %11 = getelementptr inbounds [10 x i32], [10 x i32]* %4, i64 0, i64 0
  store i32 0, i32* %11, align 16
  %12 = getelementptr inbounds [10 x i32], [10 x i32]* %4, i64 0, i64 1
  store i32 1, i32* %12, align 4
  %13 = getelementptr inbounds [10 x i32], [10 x i32]* %4, i64 0, i64 2
  store i32 2, i32* %13, align 8
  store i32 3, i32* %5, align 4                   ; assign part
  br label %14                                      

14:                                               ; preds = %19, %10
  %15 = load i32, i32* %5, align 4                ; %15 = i
  %16 = load i32, i32* %3, align 4                ; %16 = n
  %17 = add nsw i32 %16, 1                          
  %18 = icmp slt i32 %15, %17                     ; bool for while
  br i1 %18, label %19, label %36

19:                                               ; preds = %14
  %20 = load i32, i32* %5, align 4
  %21 = sub nsw i32 %20, 1
  %22 = sext i32 %21 to i64
  %23 = getelementptr inbounds [10 x i32], [10 x i32]* %4, i64 0, i64 %22
  %24 = load i32, i32* %23, align 4
  %25 = load i32, i32* %5, align 4
  %26 = sub nsw i32 %25, 2
  %27 = sext i32 %26 to i64
  %28 = getelementptr inbounds [10 x i32], [10 x i32]* %4, i64 0, i64 %27
  %29 = load i32, i32* %28, align 4
  %30 = add nsw i32 %24, %29
  %31 = load i32, i32* %5, align 4
  %32 = sext i32 %31 to i64
  %33 = getelementptr inbounds [10 x i32], [10 x i32]* %4, i64 0, i64 %32
  store i32 %30, i32* %33, align 4
  %34 = load i32, i32* %5, align 4
  %35 = add nsw i32 %34, 1
  store i32 %35, i32* %5, align 4
  br label %14, !llvm.loop !4

36:                                               ; preds = %14
  %37 = load i32, i32* %3, align 4
  %38 = sext i32 %37 to i64
  %39 = getelementptr inbounds [10 x i32], [10 x i32]* %4, i64 0, i64 %38
  %40 = load i32, i32* %39, align 4
  store i32 %40, i32* %2, align 4
  br label %41

41:                                               ; preds = %36, %8
  %42 = load i32, i32* %2, align 4
  ret i32 %42                                     ; return data
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4             ; res = 0
  %3 = load i32, i32* getelementptr inbounds ([2 x i32], [2 x i32]* @num, i64 0, i64 0), align 4
  store i32 %3, i32* @n, align 4            ; n = num[0]
  %4 = load i32, i32* @tmp, align 4
  %5 = sext i32 %4 to i64
  %6 = getelementptr inbounds [2 x i32], [2 x i32]* @num, i64 0, i64 %5
  %7 = load i32, i32* %6, align 4
  store i32 %7, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @x, i64 0, i64 0), align 4
  %8 = load i32, i32* @n, align 4
  %9 = load i32, i32* @tmp, align 4
  %10 = add nsw i32 %8, %9
  %11 = call i32 @climbStairs(i32 %10)
  store i32 %11, i32* %2, align 4
  %12 = load i32, i32* %2, align 4
  %13 = load i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @x, i64 0, i64 0), align 4
  %14 = sub nsw i32 %12, %13
  ret i32 %14
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 2}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"uwtable", i32 1}
!3 = !{!"clang version 13.0.0"}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.mustprogress"}
