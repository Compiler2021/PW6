; ModuleID = 'assign_test.c'
source_filename = "assign_test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@__const.main.a = private unnamed_addr constant [2 x i32] [i32 2, i32 0], align 4

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4                      ;维护一个始终值为0的寄存器
  %2 = alloca float, align 4                    ;给b分配栈空间
  %3 = alloca [2 x i32], align 4                ;给a分配空间
  store i32 0, i32* %1, align 4
  store float 0x3FFCCCCCC0000000, float* %2, align 4    ;b = 1.8
  %4 = getelementptr inbounds [2 x i32], [2 x i32]* %3, i64 0, i64 0    ;获取a[0]地址
  store i32 2, i32* %4, align 4                                         ;a[0] = 2
  %5 = load i32, i32* %4, align 4                                       ;取a[0]
  %6 = sitofp i32 %5 to float                                           ;将a[0]转换为float类型
  %7 = load float, float* %2, align 4                                   ;取b
  %8 = fmul float %6, %7                                                ;计算b*a[0]
  %9 = fptosi float %8 to i32                                           ;将结果转换为int
  %10 = getelementptr inbounds [2 x i32], [2 x i32]* %3, i64 0, i64 1   ;获取a[1]地址
  store i32 %9, i32* %10, align 4                                       ;将结果存入a[1]
  ret i32 %9                                                            ;返回a[1]
}

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind willreturn }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.1 "}
