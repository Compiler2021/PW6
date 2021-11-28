int num[2] = {4, 8};
int x[1];
int n;
int tmp = 1;

int climbStairs(int n) {
    if(n < 4)
        return n;
    int dp[10];
    dp[0] = 0;
    dp[1] = 1;
    dp[2] = 2;
    int i;
    i = 3;
    while(i<n+1){
        dp[i] = dp[i-1] + dp[i-2];  // dp[3] = 3, dp[4] = 5, dp[5] = 8
        i = i + 1;
    }
    return dp[n];
}

int main(){
    int res;
    n=num[0];           // n = 4
    x[0] = num[tmp];    // x[0] = 8
    res = climbStairs(n + tmp); // n + tmp = 5
    return res - x[0];  // 8 - 8 = 0
}